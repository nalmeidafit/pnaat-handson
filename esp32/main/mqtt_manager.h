#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t mqtt_manager_init(QueueHandle_t queue);

void mqtt_publish_sensor_count(uint32_t count);
void mqtt_publish_sensor_presence(bool present);
void mqtt_publish_conveyor_state(bool running);
void mqtt_publish_ota_status(const char *status, esp_err_t error_code);
