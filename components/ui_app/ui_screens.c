#include "ui_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#define RIPPLE_MAX 8

typedef struct {
    lv_obj_t *obj;
    float r;
    float alpha;
    bool active;
} ripple_t;

typedef struct {
    lv_obj_t *tabview;
    lv_obj_t *orb;
    lv_obj_t *status;
    lv_obj_t *ta;
    lv_obj_t *kb;
    lv_obj_t *radio_a;
    lv_obj_t *radio_b;
    lv_obj_t *radio_c;
    lv_obj_t *slider;
    lv_obj_t *sw;
    ripple_t ripples[RIPPLE_MAX];
    float x, y, vx, vy;
    float attract_x, attract_y;
    bool attract;
    bool touch_down;
    uint32_t frames;
    int64_t fps_t0;
    ui_telemetry_t tm;
} ui_state_t;

static const char *TAG = "ui_screens";
static ui_state_t s;

static void radio_select(lv_obj_t *btn) {
    lv_obj_clear_state(s.radio_a, LV_STATE_CHECKED);
    lv_obj_clear_state(s.radio_b, LV_STATE_CHECKED);
    lv_obj_clear_state(s.radio_c, LV_STATE_CHECKED);
    lv_obj_add_state(btn, LV_STATE_CHECKED);
}

static void radio_evt(lv_event_t *e) {
    radio_select(lv_event_get_target(e));
}

static void btn_evt(lv_event_t *e) {
    const char *txt = lv_event_get_user_data(e);
    if (strcmp(txt, "Beep") == 0) {
        board_buzzer_set(true);
        board_buzzer_set(false);
    } else if (strcmp(txt, "BL") == 0) {
        static bool dim;
        dim = !dim;
        board_backlight_set(dim ? 20 : 80);
    } else if (strcmp(txt, "Demo") == 0) {
        lv_tabview_set_act(s.tabview, 0, LV_ANIM_ON);
    }
}

static void ta_evt(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_obj_clear_flag(s.kb, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(s.kb, s.ta);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(s.kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void slider_evt(lv_event_t *e) {
    int v = lv_slider_get_value(lv_event_get_target(e));
    board_backlight_set((uint8_t)v);
}

static void switch_evt(lv_event_t *e) {
    s.attract = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
}

static void spawn_ripple(int x, int y) {
    for (int i = 0; i < RIPPLE_MAX; i++) {
        if (!s.ripples[i].active) {
            s.ripples[i].active = true;
            s.ripples[i].r = 2;
            s.ripples[i].alpha = 255;
            lv_obj_set_pos(s.ripples[i].obj, x - 2, y - 2);
            lv_obj_set_size(s.ripples[i].obj, 4, 4);
            lv_obj_clear_flag(s.ripples[i].obj, LV_OBJ_FLAG_HIDDEN);
            break;
        }
    }
}

static void anim_timer(lv_timer_t *timer) {
    (void)timer;
    const float dt = 0.04f;
    lv_coord_t w = lv_obj_get_width(lv_scr_act());
    lv_coord_t h = lv_obj_get_height(lv_scr_act());

    if (s.attract && s.touch_down) {
        float dx = s.attract_x - s.x;
        float dy = s.attract_y - s.y;
        s.vx += dx * 0.02f;
        s.vy += dy * 0.02f;
    }

    s.x += s.vx * dt;
    s.y += s.vy * dt;
    s.vx *= 0.995f;
    s.vy *= 0.995f;
    if (s.x < 10 || s.x > w - 10) s.vx = -s.vx;
    if (s.y < 10 || s.y > h - 40) s.vy = -s.vy;

    lv_obj_set_pos(s.orb, (lv_coord_t)(s.x - 10), (lv_coord_t)(s.y - 10));

    for (int i = 0; i < RIPPLE_MAX; i++) {
        if (!s.ripples[i].active) continue;
        s.ripples[i].r += 3;
        if (s.ripples[i].alpha > 15) s.ripples[i].alpha -= 15;
        else s.ripples[i].alpha = 0;
        lv_obj_set_size(s.ripples[i].obj, (lv_coord_t)(s.ripples[i].r * 2), (lv_coord_t)(s.ripples[i].r * 2));
        lv_obj_set_style_border_opa(s.ripples[i].obj, s.ripples[i].alpha, 0);
        if (s.ripples[i].alpha == 0) {
            s.ripples[i].active = false;
            lv_obj_add_flag(s.ripples[i].obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    s.frames++;
    int64_t now = esp_timer_get_time();
    if (now - s.fps_t0 >= 500000) {
        s.tm.fps = s.frames * 2;
        s.frames = 0;
        s.fps_t0 = now;
    }
    lv_label_set_text_fmt(s.status, "touch:%s %d,%d fps:%u bat:%d rtc:%s imu:%s",
                          s.tm.touch_down ? "dn" : "up", s.tm.touch_x, s.tm.touch_y,
                          s.tm.fps, s.tm.battery_mv, s.tm.rtc_str, s.tm.imu_ok ? "ok" : "na");
}

void ui_screens_handle_touch(int x, int y, bool down) {
    s.tm.touch_x = x;
    s.tm.touch_y = y;
    s.tm.touch_down = down;
    s.touch_down = down;
    s.attract_x = x;
    s.attract_y = y;
    if (down) {
        s.vx += (x - s.x) * 0.15f;
        s.vy += (y - s.y) * 0.15f;
        spawn_ripple(x, y);
#if CONFIG_BOARD_ENABLE_BUZZER
        board_buzzer_set(true);
        board_buzzer_set(false);
#endif
    }
}

void ui_screens_update_telemetry(const ui_telemetry_t *tm) {
    s.tm = *tm;
}

void ui_screens_build(void) {
    memset(&s, 0, sizeof(s));
    s.x = 80;
    s.y = 80;
    s.vx = 50;
    s.vy = 30;
    strcpy(s.tm.rtc_str, "--:--:--");
    s.fps_t0 = esp_timer_get_time();

    s.tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 40);
    lv_obj_t *demo = lv_tabview_add_tab(s.tabview, "Demo");
    lv_obj_t *widgets = lv_tabview_add_tab(s.tabview, "Widgets");

    s.orb = lv_obj_create(demo);
    lv_obj_set_size(s.orb, 20, 20);
    lv_obj_set_style_radius(s.orb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s.orb, lv_palette_main(LV_PALETTE_BLUE), 0);

    for (int i = 0; i < RIPPLE_MAX; i++) {
        s.ripples[i].obj = lv_obj_create(demo);
        lv_obj_set_style_bg_opa(s.ripples[i].obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(s.ripples[i].obj, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_set_style_border_width(s.ripples[i].obj, 2, 0);
        lv_obj_set_style_radius(s.ripples[i].obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(s.ripples[i].obj, LV_OBJ_FLAG_HIDDEN);
    }

    lv_timer_create(anim_timer, 40, NULL);

    lv_obj_t *row = lv_obj_create(widgets);
    lv_obj_set_size(row, lv_pct(100), 60);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char *btn_names[] = {"Beep", "BL", "Demo"};
    for (size_t i = 0; i < 3; i++) {
        lv_obj_t *b = lv_btn_create(row);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, btn_names[i]);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, btn_evt, LV_EVENT_CLICKED, (void *)btn_names[i]);
    }

    s.ta = lv_textarea_create(widgets);
    lv_obj_set_width(s.ta, lv_pct(95));
    lv_textarea_set_placeholder_text(s.ta, "Tap and type...");
    lv_obj_add_event_cb(s.ta, ta_evt, LV_EVENT_ALL, NULL);

    s.kb = lv_keyboard_create(widgets);
    lv_obj_add_flag(s.kb, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *radio_row = lv_obj_create(widgets);
    lv_obj_set_size(radio_row, lv_pct(95), 50);
    lv_obj_set_flex_flow(radio_row, LV_FLEX_FLOW_ROW);
    s.radio_a = lv_checkbox_create(radio_row);
    lv_checkbox_set_text(s.radio_a, "Mode A");
    s.radio_b = lv_checkbox_create(radio_row);
    lv_checkbox_set_text(s.radio_b, "Mode B");
    s.radio_c = lv_checkbox_create(radio_row);
    lv_checkbox_set_text(s.radio_c, "Mode C");
    lv_obj_add_state(s.radio_a, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s.radio_a, radio_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s.radio_b, radio_evt, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s.radio_c, radio_evt, LV_EVENT_CLICKED, NULL);

    s.slider = lv_slider_create(widgets);
    lv_obj_set_width(s.slider, lv_pct(95));
    lv_slider_set_range(s.slider, 5, 100);
    lv_slider_set_value(s.slider, CONFIG_BOARD_BACKLIGHT_DEFAULT_PERCENT, LV_ANIM_OFF);
    lv_obj_add_event_cb(s.slider, slider_evt, LV_EVENT_VALUE_CHANGED, NULL);

    s.sw = lv_switch_create(widgets);
    lv_obj_add_state(s.sw, LV_STATE_CHECKED);
    s.attract = true;
    lv_obj_add_event_cb(s.sw, switch_evt, LV_EVENT_VALUE_CHANGED, NULL);

    s.status = lv_label_create(widgets);
    lv_label_set_text(s.status, "status");

    ESP_LOGI(TAG, "UI screens built");
}
