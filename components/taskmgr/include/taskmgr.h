#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TASKMGR_UI_MSG_INIT = 0,
    TASKMGR_UI_MSG_TELEMETRY,
    TASKMGR_UI_MSG_TOUCH,
    TASKMGR_UI_MSG_NET,
} taskmgr_ui_msg_type_t;

typedef enum {
    TASKMGR_TOPIC_WIFI_UP = 0,
    TASKMGR_TOPIC_NET_READY,
    TASKMGR_TOPIC_SENSOR_SAMPLE,
    TASKMGR_TOPIC_HTTP_DONE,
    TASKMGR_TOPIC_TOUCH_EVENT,
    TASKMGR_TOPIC_MAX,
} taskmgr_topic_t;

typedef struct {
    taskmgr_ui_msg_type_t type;
    uint16_t len;
    uint8_t payload[96];
} taskmgr_ui_msg_t;

typedef void (*taskmgr_short_fn_t)(void *ctx);
typedef void (*taskmgr_work_fn_t)(void *ctx);
typedef void (*taskmgr_work_done_fn_t)(void *completion_ctx);
typedef void (*taskmgr_ui_handler_t)(const taskmgr_ui_msg_t *msg);
typedef void (*taskmgr_topic_handler_t)(taskmgr_topic_t topic, const void *data, size_t len, void *ctx);

typedef struct {
    uint32_t ui_queue_waiting;
    uint32_t io_queue_waiting;
    uint32_t worker_queue_waiting;
    uint32_t ui_loop_us;
} taskmgr_metrics_t;

esp_err_t taskmgr_init(void);
esp_err_t taskmgr_post_ui(taskmgr_ui_msg_type_t type, const void *payload_ptr, uint16_t payload_len);
esp_err_t taskmgr_post_io(taskmgr_short_fn_t fn, void *ctx);
esp_err_t taskmgr_submit_work(taskmgr_work_fn_t fn, void *ctx, taskmgr_work_done_fn_t completion_fn, void *completion_ctx);
void taskmgr_metrics_get(taskmgr_metrics_t *out);
void taskmgr_register_ui_handler(taskmgr_ui_handler_t cb);
esp_err_t taskmgr_subscribe(taskmgr_topic_t topic, taskmgr_topic_handler_t handler, void *ctx);
esp_err_t taskmgr_publish(taskmgr_topic_t topic, const void *data, size_t len);

#ifdef __cplusplus
}
#endif
