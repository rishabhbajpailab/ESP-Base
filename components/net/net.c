#include "net.h"

#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "taskmgr.h"

static const char *TAG = "net";

typedef struct {
    char url[96];
    uint32_t req_id;
} http_job_t;

static void http_worker(void *ctx) {
    http_job_t *job = (http_job_t *)ctx;
    ESP_LOGI(TAG, "HTTP placeholder req=%lu url=%s", (unsigned long)job->req_id, job->url);
}

static void http_done(void *ctx) {
    http_job_t *job = (http_job_t *)ctx;
    taskmgr_publish(TASKMGR_TOPIC_HTTP_DONE, job, sizeof(*job));
}

esp_err_t net_init(void) {
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    ESP_LOGI(TAG, "network skeleton ready (configure Wi-Fi in project-specific module)");
    return ESP_OK;
}

esp_err_t net_http_get_async(const char *url, uint32_t request_id) {
    if (!url) return ESP_ERR_INVALID_ARG;
    static http_job_t jobs[4];
    static uint8_t idx;
    http_job_t *job = &jobs[idx++ % 4];
    memset(job, 0, sizeof(*job));
    strlcpy(job->url, url, sizeof(job->url));
    job->req_id = request_id;
    return taskmgr_submit_work(http_worker, job, http_done, job);
}
