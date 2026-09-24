#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t counter_sensor_init(QueueHandle_t queue);
uint32_t counter_sensor_get_count(void);
esp_err_t counter_sensor_reset(void);
