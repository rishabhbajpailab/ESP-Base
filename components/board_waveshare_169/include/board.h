#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_init(void);
esp_lcd_panel_handle_t board_lcd_panel(void);
esp_lcd_touch_handle_t board_touch(void);
esp_err_t board_battery_read_mv(int *out_mv);
void board_backlight_set(uint8_t percent);
void board_buzzer_set(bool on);
bool board_touch_present(void);

#ifdef __cplusplus
}
#endif
