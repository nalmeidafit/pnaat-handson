#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t conveyor_init(void);
esp_err_t conveyor_start(void);
esp_err_t conveyor_stop(void);
bool conveyor_is_running(void);
