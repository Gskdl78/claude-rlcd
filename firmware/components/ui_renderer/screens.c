#include "lvgl.h"
#include "ui.h"
#include <stdio.h>

LV_FONT_DECLARE(font_cjk)

static lv_obj_t *scr_softap, *scr_connecting, *scr_listening;
static lv_obj_t *softap_ssid_l, *softap_url_l;
static lv_obj_t *connecting_ssid_l, *connecting_status_l;
static lv_obj_t *listening_name_l, *listening_ip_l;

static lv_obj_t *make_centered_screen(const char *title) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, &font_cjk, 0);
    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, title);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 36);
    return scr;
}

static lv_obj_t *make_label_at(lv_obj_t *parent, int y) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, "");
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    return l;
}

void screens_build(void) {
    scr_softap = make_centered_screen("配網模式");
    softap_ssid_l = make_label_at(scr_softap, 100);
    softap_url_l  = make_label_at(scr_softap, 140);
    lv_obj_t *hint = lv_label_create(scr_softap);
    lv_label_set_text(hint, "用手機連此 SSID\n瀏覽器自動跳出設定頁");
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 200);

    scr_connecting = make_centered_screen("連線中");
    connecting_ssid_l   = make_label_at(scr_connecting, 100);
    connecting_status_l = make_label_at(scr_connecting, 140);

    scr_listening = make_centered_screen("等待 PC bridge");
    listening_name_l = make_label_at(scr_listening, 110);
    listening_ip_l   = make_label_at(scr_listening, 150);
    lv_obj_t *foot = lv_label_create(scr_listening);
    lv_label_set_text(foot, "請啟動 PC 端 bridge");
    lv_obj_align(foot, LV_ALIGN_TOP_MID, 0, 210);
}

void ui_set_softap_info(const char *ssid, const char *url) {
    char b[64];
    snprintf(b, sizeof(b), "SSID: %s", ssid);
    lv_label_set_text(softap_ssid_l, b);
    snprintf(b, sizeof(b), "%s", url);
    lv_label_set_text(softap_url_l, b);
}

void ui_set_connecting_info(const char *ssid, int attempt, int max_attempts, int elapsed_sec) {
    char b[64];
    snprintf(b, sizeof(b), "SSID: %s", ssid);
    lv_label_set_text(connecting_ssid_l, b);
    snprintf(b, sizeof(b), "嘗試 %d / %d  ·  %d s", attempt, max_attempts, elapsed_sec);
    lv_label_set_text(connecting_status_l, b);
}

void ui_set_listening_info(const char *mdns_name, const char *ip) {
    char b[64];
    snprintf(b, sizeof(b), "%s.local", mdns_name);
    lv_label_set_text(listening_name_l, b);
    snprintf(b, sizeof(b), "%s", ip);
    lv_label_set_text(listening_ip_l, b);
}

void ui_show_screens_apply(ui_screen_t scr) {
    switch (scr) {
        case UI_SOFTAP:     lv_scr_load(scr_softap);     break;
        case UI_CONNECTING: lv_scr_load(scr_connecting); break;
        case UI_LISTENING:  lv_scr_load(scr_listening);  break;
        default: break;
    }
}
