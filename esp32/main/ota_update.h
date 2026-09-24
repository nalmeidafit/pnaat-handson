#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t ota_update_start(const char *url);
bool ota_update_is_running(void);
