#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_BOOTING, NET_SOFTAP, NET_CONNECTING, NET_CONNECTED, NET_LOST
} net_status_t;

typedef struct {
    net_status_t status;
    char ssid[33];
    char ip[16];
    char mac[18];
    char mdns_name[32];
    int  attempt;
    int  max_attempts;
    int  elapsed_sec;
} net_info_t;

typedef void (*net_status_cb_t)(const net_info_t *info);

void net_init(net_status_cb_t cb);
void net_get_info(net_info_t *out);

#ifdef __cplusplus
}
#endif
