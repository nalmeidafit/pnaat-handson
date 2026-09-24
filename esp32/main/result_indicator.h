#pragma once

#include "app_events.h"
#include "esp_err.h"

esp_err_t result_indicator_init(void);
esp_err_t result_indicator_show(pnaat_piece_result_t result);
esp_err_t result_indicator_clear(void);
