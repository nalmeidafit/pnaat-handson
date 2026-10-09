#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app_events.h"
#include "esp_err.h"

typedef struct {
    uint32_t sensor_count;
    uint32_t camera_id;
    pnaat_piece_result_t result;
    float confidence;
} pnaat_piece_match_t;

esp_err_t piece_matcher_init(void);

bool piece_matcher_on_sensor(
    uint32_t sensor_count,
    pnaat_piece_match_t *match
);

bool piece_matcher_on_vision(
    uint32_t camera_id,
    pnaat_piece_result_t result,
    float confidence,
    pnaat_piece_match_t *match
);

void piece_matcher_reset(void);
