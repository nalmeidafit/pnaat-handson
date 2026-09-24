#pragma once

#include <stdint.h>

#define PNAAT_OTA_URL_MAX_LEN 256

typedef enum {
    APP_EVENT_SENSOR_COUNT = 0,
    APP_EVENT_START,
    APP_EVENT_STOP,
    APP_EVENT_RESET_COUNT,
    APP_EVENT_OTA_UPDATE,
    APP_EVENT_MQTT_CONNECTED
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    uint32_t count;
    char ota_url[PNAAT_OTA_URL_MAX_LEN];
} app_event_t;
