#include "esp_log.h"
#include "board.h"
#include "net.h"
#include "sensors.h"
#include "taskmgr.h"
#include "ui_app.h"

static const char *TAG = "app_main";

void app_main(void) {
    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(taskmgr_init());
    ESP_ERROR_CHECK(ui_app_init());
    ESP_ERROR_CHECK(sensors_init());
    ESP_ERROR_CHECK(net_init());

    ESP_LOGI(TAG, "Template boot complete");
    taskmgr_post_ui(TASKMGR_UI_MSG_INIT, NULL, 0);
}
