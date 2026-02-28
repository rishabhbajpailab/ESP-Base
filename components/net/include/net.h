#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t net_init(void);
esp_err_t net_http_get_async(const char *url, uint32_t request_id);
