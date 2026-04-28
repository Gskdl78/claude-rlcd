#pragma once
#include <stdbool.h>
#include <stdint.h>

#define STATE_MAX_SESSIONS  5
#define STATE_NAME_MAX      48
#define STATE_TARGET_MAX    60
#define STATE_TOOL_MAX      16
#define STATE_ID_MAX        40
#define STATE_ALERT_TEXT_MAX 80

typedef enum { SS_ACTIVE, SS_IDLE, SS_WAITING } session_status_t;

typedef struct {
    char id[STATE_ID_MAX];
    char name[STATE_NAME_MAX];
    session_status_t status;
    char last_tool[STATE_TOOL_MAX];
    char last_target[STATE_TARGET_MAX];
    uint32_t last_time;
    uint32_t tool_count;
} session_t;

typedef struct {
    float five_hour;
    float seven_day;
    float opus;        // NaN if absent
    uint32_t reset_5h;
    uint32_t reset_7d;
    bool present;
} quota_t;

typedef struct {
    char id[STATE_ID_MAX];
    char session_name[STATE_NAME_MAX];
    char text[STATE_ALERT_TEXT_MAX];
    uint32_t ts;
    bool active;
} alert_t;

typedef struct {
    session_t sessions[STATE_MAX_SESSIONS];
    uint8_t   sessions_count;
    uint16_t  sessions_total;
    quota_t   quota;
    alert_t   alert;
    bool      ws_connected;
    bool      ws_stale;
    uint32_t  last_msg_time;
} state_t;

#ifdef __cplusplus
extern "C" {
#endif

void state_init(state_t *s);
void state_apply_state_msg(state_t *s, const char *json);
void state_apply_session_update(state_t *s, const char *json);
void state_apply_session_end(state_t *s, const char *id);
void state_apply_quota_msg(state_t *s, const char *json);
void state_apply_alert(state_t *s, const char *json);
void state_apply_alert_clear(state_t *s, const char *id);

#ifdef __cplusplus
}
#endif
