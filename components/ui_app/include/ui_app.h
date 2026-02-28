#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "taskmgr.h"

typedef struct {
    int battery_mv;
    bool touch_down;
    int touch_x;
    int touch_y;
    char rtc_str[24];
    bool imu_ok;
    uint16_t fps;
} ui_telemetry_t;

esp_err_t ui_app_init(void);
void ui_app_handle_msg(const taskmgr_ui_msg_t *msg);
void ui_app_post_telemetry(const ui_telemetry_t *tm);
