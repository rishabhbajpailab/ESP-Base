#include "taskmgr.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

#define UI_Q_DEPTH 16
#define IO_Q_DEPTH 16
#define WORK_Q_DEPTH 16
#define WORKERS 2
#define MAX_SUBS 4

typedef struct {
    taskmgr_short_fn_t fn;
    void *ctx;
} io_job_t;

typedef struct {
    taskmgr_work_fn_t fn;
    void *ctx;
    taskmgr_work_done_fn_t done;
    void *done_ctx;
} work_job_t;

typedef struct {
    taskmgr_topic_handler_t h;
    void *ctx;
} sub_t;

typedef struct {
    taskmgr_topic_t topic;
    uint16_t len;
    uint8_t payload[64];
} topic_msg_t;

static const char *TAG = "taskmgr";
static QueueHandle_t s_ui_q, s_io_q, s_work_q;
static QueueHandle_t s_topic_q;
static taskmgr_ui_handler_t s_ui_handler;
static sub_t s_subs[TASKMGR_TOPIC_MAX][MAX_SUBS];
static taskmgr_metrics_t s_metrics;

static void ui_task(void *arg) {
    (void)arg;
    taskmgr_ui_msg_t msg;
    while (1) {
        int64_t t0 = esp_timer_get_time();
        while (xQueueReceive(s_ui_q, &msg, 0) == pdTRUE) {
            if (s_ui_handler) {
                s_ui_handler(&msg);
            }
        }
        lv_timer_handler();
        int64_t dt = esp_timer_get_time() - t0;
        s_metrics.ui_loop_us = (uint32_t)dt;
        s_metrics.ui_queue_waiting = (uint32_t)uxQueueMessagesWaiting(s_ui_q);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void io_task(void *arg) {
    (void)arg;
    io_job_t job;
    topic_msg_t evt;
    while (1) {
        if (xQueueReceive(s_io_q, &job, pdMS_TO_TICKS(5)) == pdTRUE && job.fn) {
            job.fn(job.ctx);
        }
        while (xQueueReceive(s_topic_q, &evt, 0) == pdTRUE) {
            for (int i = 0; i < MAX_SUBS; i++) {
                if (s_subs[evt.topic][i].h) {
                    s_subs[evt.topic][i].h(evt.topic, evt.payload, evt.len, s_subs[evt.topic][i].ctx);
                }
            }
        }
        s_metrics.io_queue_waiting = (uint32_t)uxQueueMessagesWaiting(s_io_q);
    }
}

static void worker_task(void *arg) {
    (void)arg;
    work_job_t job;
    while (1) {
        if (xQueueReceive(s_work_q, &job, portMAX_DELAY) == pdTRUE && job.fn) {
            job.fn(job.ctx);
            if (job.done) {
                taskmgr_post_io((taskmgr_short_fn_t)job.done, job.done_ctx);
            }
        }
        s_metrics.worker_queue_waiting = (uint32_t)uxQueueMessagesWaiting(s_work_q);
    }
}

esp_err_t taskmgr_init(void) {
    s_ui_q = xQueueCreate(UI_Q_DEPTH, sizeof(taskmgr_ui_msg_t));
    s_io_q = xQueueCreate(IO_Q_DEPTH, sizeof(io_job_t));
    s_work_q = xQueueCreate(WORK_Q_DEPTH, sizeof(work_job_t));
    s_topic_q = xQueueCreate(IO_Q_DEPTH, sizeof(topic_msg_t));
    if (!s_ui_q || !s_io_q || !s_work_q || !s_topic_q) return ESP_ERR_NO_MEM;

    xTaskCreate(ui_task, "ui_task", 8192, NULL, 6, NULL);
    xTaskCreate(io_task, "io_task", 4096, NULL, 5, NULL);
    for (int i = 0; i < WORKERS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "worker_%d", i);
        xTaskCreate(worker_task, name, 4096, NULL, 4, NULL);
    }
    ESP_LOGI(TAG, "taskmgr started");
    return ESP_OK;
}

esp_err_t taskmgr_post_ui(taskmgr_ui_msg_type_t type, const void *payload_ptr, uint16_t payload_len) {
    taskmgr_ui_msg_t msg = {.type = type, .len = payload_len};
    if (payload_len > sizeof(msg.payload)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (payload_ptr && payload_len) {
        memcpy(msg.payload, payload_ptr, payload_len);
    }
    return xQueueSend(s_ui_q, &msg, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t taskmgr_post_io(taskmgr_short_fn_t fn, void *ctx) {
    io_job_t job = {.fn = fn, .ctx = ctx};
    return xQueueSend(s_io_q, &job, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t taskmgr_submit_work(taskmgr_work_fn_t fn, void *ctx, taskmgr_work_done_fn_t completion_fn, void *completion_ctx) {
    work_job_t job = {.fn = fn, .ctx = ctx, .done = completion_fn, .done_ctx = completion_ctx};
    return xQueueSend(s_work_q, &job, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void taskmgr_metrics_get(taskmgr_metrics_t *out) {
    if (out) *out = s_metrics;
}

void taskmgr_register_ui_handler(taskmgr_ui_handler_t cb) { s_ui_handler = cb; }

esp_err_t taskmgr_subscribe(taskmgr_topic_t topic, taskmgr_topic_handler_t handler, void *ctx) {
    if (topic >= TASKMGR_TOPIC_MAX || !handler) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (!s_subs[topic][i].h) {
            s_subs[topic][i].h = handler;
            s_subs[topic][i].ctx = ctx;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t taskmgr_publish(taskmgr_topic_t topic, const void *data, size_t len) {
    if (topic >= TASKMGR_TOPIC_MAX || len > sizeof(((topic_msg_t *)0)->payload)) return ESP_ERR_INVALID_ARG;
    topic_msg_t evt = {.topic = topic, .len = len};
    if (data && len) memcpy(evt.payload, data, len);
    return xQueueSend(s_topic_q, &evt, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
