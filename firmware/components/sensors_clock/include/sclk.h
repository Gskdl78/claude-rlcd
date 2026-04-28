#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*env_cb_t)(float temp_c, float humidity, bool stale);
typedef void (*time_cb_t)(uint32_t utc_sec);

void sclk_init(env_cb_t env_cb, time_cb_t time_cb);
void sclk_on_wifi_up(void);
void sclk_apply_time_sync(uint32_t utc_sec, const char *tz);

#ifdef __cplusplus
}
#endif
