#include "ui_app.h"

#include <string.h>
#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#define LCD_H_RES 240
#define LCD_V_RES 280

static const char *TAG = "ui_app";

LV_FONT_DECLARE(lv_font_montserrat_14);

void ui_screens_build(void);
void ui_screens_handle_touch(int x, int y, bool down);
void ui_screens_update_telemetry(const ui_telemetry_t *tm);

static lv_display_t *s_disp;

static void poll_touch_timer(lv_timer_t *t) {
    (void)t;
    esp_lcd_touch_handle_t touch = board_touch();
    if (!touch) return;
    uint16_t x[1], y[1];
    uint8_t cnt = 0;
    esp_lcd_touch_read_data(touch);
    bool down = esp_lcd_touch_get_coordinates(touch, x, y, NULL, &cnt, 1);
    if (down && cnt > 0) {
        ESP_LOGI(TAG, "touch mapped x=%u y=%u", x[0], y[0]);
        ui_screens_handle_touch(x[0], y[0], true);
    } else {
        ui_screens_handle_touch(0, 0, false);
    }
}

esp_err_t ui_app_init(void) {
    const lvgl_port_cfg_t cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&cfg), TAG, "lvgl_port_init");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,
        .panel_handle = board_lcd_panel(),
        .buffer_size = LCD_H_RES * 40,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
#if CONFIG_BOARD_LCD_ROT_90
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = true,
#elif CONFIG_BOARD_LCD_ROT_180
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = true,
#elif CONFIG_BOARD_LCD_ROT_270
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = false,
#else
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
#endif
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);
    if (!s_disp) return ESP_FAIL;

    if (board_touch()) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = s_disp,
            .handle = board_touch(),
        };
        lvgl_port_add_touch(&touch_cfg);
    }

    taskmgr_register_ui_handler(ui_app_handle_msg);
    lv_timer_create(poll_touch_timer, 50, NULL);

    ESP_LOGI(TAG, "UI init done, touch:%s", board_touch_present() ? "present" : "missing");
    return ESP_OK;
}

void ui_app_post_telemetry(const ui_telemetry_t *tm) {
    taskmgr_post_ui(TASKMGR_UI_MSG_TELEMETRY, tm, sizeof(*tm));
}

void ui_app_handle_msg(const taskmgr_ui_msg_t *msg) {
    if (!msg) return;
    switch (msg->type) {
        case TASKMGR_UI_MSG_INIT:
            ui_screens_build();
            if (!board_touch_present()) {
                lv_obj_t *label = lv_label_create(lv_scr_act());
                lv_label_set_text(label, "TOUCH: missing");
                lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -4, 4);
            }
            break;
        case TASKMGR_UI_MSG_TELEMETRY: {
            ui_telemetry_t tm;
            memcpy(&tm, msg->payload, sizeof(tm));
            ui_screens_update_telemetry(&tm);
            break;
        }
        default:
            break;
    }
}
