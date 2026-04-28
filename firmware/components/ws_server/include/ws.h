#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ws_msg_cb_t)(const char *line);
typedef void (*ws_conn_cb_t)(bool connected);

void ws_init(ws_msg_cb_t on_message, ws_conn_cb_t on_conn);
void ws_send_line(const char *line);
bool ws_is_connected(void);
int  ws_seconds_since_last_message(void);

#ifdef __cplusplus
}
#endif
