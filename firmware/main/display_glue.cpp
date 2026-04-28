// C++ shim that brings up the Waveshare reflective-LCD BSP and wires it
// into LVGL. Exposes a plain-C entry point so app_main.c can stay C.
//
// Pin assignments below come from Waveshare's ESP32-S3-RLCD-4.2 reference
// (02_Example/ESP-IDF/01_LCD_Test). If Waveshare changes pinout in a board
// revision, override here.

#include "display_bsp.h"
#include "lvgl_bsp.h"
#include "esp_log.h"

extern "C" {
    void display_init(void);
}

#define LCD_W 400
#define LCD_H 300

/* Pinout from Waveshare ESP32-S3-RLCD-4.2 board labels:
 *   GPIO12 LCD SDA  (MOSI)
 *   GPIO11 LCD SCL  (SCLK)
 *   GPIO5  LCD RS   (DC)
 *   GPIO40 LCD CS
 *   GPIO41 LCD RESET
 *   GPIO6  LCD TE   (tearing-effect, unused) */
#define PIN_MOSI 12
#define PIN_SCLK 11
#define PIN_DC   5
#define PIN_CS   40
#define PIN_RST  41

static DisplayPort *s_disp = nullptr;

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    if (!s_disp) { lv_disp_flush_ready(drv); return; }
    int32_t x1 = area->x1, x2 = area->x2;
    int32_t y1 = area->y1, y2 = area->y2;
    for (int32_t y = y1; y <= y2; y++) {
        for (int32_t x = x1; x <= x2; x++) {
            // 1-bit panel: any non-zero -> white. LVGL gives us 16-bit color
            // when MONOCHROME isn't set, so drive on luminance proxy.
            uint8_t v = color_p->full ? ColorWhite : ColorBlack;
            s_disp->RLCD_SetPixel((uint16_t)x, (uint16_t)y, v);
            color_p++;
        }
    }
    s_disp->RLCD_Display();
    lv_disp_flush_ready(drv);
}

void display_init(void) {
    s_disp = new DisplayPort(PIN_MOSI, PIN_SCLK, PIN_DC, PIN_CS, PIN_RST,
                             LCD_W, LCD_H);
    s_disp->RLCD_Init();
    s_disp->RLCD_ColorClear(ColorWhite);
    s_disp->RLCD_Display();

    Lvgl_PortInit(LCD_W, LCD_H, lvgl_flush_cb);
}
