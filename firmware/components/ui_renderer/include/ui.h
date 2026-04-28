#pragma once
#include <stdint.h>
#include "state_store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_BOOT, UI_SOFTAP, UI_CONNECTING, UI_LISTENING,
    UI_MAIN, UI_STALE
} ui_screen_t;

void ui_init(void);
void ui_show(ui_screen_t scr);
void ui_set_state(const state_t *s);
void ui_set_env(float temp_c, float humidity);
void ui_set_clock(uint32_t utc_sec);
void ui_set_softap_info(const char *ssid, const char *url);
void ui_set_connecting_info(const char *ssid, int attempt, int max_attempts, int elapsed_sec);
void ui_set_listening_info(const char *mdns_name, const char *ip);
void ui_set_stale(int stale_seconds);

#ifdef __cplusplus
}
#endif
