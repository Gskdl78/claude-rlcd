#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "net.h"
#include "ws.h"
#include "ui.h"
#include "state_store.h"
#include "sclk.h"
#include "audio.h"

void display_init(void);   // implemented in display_glue.cpp

static const char *TAG = "app_main";

static state_t g_state;
static net_info_t g_net;

static const char *jget_str_v(const char *json, const char *key, char *out, size_t n) {
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    const char *p = strstr(json, needle);
    if (!p) { if (out && n) out[0] = 0; return NULL; }
    p += strlen(needle);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < n) out[i++] = *p++;
    if (out && n) out[i] = 0;
    return out;
}

static void on_ws_msg(const char *line) {
    char type[24] = {0};
    if (!jget_str_v(line, "type", type, sizeof(type))) return;
    g_state.last_msg_time = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);

    if      (!strcmp(type, "state"))          state_apply_state_msg(&g_state, line);
    else if (!strcmp(type, "session-update")) state_apply_session_update(&g_state, line);
    else if (!strcmp(type, "session-end")) {
        char id[STATE_ID_MAX] = {0};
        jget_str_v(line, "id", id, sizeof(id));
        state_apply_session_end(&g_state, id);
    }
    else if (!strcmp(type, "quota"))          state_apply_quota_msg(&g_state, line);
    else if (!strcmp(type, "alert")) {
        bool was_active = g_state.alert.active;
        state_apply_alert(&g_state, line);
        if (!was_active && g_state.alert.active) audio_beep();
    }
    else if (!strcmp(type, "alert-clear")) {
        char id[STATE_ID_MAX] = {0};
        jget_str_v(line, "id", id, sizeof(id));
        state_apply_alert_clear(&g_state, id);
    }
    else if (!strcmp(type, "time-sync")) {
        const char *p = strstr(line, "\"utc_sec\":");
        if (p) {
            uint32_t utc = (uint32_t)strtoul(p + 10, NULL, 10);
            char tz[40] = {0};
            jget_str_v(line, "tz", tz, sizeof(tz));
            sclk_apply_time_sync(utc, tz);
        }
    }
    else if (!strcmp(type, "ping")) {
        const char *p = strstr(line, "\"id\":");
        unsigned long pid = p ? strtoul(p + 5, NULL, 10) : 0;
        char buf[48];
        snprintf(buf, sizeof(buf), "{\"v\":1,\"type\":\"pong\",\"id\":%lu}", pid);
        ws_send_line(buf);
    }
    else if (!strcmp(type, "hello")) {
        ws_send_line("{\"v\":1,\"type\":\"hello-ack\"}");
    }

    ui_set_state(&g_state);
}

static void on_ws_conn(bool connected) {
    g_state.ws_connected = connected;
    /* On connect: switch to MAIN. On disconnect: stay where we are -- the
     * stale pill (driven by diag_task via ws_seconds_since_last_message)
     * communicates "data getting old" without the disorienting flash from
     * MAIN -> LISTENING -> MAIN every time the bridge reconnects. */
    if (connected) ui_show(UI_MAIN);
}

static void on_net(const net_info_t *info) {
    g_net = *info;
    switch (info->status) {
        case NET_SOFTAP:
            ui_set_softap_info(info->ssid, "http://192.168.4.1");
            ui_show(UI_SOFTAP);
            break;
        case NET_CONNECTING:
            ui_set_connecting_info(info->ssid, info->attempt,
                                   info->max_attempts, info->elapsed_sec);
            ui_show(UI_CONNECTING);
            break;
        case NET_CONNECTED:
            ui_set_listening_info(info->mdns_name, info->ip);
            ui_show(UI_LISTENING);
            sclk_on_wifi_up();
            break;
        default:
            break;
    }
}

static void on_env(float t, float h, bool stale) {
    ui_set_env(stale ? (float)NAN : t, stale ? (float)NAN : h);
    if (!stale && ws_is_connected()) {
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "{\"v\":1,\"type\":\"env\",\"tempC\":%.2f,\"humidity\":%.2f}", t, h);
        ws_send_line(buf);
    }
}

static void on_time(uint32_t utc_sec) {
    ui_set_clock(utc_sec);
}

static void diag_task(void *_) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        int stale = ws_seconds_since_last_message();
        if (stale > 10) ui_set_stale(stale); else ui_set_stale(0);

        if (!ws_is_connected()) continue;
        wifi_ap_record_t ap = {0};
        int8_t rssi = 0;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "{\"v\":1,\"type\":\"diag\",\"free_heap\":%u,\"rssi\":%d}",
                 (unsigned)esp_get_free_heap_size(), (int)rssi);
        ws_send_line(buf);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "claude-rlcd-display booting");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    display_init();
    ui_init();
    ui_show(UI_BOOT);

    state_init(&g_state);
    audio_init();

    net_init(on_net);

    /* SOFTAP mode runs the captive portal on port 80 — do not start the
     * WS server until we're actually connected as STA, otherwise httpd_start
     * collides on port 80 (errno 112). After STA connects, net_init returns
     * with status == NET_CONNECTED. */
    net_info_t cur;
    net_get_info(&cur);
    if (cur.status == NET_CONNECTED) {
        ws_init(on_ws_msg, on_ws_conn);
    } else {
        ESP_LOGI(TAG, "WS server deferred (net status=%d, captive portal active)",
                 (int)cur.status);
    }

    sclk_init(on_env, on_time);

    xTaskCreate(diag_task, "diag", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "boot complete; free heap=%u",
             (unsigned)esp_get_free_heap_size());
}
