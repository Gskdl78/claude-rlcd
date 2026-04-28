#include "lvgl.h"
#include "ui.h"
#include <stdio.h>
#include <math.h>
#include <time.h>

LV_FONT_DECLARE(font_cjk)

void  screens_build(void);
void  ui_show_screens_apply(ui_screen_t scr);

lv_obj_t *scr_main;
static lv_obj_t *header_clock, *header_env, *header_sep;
static lv_obj_t *section_label, *quota_label;
static lv_obj_t *sessions_list;
static lv_obj_t *quota_fill[3];    /* hand-drawn bar inner fill */
static lv_obj_t *quota_pct[3];
static lv_obj_t *alert_overlay, *alert_text, *alert_session;
static lv_obj_t *stale_pill;

/* Helper: build a flat solid rectangle that survives 1-bit binarization. */
static void style_solid_black(lv_obj_t *o) {
    lv_obj_set_style_bg_color(o, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}
static void style_outlined(lv_obj_t *o) {
    lv_obj_set_style_bg_color(o, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, lv_color_black(), 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static void build_main_screen(void) {
    scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_main, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, 0);
    /* CJK font: applied at screen level so all child labels inherit it. */
    lv_obj_set_style_text_font(scr_main, &font_cjk, 0);

    header_clock = lv_label_create(scr_main);
    lv_label_set_text(header_clock, "--:--");
    lv_obj_align(header_clock, LV_ALIGN_TOP_LEFT, 6, 4);

    header_env = lv_label_create(scr_main);
    lv_label_set_text(header_env, "");
    lv_obj_align(header_env, LV_ALIGN_TOP_RIGHT, -6, 4);

    /* Separator under the header. 1-bit panel can't render anti-aliased
     * lines, so use a 2 px solid-black rect with explicit OPA_COVER. */
    header_sep = lv_obj_create(scr_main);
    lv_obj_set_size(header_sep, 392, 2);
    lv_obj_align(header_sep, LV_ALIGN_TOP_LEFT, 4, 30);
    style_solid_black(header_sep);

    section_label = lv_label_create(scr_main);
    lv_label_set_text(section_label, "SESSIONS");
    lv_obj_align(section_label, LV_ALIGN_TOP_LEFT, 6, 36);
    lv_obj_set_style_text_letter_space(section_label, 1, 0);

    sessions_list = lv_obj_create(scr_main);
    lv_obj_set_size(sessions_list, 392, 168);
    lv_obj_align(sessions_list, LV_ALIGN_TOP_LEFT, 4, 60);
    lv_obj_clear_flag(sessions_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(sessions_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sessions_list, 0, 0);
    lv_obj_set_style_pad_all(sessions_list, 0, 0);

    /* QUOTA section label, mirrors SESSIONS one. */
    quota_label = lv_label_create(scr_main);
    lv_label_set_text(quota_label, "QUOTA");
    lv_obj_align(quota_label, LV_ALIGN_BOTTOM_LEFT, 6, -52);
    lv_obj_set_style_text_letter_space(quota_label, 1, 0);

    static const int qx[3] = { 4, 138, 272 };
    static const char *labs[3] = { "5h", "7d", "Opus" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *box = lv_obj_create(scr_main);
        lv_obj_set_size(box, 124, 38);
        lv_obj_align(box, LV_ALIGN_BOTTOM_LEFT, qx[i], -8);
        lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *cap = lv_label_create(box);
        lv_label_set_text(cap, labs[i]);
        lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *pct = lv_label_create(box);
        lv_label_set_text(pct, "--");
        lv_obj_align(pct, LV_ALIGN_TOP_RIGHT, 0, 0);
        quota_pct[i] = pct;

        /* Hand-drawn progress bar = outlined frame + solid fill child.
         * Avoids anti-aliased lv_bar artefacts on the 1-bit panel. */
        lv_obj_t *frame = lv_obj_create(box);
        lv_obj_set_size(frame, 124, 12);
        lv_obj_align(frame, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        style_outlined(frame);

        lv_obj_t *fill = lv_obj_create(frame);
        lv_obj_set_size(fill, 0, 8);
        lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);
        style_solid_black(fill);
        quota_fill[i] = fill;
    }

    alert_overlay = lv_obj_create(scr_main);
    lv_obj_set_size(alert_overlay, 360, 110);
    lv_obj_align(alert_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(alert_overlay, lv_color_black(), 0);
    lv_obj_set_style_border_color(alert_overlay, lv_color_black(), 0);
    lv_obj_set_style_border_width(alert_overlay, 3, 0);
    lv_obj_add_flag(alert_overlay, LV_OBJ_FLAG_HIDDEN);

    alert_session = lv_label_create(alert_overlay);
    lv_label_set_text(alert_session, "");
    lv_obj_set_style_text_color(alert_session, lv_color_white(), 0);
    lv_obj_align(alert_session, LV_ALIGN_TOP_LEFT, 8, 8);

    alert_text = lv_label_create(alert_overlay);
    lv_label_set_text(alert_text, "");
    lv_obj_set_style_text_color(alert_text, lv_color_white(), 0);
    lv_obj_align(alert_text, LV_ALIGN_TOP_LEFT, 8, 36);

    stale_pill = lv_label_create(scr_main);
    lv_label_set_text(stale_pill, "");
    lv_obj_align(stale_pill, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_set_style_bg_color(stale_pill, lv_color_black(), 0);
    lv_obj_set_style_text_color(stale_pill, lv_color_white(), 0);
    lv_obj_set_style_pad_all(stale_pill, 2, 0);
    lv_obj_add_flag(stale_pill, LV_OBJ_FLAG_HIDDEN);
}

void ui_init(void) {
    build_main_screen();
    screens_build();
}

void ui_show(ui_screen_t scr) {
    switch (scr) {
        case UI_MAIN:
        case UI_STALE:
            lv_scr_load(scr_main);
            break;
        default:
            ui_show_screens_apply(scr);
    }
}

void ui_set_clock(uint32_t utc_sec) {
    time_t t = (time_t)utc_sec;
    struct tm tm;
    localtime_r(&t, &tm);
    static const char *wd[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d %s", tm.tm_hour, tm.tm_min, wd[tm.tm_wday]);
    lv_label_set_text(header_clock, buf);
}

void ui_set_env(float t, float h) {
    if (isnan(t) || isnan(h)) {
        lv_label_set_text(header_env, "--°C / --%");
        return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f°C · %.0f%%", t, h);
    lv_label_set_text(header_env, buf);
}

void ui_set_state(const state_t *s) {
    /* Update section header count. */
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "SESSIONS · %d ACTIVE", s->sessions_total);
    lv_label_set_text(section_label, hdr);

    lv_obj_clean(sessions_list);
    const int row_h = 28;
    for (int i = 0; i < s->sessions_count; i++) {
        const session_t *ss = &s->sessions[i];
        const char *gly =
            ss->status == SS_WAITING ? "!" :
            ss->status == SS_ACTIVE  ? "\xE2\x97\x8F" :  /* ● U+25CF */
                                       "\xE2\x97\x8B";  /* ○ U+25CB */

        lv_obj_t *gly_l = lv_label_create(sessions_list);
        lv_label_set_text(gly_l, gly);
        lv_obj_set_pos(gly_l, 0, i * row_h);

        lv_obj_t *name_l = lv_label_create(sessions_list);
        lv_label_set_text(name_l, ss->name);
        lv_label_set_long_mode(name_l, LV_LABEL_LONG_DOT);
        lv_obj_set_size(name_l, 178, row_h);
        lv_obj_set_pos(name_l, 22, i * row_h);

        char act[80];
        if (ss->last_target[0]) snprintf(act, sizeof(act), "%s %s", ss->last_tool, ss->last_target);
        else                    snprintf(act, sizeof(act), "%s",    ss->last_tool);
        lv_obj_t *act_l = lv_label_create(sessions_list);
        lv_label_set_text(act_l, act);
        lv_label_set_long_mode(act_l, LV_LABEL_LONG_DOT);
        lv_obj_set_size(act_l, 188, row_h);
        lv_obj_set_pos(act_l, 204, i * row_h);
    }
    if (s->sessions_total > s->sessions_count) {
        lv_obj_t *more = lv_label_create(sessions_list);
        char buf[32];
        snprintf(buf, sizeof(buf), "+ %d more", s->sessions_total - s->sessions_count);
        lv_label_set_text(more, buf);
        lv_obj_set_pos(more, 22, s->sessions_count * row_h);
    }

    if (s->quota.present) {
        const float pcts[3] = {
            s->quota.five_hour * 100.0f,
            s->quota.seven_day * 100.0f,
            isnan(s->quota.opus) ? -1.0f : s->quota.opus * 100.0f
        };
        char b[16];
        for (int i = 0; i < 3; i++) {
            if (pcts[i] < 0) {
                lv_label_set_text(quota_pct[i], "--");
                lv_obj_set_size(quota_fill[i], 0, 8);
            } else {
                int pv = (int)(pcts[i] + 0.5f);
                if (pv < 0) pv = 0;
                if (pv > 100) pv = 100;
                snprintf(b, sizeof(b), "%d%%", pv);
                lv_label_set_text(quota_pct[i], b);
                /* Frame inner width = 124 - 2*border(2) = 120 px usable. */
                lv_obj_set_size(quota_fill[i], (120 * pv) / 100, 8);
            }
        }
    } else {
        for (int i = 0; i < 3; i++) {
            lv_label_set_text(quota_pct[i], "--");
            lv_obj_set_size(quota_fill[i], 0, 8);
        }
    }

    if (s->alert.active) {
        lv_label_set_text(alert_session, s->alert.session_name);
        lv_label_set_text(alert_text,    s->alert.text);
        lv_obj_clear_flag(alert_overlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(alert_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_set_stale(int stale_seconds) {
    if (stale_seconds <= 0) {
        lv_obj_add_flag(stale_pill, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char b[32];
    if (stale_seconds < 60) snprintf(b, sizeof(b), " 資料停滯 %ds ", stale_seconds);
    else                    snprintf(b, sizeof(b), " 資料停滯 %dm ", stale_seconds / 60);
    lv_label_set_text(stale_pill, b);
    lv_obj_clear_flag(stale_pill, LV_OBJ_FLAG_HIDDEN);
}
