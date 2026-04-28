#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ws.h"

static const char *TAG = "ws";

static httpd_handle_t s_server = NULL;
static int s_active_fd = -1;
static ws_msg_cb_t  s_on_msg;
static ws_conn_cb_t s_on_conn;
static volatile uint32_t s_last_msg_sec = 0;

static uint32_t now_sec(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
}

static esp_err_t reject_extra_client(httpd_req_t *req) {
    static const char err[] =
        "{\"v\":1,\"type\":\"error\",\"reason\":\"already-connected\"}\n";
    httpd_ws_frame_t f = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)err,
        .len = sizeof(err) - 1
    };
    httpd_ws_send_frame(req, &f);
    return ESP_FAIL;
}

static esp_err_t handler(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);
    if (req->method == HTTP_GET) {
        if (s_active_fd != -1 && s_active_fd != fd) {
            ESP_LOGW(TAG, "rejecting second client (fd=%d, active=%d)", fd, s_active_fd);
            return reject_extra_client(req);
        }
        s_active_fd = fd;
        s_last_msg_sec = now_sec();
        if (s_on_conn) s_on_conn(true);
        return ESP_OK;
    }

    httpd_ws_frame_t f = {0};
    esp_err_t r = httpd_ws_recv_frame(req, &f, 0);
    if (r != ESP_OK) return r;
    if (f.type != HTTPD_WS_TYPE_TEXT || f.len == 0) return ESP_OK;

    f.payload = (uint8_t *)malloc(f.len + 1);
    if (!f.payload) return ESP_ERR_NO_MEM;
    r = httpd_ws_recv_frame(req, &f, f.len);
    if (r == ESP_OK) {
        f.payload[f.len] = 0;
        char *line = (char *)f.payload;
        char *p = line;
        while (*p) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = 0;
            if (*p && s_on_msg) {
                s_last_msg_sec = now_sec();
                s_on_msg(p);
            }
            if (!nl) break;
            p = nl + 1;
        }
    }
    free(f.payload);
    return r;
}

void ws_init(ws_msg_cb_t on_msg, ws_conn_cb_t on_conn) {
    s_on_msg = on_msg;
    s_on_conn = on_conn;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    ESP_ERROR_CHECK(httpd_start(&s_server, &cfg));

    httpd_uri_t u = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handler,
        .is_websocket = true,
        .handle_ws_control_frames = true
    };
    httpd_register_uri_handler(s_server, &u);
    ESP_LOGI(TAG, "WebSocket server listening on :80");
}

void ws_send_line(const char *line) {
    if (s_server == NULL || s_active_fd < 0) return;
    size_t n = strlen(line);
    char *buf = (char *)malloc(n + 2);
    if (!buf) return;
    memcpy(buf, line, n);
    buf[n] = '\n';
    buf[n + 1] = 0;
    httpd_ws_frame_t f = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)buf,
        .len = n + 1
    };
    esp_err_t err = httpd_ws_send_frame_async(s_server, s_active_fd, &f);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ws send failed (%d) — peer probably gone", err);
        s_active_fd = -1;
        if (s_on_conn) s_on_conn(false);
    }
    free(buf);
}

bool ws_is_connected(void) { return s_active_fd >= 0; }

int ws_seconds_since_last_message(void) {
    if (!s_last_msg_sec) return 0;
    return (int)(now_sec() - s_last_msg_sec);
}
