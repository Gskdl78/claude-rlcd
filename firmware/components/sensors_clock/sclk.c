#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "sclk.h"

static const char *TAG = "sclk";

/* Waveshare ESP32-S3-RLCD-4.2 main I2C bus: GPIO13 SDA, GPIO14 SCL.
 * Shared by PCF85063 (RTC), ES8311 (audio codec), QMI8658C (IMU), and the
 * touch panel. Note: this board has NO SHTC3 — env reads will always fail
 * and the 3-fail stale fallback will fire (env shown as "--"). */
#define I2C_NUM I2C_NUM_0
#define I2C_SDA 13
#define I2C_SCL 14
#define I2C_FREQ 400000
#define SHTC3_ADDR     0x70
#define PCF85063_ADDR  0x51

static env_cb_t  s_env_cb;
static time_cb_t s_time_cb;
static volatile bool s_sntp_done = false;

static esp_err_t i2c_setup(void) {
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ
    };
    esp_err_t err = i2c_param_config(I2C_NUM, &cfg);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
}

static esp_err_t shtc3_read(float *t, float *h) {
    uint8_t cmd[2] = { 0x7C, 0xA2 };
    uint8_t buf[6] = {0};
    if (i2c_master_write_to_device(I2C_NUM, SHTC3_ADDR, cmd, 2, pdMS_TO_TICKS(50)) != ESP_OK)
        return ESP_FAIL;
    vTaskDelay(pdMS_TO_TICKS(15));
    if (i2c_master_read_from_device(I2C_NUM, SHTC3_ADDR, buf, 6, pdMS_TO_TICKS(50)) != ESP_OK)
        return ESP_FAIL;
    uint16_t rt = (buf[0] << 8) | buf[1];
    uint16_t rh = (buf[3] << 8) | buf[4];
    *t = -45.0f + 175.0f * (rt / 65535.0f);
    *h = 100.0f * (rh / 65535.0f);
    return ESP_OK;
}

static uint8_t bcd_to_bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin_to_bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

static esp_err_t pcf_read_time(struct tm *out) {
    uint8_t reg = 0x04;
    uint8_t buf[7] = {0};
    if (i2c_master_write_read_device(I2C_NUM, PCF85063_ADDR, &reg, 1, buf, 7,
                                     pdMS_TO_TICKS(50)) != ESP_OK)
        return ESP_FAIL;
    out->tm_sec  = bcd_to_bin(buf[0] & 0x7F);
    out->tm_min  = bcd_to_bin(buf[1] & 0x7F);
    out->tm_hour = bcd_to_bin(buf[2] & 0x3F);
    out->tm_mday = bcd_to_bin(buf[3] & 0x3F);
    out->tm_wday = buf[4] & 0x07;
    out->tm_mon  = bcd_to_bin(buf[5] & 0x1F) - 1;
    out->tm_year = bcd_to_bin(buf[6]) + 100;
    return ESP_OK;
}

static esp_err_t pcf_write_time(const struct tm *in) {
    uint8_t buf[8] = {
        0x04,
        bin_to_bcd(in->tm_sec)  & 0x7F,
        bin_to_bcd(in->tm_min)  & 0x7F,
        bin_to_bcd(in->tm_hour) & 0x3F,
        bin_to_bcd(in->tm_mday) & 0x3F,
        in->tm_wday & 0x07,
        bin_to_bcd(in->tm_mon + 1) & 0x1F,
        bin_to_bcd(in->tm_year - 100)
    };
    return i2c_master_write_to_device(I2C_NUM, PCF85063_ADDR, buf, sizeof(buf),
                                      pdMS_TO_TICKS(50));
}

static void env_task(void *_) {
    int fail_streak = 0;
    while (1) {
        float t = 0, h = 0;
        if (shtc3_read(&t, &h) == ESP_OK) {
            fail_streak = 0;
            if (s_env_cb) s_env_cb(t, h, false);
        } else {
            fail_streak++;
            if (fail_streak >= 3 && s_env_cb) s_env_cb(NAN, NAN, true);
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

static void on_sntp_sync(struct timeval *tv) {
    s_sntp_done = true;
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    pcf_write_time(&tm);
    if (s_time_cb) s_time_cb((uint32_t)now);
    ESP_LOGI(TAG, "SNTP sync: %u", (unsigned)now);
}

void sclk_on_wifi_up(void) {
    static bool s_sntp_started = false;
    if (s_sntp_started) return;     /* idempotent: net may emit CONNECTED more than once */
    s_sntp_started = true;
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(on_sntp_sync);
    esp_sntp_init();
}

void sclk_apply_time_sync(uint32_t utc_sec, const char *tz) {
    if (s_sntp_done) return;
    struct timeval tv = { .tv_sec = (time_t)utc_sec, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    if (tz && *tz) { setenv("TZ", tz, 1); tzset(); }
    time_t now = (time_t)utc_sec;
    struct tm tm;
    gmtime_r(&now, &tm);
    pcf_write_time(&tm);
    if (s_time_cb) s_time_cb(utc_sec);
}

void sclk_init(env_cb_t env_cb, time_cb_t time_cb) {
    s_env_cb  = env_cb;
    s_time_cb = time_cb;

    /* Default TZ = Asia/Taipei (UTC+8). The bridge can override via
     * time-sync's tz field; SNTP gives UTC only, so without this header
     * clock would render in UTC. POSIX TZ format: name + offset_west,
     * so UTC+8 is "CST-8". */
    setenv("TZ", "CST-8", 1);
    tzset();

    if (i2c_setup() != ESP_OK) {
        ESP_LOGW(TAG, "i2c setup failed; sensors disabled");
        return;
    }

    struct tm tm = {0};
    if (pcf_read_time(&tm) == ESP_OK && tm.tm_year > 100) {
        time_t t = mktime(&tm);
        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        if (s_time_cb) s_time_cb((uint32_t)t);
        ESP_LOGI(TAG, "RTC restored: %lld", (long long)t);
    }

    xTaskCreate(env_task, "env", 4096, NULL, 5, NULL);
}
