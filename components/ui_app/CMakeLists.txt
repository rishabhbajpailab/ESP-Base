idf_component_register(SRCS "ui_app.c" "ui_screens.c" "ui_styles.c"
                       INCLUDE_DIRS "include"
                       REQUIRES board_waveshare_169 taskmgr esp_lvgl_port esp_lcd_touch)
