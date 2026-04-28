#include "state_store.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void state_init(state_t *s) {
    memset(s, 0, sizeof(*s));
    s->quota.opus = NAN;
}

static void copy_str(char *dst, size_t dst_sz, const char *src) {
    if (!src) { dst[0] = 0; return; }
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = 0;
}

static session_status_t parse_status(const char *st) {
    if (!st) return SS_ACTIVE;
    if (strcmp(st, "idle") == 0)    return SS_IDLE;
    if (strcmp(st, "waiting") == 0) return SS_WAITING;
    return SS_ACTIVE;
}

static int find_session(state_t *s, const char *id) {
    for (int i = 0; i < s->sessions_count; i++)
        if (strcmp(s->sessions[i].id, id) == 0) return i;
    return -1;
}

static void apply_patch(session_t *sess, cJSON *patch) {
    cJSON *f;
    if ((f = cJSON_GetObjectItem(patch, "name"))       && cJSON_IsString(f))
        copy_str(sess->name,        sizeof(sess->name), f->valuestring);
    if ((f = cJSON_GetObjectItem(patch, "lastTool"))   && cJSON_IsString(f))
        copy_str(sess->last_tool,   sizeof(sess->last_tool), f->valuestring);
    if ((f = cJSON_GetObjectItem(patch, "lastTarget")) && cJSON_IsString(f))
        copy_str(sess->last_target, sizeof(sess->last_target), f->valuestring);
    if ((f = cJSON_GetObjectItem(patch, "status"))     && cJSON_IsString(f))
        sess->status = parse_status(f->valuestring);
    if ((f = cJSON_GetObjectItem(patch, "lastTime"))   && cJSON_IsNumber(f))
        sess->last_time = (uint32_t)f->valuedouble;
    if ((f = cJSON_GetObjectItem(patch, "toolCount")) && cJSON_IsNumber(f))
        sess->tool_count = (uint32_t)f->valuedouble;
}

void state_apply_state_msg(state_t *s, const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return;

    cJSON *sessions = cJSON_GetObjectItem(root, "sessions");
    if (cJSON_IsObject(sessions)) {
        cJSON *active = cJSON_GetObjectItem(sessions, "active");
        cJSON *items  = cJSON_GetObjectItem(sessions, "items");
        s->sessions_total = (cJSON_IsNumber(active)) ? (uint16_t)active->valueint : 0;
        s->sessions_count = 0;
        cJSON *it;
        int idx = 0;
        cJSON_ArrayForEach(it, items) {
            if (idx >= STATE_MAX_SESSIONS) break;
            session_t *sess = &s->sessions[idx++];
            memset(sess, 0, sizeof(*sess));
            cJSON *f;
            if ((f = cJSON_GetObjectItem(it, "id"))         && cJSON_IsString(f))
                copy_str(sess->id, sizeof(sess->id), f->valuestring);
            if ((f = cJSON_GetObjectItem(it, "name"))       && cJSON_IsString(f))
                copy_str(sess->name, sizeof(sess->name), f->valuestring);
            if ((f = cJSON_GetObjectItem(it, "lastTool"))   && cJSON_IsString(f))
                copy_str(sess->last_tool, sizeof(sess->last_tool), f->valuestring);
            if ((f = cJSON_GetObjectItem(it, "lastTarget")) && cJSON_IsString(f))
                copy_str(sess->last_target, sizeof(sess->last_target), f->valuestring);
            if ((f = cJSON_GetObjectItem(it, "status"))     && cJSON_IsString(f))
                sess->status = parse_status(f->valuestring);
            if ((f = cJSON_GetObjectItem(it, "lastTime"))   && cJSON_IsNumber(f))
                sess->last_time = (uint32_t)f->valuedouble;
            if ((f = cJSON_GetObjectItem(it, "toolCount")) && cJSON_IsNumber(f))
                sess->tool_count = (uint32_t)f->valuedouble;
        }
        s->sessions_count = idx;
    }

    cJSON *quota = cJSON_GetObjectItem(root, "quota");
    if (cJSON_IsObject(quota)) {
        cJSON *fh = cJSON_GetObjectItem(quota, "fiveHour");
        cJSON *sd = cJSON_GetObjectItem(quota, "sevenDay");
        cJSON *op = cJSON_GetObjectItem(quota, "opus");
        if (cJSON_IsObject(fh)) {
            cJSON *u = cJSON_GetObjectItem(fh, "utilization");
            cJSON *r = cJSON_GetObjectItem(fh, "reset");
            if (cJSON_IsNumber(u)) s->quota.five_hour = (float)u->valuedouble;
            if (cJSON_IsNumber(r)) s->quota.reset_5h  = (uint32_t)r->valuedouble;
        }
        if (cJSON_IsObject(sd)) {
            cJSON *u = cJSON_GetObjectItem(sd, "utilization");
            cJSON *r = cJSON_GetObjectItem(sd, "reset");
            if (cJSON_IsNumber(u)) s->quota.seven_day = (float)u->valuedouble;
            if (cJSON_IsNumber(r)) s->quota.reset_7d  = (uint32_t)r->valuedouble;
        }
        if (op && cJSON_IsObject(op)) {
            cJSON *u = cJSON_GetObjectItem(op, "utilization");
            s->quota.opus = (cJSON_IsNumber(u)) ? (float)u->valuedouble : NAN;
        } else {
            s->quota.opus = NAN;
        }
        s->quota.present = true;
    } else if (cJSON_IsNull(quota)) {
        s->quota.present = false;
    }

    cJSON_Delete(root);
}

void state_apply_session_update(state_t *s, const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return;
    cJSON *id = cJSON_GetObjectItem(root, "id");
    cJSON *patch = cJSON_GetObjectItem(root, "patch");
    if (!cJSON_IsString(id) || !cJSON_IsObject(patch)) { cJSON_Delete(root); return; }

    int idx = find_session(s, id->valuestring);
    if (idx >= 0) {
        apply_patch(&s->sessions[idx], patch);
    } else if (s->sessions_count < STATE_MAX_SESSIONS) {
        session_t *sess = &s->sessions[s->sessions_count++];
        memset(sess, 0, sizeof(*sess));
        copy_str(sess->id, sizeof(sess->id), id->valuestring);
        apply_patch(sess, patch);
        if (s->sessions_total < s->sessions_count) s->sessions_total = s->sessions_count;
    }
    cJSON_Delete(root);
}

void state_apply_session_end(state_t *s, const char *id) {
    if (!id) return;
    int idx = find_session(s, id);
    if (idx < 0) return;
    for (int i = idx; i < s->sessions_count - 1; i++) s->sessions[i] = s->sessions[i + 1];
    s->sessions_count--;
    if (s->sessions_total > 0) s->sessions_total--;
}

void state_apply_quota_msg(state_t *s, const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return;
    cJSON *fh = cJSON_GetObjectItem(root, "fiveHour");
    cJSON *sd = cJSON_GetObjectItem(root, "sevenDay");
    cJSON *op = cJSON_GetObjectItem(root, "opus");
    if (cJSON_IsObject(fh)) {
        cJSON *u = cJSON_GetObjectItem(fh, "utilization");
        cJSON *r = cJSON_GetObjectItem(fh, "reset");
        if (cJSON_IsNumber(u)) s->quota.five_hour = (float)u->valuedouble;
        if (cJSON_IsNumber(r)) s->quota.reset_5h  = (uint32_t)r->valuedouble;
    }
    if (cJSON_IsObject(sd)) {
        cJSON *u = cJSON_GetObjectItem(sd, "utilization");
        cJSON *r = cJSON_GetObjectItem(sd, "reset");
        if (cJSON_IsNumber(u)) s->quota.seven_day = (float)u->valuedouble;
        if (cJSON_IsNumber(r)) s->quota.reset_7d  = (uint32_t)r->valuedouble;
    }
    if (op && cJSON_IsObject(op)) {
        cJSON *u = cJSON_GetObjectItem(op, "utilization");
        s->quota.opus = (cJSON_IsNumber(u)) ? (float)u->valuedouble : NAN;
    } else {
        s->quota.opus = NAN;
    }
    s->quota.present = true;
    cJSON_Delete(root);
}

void state_apply_alert(state_t *s, const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return;
    cJSON *id   = cJSON_GetObjectItem(root, "id");
    cJSON *name = cJSON_GetObjectItem(root, "sessionName");
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (cJSON_IsString(id))   copy_str(s->alert.id,           sizeof(s->alert.id),           id->valuestring);
    if (cJSON_IsString(name)) copy_str(s->alert.session_name, sizeof(s->alert.session_name), name->valuestring);
    if (cJSON_IsString(text)) copy_str(s->alert.text,         sizeof(s->alert.text),         text->valuestring);
    s->alert.active = true;
    cJSON_Delete(root);
}

void state_apply_alert_clear(state_t *s, const char *id) {
    if (id && s->alert.active && strcmp(s->alert.id, id) == 0) {
        s->alert.active = false;
    }
}
