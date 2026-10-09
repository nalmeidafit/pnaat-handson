#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PNAAT_OTA_URL_MAX_LEN 256

typedef enum {
    PNAAT_PIECE_RESULT_UNKNOWN = 0,
    PNAAT_PIECE_RESULT_APPROVED,
    PNAAT_PIECE_RESULT_REJECTED
} pnaat_piece_result_t;

typedef enum {
    APP_EVENT_SENSOR_COUNT = 0,
    APP_EVENT_SENSOR_STATE,
    APP_EVENT_VISION_RESULT,
    APP_EVENT_START,
    APP_EVENT_STOP,
    APP_EVENT_RESET_COUNT,
    APP_EVENT_OTA_UPDATE,
    APP_EVENT_MQTT_CONNECTED
} app_event_type_t;

typedef struct {
    app_event_type_t type;

    uint32_t count;
    bool sensor_active;

    uint32_t vision_id;
    pnaat_piece_result_t vision_result;
    float vision_confidence;

    char ota_url[PNAAT_OTA_URL_MAX_LEN];
} app_event_t;
