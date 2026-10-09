#include "piece_matcher.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

static const char *TAG = "matcher";

typedef struct {
    uint32_t sensor_count;
    int64_t timestamp_us;
} pending_sensor_t;

typedef struct {
    uint32_t camera_id;
    pnaat_piece_result_t result;
    float confidence;
    int64_t timestamp_us;
} pending_vision_t;

static QueueHandle_t s_sensor_queue;
static QueueHandle_t s_vision_queue;

static int64_t timeout_us(void)
{
    return (int64_t)CONFIG_PNAAT_MATCH_TIMEOUT_MS * 1000LL;
}

static void purge_expired_sensor(int64_t now)
{
    pending_sensor_t item;

    while (
        s_sensor_queue != NULL &&
        xQueuePeek(s_sensor_queue, &item, 0) == pdTRUE &&
        (now - item.timestamp_us) > timeout_us()
    ) {
        xQueueReceive(s_sensor_queue, &item, 0);

        ESP_LOGW(
            TAG,
            "Contagem física %lu expirou sem resultado de visão.",
            (unsigned long)item.sensor_count
        );
    }
}

static void purge_expired_vision(int64_t now)
{
    pending_vision_t item;

    while (
        s_vision_queue != NULL &&
        xQueuePeek(s_vision_queue, &item, 0) == pdTRUE &&
        (now - item.timestamp_us) > timeout_us()
    ) {
        xQueueReceive(s_vision_queue, &item, 0);

        ESP_LOGW(
            TAG,
            "Resultado da câmera %lu expirou sem chegar ao sensor final.",
            (unsigned long)item.camera_id
        );
    }
}

static void fill_match(
    pnaat_piece_match_t *match,
    uint32_t sensor_count,
    const pending_vision_t *vision
)
{
    if (match == NULL || vision == NULL) {
        return;
    }

    match->sensor_count = sensor_count;
    match->camera_id = vision->camera_id;
    match->result = vision->result;
    match->confidence = vision->confidence;
}

esp_err_t piece_matcher_init(void)
{
    s_sensor_queue = xQueueCreate(
        CONFIG_PNAAT_MATCH_QUEUE_DEPTH,
        sizeof(pending_sensor_t)
    );

    if (s_sensor_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_vision_queue = xQueueCreate(
        CONFIG_PNAAT_MATCH_QUEUE_DEPTH,
        sizeof(pending_vision_t)
    );

    if (s_vision_queue == NULL) {
        vQueueDelete(s_sensor_queue);
        s_sensor_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(
        TAG,
        "Sincronização visão/sensor habilitada: fila=%d timeout=%d ms",
        CONFIG_PNAAT_MATCH_QUEUE_DEPTH,
        CONFIG_PNAAT_MATCH_TIMEOUT_MS
    );

    return ESP_OK;
}

bool piece_matcher_on_sensor(
    uint32_t sensor_count,
    pnaat_piece_match_t *match
)
{
    if (s_sensor_queue == NULL || s_vision_queue == NULL) {
        return false;
    }

    const int64_t now = esp_timer_get_time();

    purge_expired_sensor(now);
    purge_expired_vision(now);

    pending_vision_t vision;

    if (xQueueReceive(s_vision_queue, &vision, 0) == pdTRUE) {
        fill_match(match, sensor_count, &vision);
        return true;
    }

    pending_sensor_t sensor = {
        .sensor_count = sensor_count,
        .timestamp_us = now,
    };

    if (xQueueSend(s_sensor_queue, &sensor, 0) != pdTRUE) {
        ESP_LOGW(
            TAG,
            "Fila de peças físicas cheia; contagem %lu não foi enfileirada.",
            (unsigned long)sensor_count
        );
    } else {
        ESP_LOGI(
            TAG,
            "Peça física %lu aguardando resultado da visão.",
            (unsigned long)sensor_count
        );
    }

    return false;
}

bool piece_matcher_on_vision(
    uint32_t camera_id,
    pnaat_piece_result_t result,
    float confidence,
    pnaat_piece_match_t *match
)
{
    if (s_sensor_queue == NULL || s_vision_queue == NULL) {
        return false;
    }

    if (result == PNAAT_PIECE_RESULT_UNKNOWN) {
        ESP_LOGW(
            TAG,
            "Resultado desconhecido da câmera %lu ignorado.",
            (unsigned long)camera_id
        );
        return false;
    }

    const int64_t now = esp_timer_get_time();

    purge_expired_sensor(now);
    purge_expired_vision(now);

    pending_sensor_t sensor;

    if (xQueueReceive(s_sensor_queue, &sensor, 0) == pdTRUE) {
        pending_vision_t vision = {
            .camera_id = camera_id,
            .result = result,
            .confidence = confidence,
            .timestamp_us = now,
        };

        fill_match(match, sensor.sensor_count, &vision);
        return true;
    }

    pending_vision_t vision = {
        .camera_id = camera_id,
        .result = result,
        .confidence = confidence,
        .timestamp_us = now,
    };

    if (xQueueSend(s_vision_queue, &vision, 0) != pdTRUE) {
        ESP_LOGW(
            TAG,
            "Fila de resultados da visão cheia; câmera %lu não foi enfileirada.",
            (unsigned long)camera_id
        );
    } else {
        ESP_LOGI(
            TAG,
            "Resultado da câmera %lu aguardando sensor final.",
            (unsigned long)camera_id
        );
    }

    return false;
}

void piece_matcher_reset(void)
{
    if (s_sensor_queue != NULL) {
        xQueueReset(s_sensor_queue);
    }

    if (s_vision_queue != NULL) {
        xQueueReset(s_vision_queue);
    }

    ESP_LOGI(TAG, "Filas de sincronização limpas.");
}
