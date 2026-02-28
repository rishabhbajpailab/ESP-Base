#pragma once

#include "esp_err.h"

esp_err_t sensors_init(void);
void sensors_telemetry_poll(void);
