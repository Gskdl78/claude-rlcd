#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "net.h"

static const char *TAG = "net";
static const char *NVS_NS = "claude-rlcd";

static net_info_t g_info;
static net_status_cb_t g_cb;
static EventGroupHandle_t s_evg;

#define EVT_STA_CONNECTED  BIT0
#define EVT_STA_FAIL       BIT1

extern const uint8_t captive_html_start[] asm("_binary_captive_html_start");
extern const uint8_t captive_html_end[]   asm("_binary_captive_html_end");

static int s_attempt = 0;
static const int s_max_attempts = 30;
static const int s_backoff_ms[] = { 1000, 2000, 5000, 10000, 30000 };

static void emit(void) { if (g_cb) g_cb(&g_info); }

void net_get_info(net_info_t *out) { *out = g_info; }

static esp_err_t nvs_read_str(const char *key, char *buf, size_t buf_sz) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return ESP_ERR_NOT_FOUND;
    size_t sz = buf_sz;
    esp_err_t err = nvs_get_str(h, key, buf, &sz);
    nvs_close(h);
    return err;
}

static esp_err_t nvs_write_str(const char *key, const char *value) {
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &h));
    esp_err_t err = nvs_set_str(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_attempt++;
        g_info.attempt = s_attempt;
        emit();
        if (s_attempt >= s_max_attempts) {
            xEventGroupSetBits(s_evg, EVT_STA_FAIL);
            return;
        }
        int idx = s_attempt - 1;
        if (idx >= (int)(sizeof(s_backoff_ms) / sizeof(s_backoff_ms[0])))
            idx = (int)(sizeof(s_backoff_ms) / sizeof(s_backoff_ms[0])) - 1;
        vTaskDelay(pdMS_TO_TICKS(s_backoff_ms[idx]));
        esp_wifi_connect();
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(g_info.ip, sizeof(g_info.ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_attempt = 0;
        xEventGroupSetBits(s_evg, EVT_STA_CONNECTED);
    }
}

static void mdns_advertise_unique(void) {
    ESP_ERROR_CHECK(mdns_init());
    char name[32] = "claude-rlcd";
    for (int i = 1; i <= 5; i++) {
        if (mdns_hostname_set(name) == ESP_OK) break;
        snprintf(name, sizeof(name), "claude-rlcd-%d", i + 1);
    }
    mdns_instance_name_set("claude-rlcd display");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    strncpy(g_info.mdns_name, name, sizeof(g_info.mdns_name) - 1);
    ESP_LOGI(TAG, "mDNS hostname=%s.local", name);
}

static esp_err_t handle_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, (const char *)captive_html_start,
                                 captive_html_end - captive_html_start);
}

static esp_err_t handle_save(httpd_req_t *req) {
    char buf[256] = {0};
    int total = req->content_len < (int)sizeof(buf) - 1 ? req->content_len : (int)sizeof(buf) - 1;
    int n = httpd_req_recv(req, buf, total);
    if (n <= 0) return ESP_FAIL;
    buf[n] = 0;

    char ssid[33] = {0}, psk[65] = {0};
    httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(buf, "psk",  psk,  sizeof(psk));
    if (!ssid[0]) {
        httpd_resp_send(req, "missing ssid", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    nvs_write_str("ssid", ssid);
    nvs_write_str("psk",  psk);
    httpd_resp_send(req, "OK, rebooting...", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

static httpd_handle_t start_portal(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t srv = NULL;
    ESP_ERROR_CHECK(httpd_start(&srv, &cfg));
    httpd_uri_t u_root = { .uri = "/",     .method = HTTP_GET,  .handler = handle_root };
    httpd_uri_t u_save = { .uri = "/save", .method = HTTP_POST, .handler = handle_save };
    httpd_register_uri_handler(srv, &u_root);
    httpd_register_uri_handler(srv, &u_save);
    return srv;
}

static void start_softap(void) {
    g_info.status = NET_SOFTAP;
    strcpy(g_info.ssid, "Claude-RLCD-Setup");
    emit();

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "Claude-RLCD-Setup",
            .ssid_len = (uint8_t)strlen("Claude-RLCD-Setup"),
            .channel = 6,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    start_portal();
}

static bool start_sta(const char *ssid, const char *psk) {
    g_info.status = NET_CONNECTING;
    strncpy(g_info.ssid, ssid, sizeof(g_info.ssid) - 1);
    g_info.attempt = 0;
    g_info.max_attempts = s_max_attempts;
    emit();

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid,     ssid, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)sta_cfg.sta.password, psk,  sizeof(sta_cfg.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    EventBits_t bits = xEventGroupWaitBits(s_evg,
        EVT_STA_CONNECTED | EVT_STA_FAIL, pdTRUE, pdFALSE, portMAX_DELAY);
    if (bits & EVT_STA_CONNECTED) {
        g_info.status = NET_CONNECTED;

        uint8_t mac[6] = {0};
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        snprintf(g_info.mac, sizeof(g_info.mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        emit();
        mdns_advertise_unique();
        emit();
        return true;
    }
    return false;
}

void net_init(net_status_cb_t cb) {
    g_cb = cb;
    s_evg = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wic = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wic));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    char ssid[33] = {0}, psk[65] = {0};
    if (nvs_read_str("ssid", ssid, sizeof(ssid)) != ESP_OK || ssid[0] == 0) {
        start_softap();
        return;
    }
    nvs_read_str("psk", psk, sizeof(psk));
    if (!start_sta(ssid, psk)) {
        ESP_LOGW(TAG, "STA failed after %d attempts; falling back to SOFTAP", s_max_attempts);
        nvs_write_str("ssid", "");
        start_softap();
    }
}
