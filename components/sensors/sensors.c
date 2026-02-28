#include "sensors.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_app.h"

static const char *TAG = "sensors";
static bool s_rtc_ok;
static bool s_imu_ok;

static void rtc_probe(void) {
#if CONFIG_BOARD_ENABLE_RTC
    s_rtc_ok = true;
#else
    s_rtc_ok = false;
#endif
}

static void imu_probe(void) {
#if CONFIG_BOARD_ENABLE_IMU
    s_imu_ok = true;
#else
    s_imu_ok = false;
#endif
}

void sensors_telemetry_poll(void) {
    ui_telemetry_t tm = {0};
    int bat_mv = 0;
    if (board_battery_read_mv(&bat_mv) == ESP_OK) {
        tm.battery_mv = bat_mv;
    }

    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    strftime(tm.rtc_str, sizeof(tm.rtc_str), "%H:%M:%S", &ti);
    tm.imu_ok = s_imu_ok;

    ESP_LOGI(TAG, "telemetry bat=%dmV rtc=%s imu=%s", tm.battery_mv, tm.rtc_str, tm.imu_ok ? "ok" : "missing");
    ui_app_post_telemetry(&tm);
}

static void telemetry_task(void *arg) {
    (void)arg;
    while (1) {
        sensors_telemetry_poll();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t sensors_init(void) {
    rtc_probe();
    imu_probe();
    ESP_LOGI(TAG, "RTC probe: %s", s_rtc_ok ? "OK" : "missing/disabled");
    ESP_LOGI(TAG, "IMU probe: %s", s_imu_ok ? "OK" : "missing/disabled");
    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 3, NULL);
    return ESP_OK;
}
