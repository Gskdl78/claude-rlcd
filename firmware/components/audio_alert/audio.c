#include <string.h>
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio.h"
#include "beep_pcm.h"

static const char *TAG = "audio";

#define ES8311_ADDR 0x18
#define I2C_NUM     I2C_NUM_0     /* shared with sensors_clock */

/* I2S pins (Waveshare ESP32-S3-RLCD-4.2):
 *   GPIO16 MCLK, GPIO9 BCLK, GPIO45 LRCK/WS, GPIO8 DSDIN(=DOUT to codec)
 *   GPIO46 PA CTRL — drive HIGH to enable the analog amplifier. Without
 *   this the codec is configured but no audible output. */
#define MCLK_GPIO 16
#define BCLK_GPIO 9
#define WS_GPIO   45
#define DOUT_GPIO 8
#define PA_CTRL_GPIO 46

static i2s_chan_handle_t s_tx;
static bool s_ready = false;

static esp_err_t es8311_w(uint8_t reg, uint8_t val) {
    uint8_t b[2] = { reg, val };
    return i2c_master_write_to_device(I2C_NUM, ES8311_ADDR, b, 2, pdMS_TO_TICKS(50));
}

/*
 * Minimal ES8311 init for 16 kHz mono playback. Register sequence reused
 * from Waveshare's ESP32-S3-RLCD-4.2 02_Example/ESP-IDF/07_Audio_Test.
 */
static esp_err_t es8311_init(void) {
    if (es8311_w(0x00, 0x1F) != ESP_OK) return ESP_FAIL; /* reset */
    vTaskDelay(pdMS_TO_TICKS(20));
    es8311_w(0x00, 0x00);
    es8311_w(0x01, 0x30);          /* clock manager: MCLK from BCLK */
    es8311_w(0x02, 0x10);
    es8311_w(0x03, 0x10);
    es8311_w(0x04, 0x10);
    es8311_w(0x05, 0x00);
    es8311_w(0x06, 0x03);
    es8311_w(0x07, 0x00);
    es8311_w(0x08, 0xFF);
    es8311_w(0x09, 0x0C);          /* SDP in: I2S, 16-bit */
    es8311_w(0x0A, 0x0C);          /* SDP out: I2S, 16-bit */
    es8311_w(0x0B, 0x00);
    es8311_w(0x0C, 0x00);
    es8311_w(0x0D, 0x01);          /* enable analog */
    es8311_w(0x0E, 0x02);
    es8311_w(0x10, 0x1F);
    es8311_w(0x11, 0x7F);
    es8311_w(0x14, 0x18);          /* ADC enable */
    es8311_w(0x32, 0xC0);          /* DAC volume ~ -6 dB */
    es8311_w(0x37, 0x08);          /* DAC config */
    es8311_w(0x44, 0x08);          /* DAC -> ADC bypass off */
    return ESP_OK;
}

static void pa_enable(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PA_CTRL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
    gpio_set_level(PA_CTRL_GPIO, 1);
}

void audio_init(void) {
    pa_enable();
    if (es8311_init() != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 init failed; audio disabled");
        return;
    }

    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    if (i2s_new_channel(&cc, &s_tx, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "i2s_new_channel failed");
        return;
    }
    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BEEP_PCM_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = MCLK_GPIO,
            .bclk = BCLK_GPIO,
            .ws   = WS_GPIO,
            .dout = DOUT_GPIO,
            .din  = -1,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false }
        }
    };
    if (i2s_channel_init_std_mode(s_tx, &std) != ESP_OK) {
        ESP_LOGW(TAG, "i2s std mode init failed");
        return;
    }
    if (i2s_channel_enable(s_tx) != ESP_OK) {
        ESP_LOGW(TAG, "i2s enable failed");
        return;
    }
    s_ready = true;
    ESP_LOGI(TAG, "audio ready (16kHz mono via ES8311)");
}

void audio_beep(void) {
    if (!s_ready) return;
    size_t written = 0;
    i2s_channel_write(s_tx, BEEP_PCM, sizeof(BEEP_PCM), &written, pdMS_TO_TICKS(500));
}
