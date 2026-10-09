#include "app_events.h"
#include "conveyor.h"
#include "counter_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "hardware_config.h"
#include "mqtt_manager.h"
#include "nvs_flash.h"
#include "ota_update.h"
#include "piece_matcher.h"
#include "result_indicator.h"
#include "sdkconfig.h"
#include "wifi_manager.h"

static const char *TAG = "pnaat";

static const char *result_name(
    pnaat_piece_result_t result
)
{
    switch (result) {
        case PNAAT_PIECE_RESULT_APPROVED:
            return "APROVADA";

        case PNAAT_PIECE_RESULT_REJECTED:
            return "REJEITADA";

        default:
            return "DESCONHECIDA";
    }
}

static void apply_piece_match(
    const pnaat_piece_match_t *match
)
{
    if (match == NULL) {
        return;
    }

    ESP_LOGI(
        TAG,
        "Associação: sensor=%lu câmera=%lu -> %s",
        (unsigned long)match->sensor_count,
        (unsigned long)match->camera_id,
        result_name(match->result)
    );

    result_indicator_show(
        match->result
    );

    mqtt_publish_piece_match(
        match->sensor_count,
        match->camera_id,
        match->result,
        match->confidence
    );
}

void app_main(void)
{
    ESP_LOGI(
        TAG,
        "PNAAT - ESP32-S3 / sensor final / MQTT / LEDs / OTA"
    );

    ESP_LOGI(
        TAG,
        "ESP-IDF v5.5.2 / target fixo ESP32-S3"
    );

    esp_err_t err =
        nvs_flash_init();

    if (
        err ==
            ESP_ERR_NVS_NO_FREE_PAGES ||
        err ==
            ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {
        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ESP_ERROR_CHECK(
            nvs_flash_init()
        );
    } else {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(
        hardware_config_validate()
    );

    QueueHandle_t queue =
        xQueueCreate(
            24,
            sizeof(app_event_t)
        );

    if (queue == NULL) {
        ESP_LOGE(
            TAG,
            "Falha ao criar fila principal."
        );
        return;
    }

#if CONFIG_PNAAT_MATCH_VISION_WITH_SENSOR
    ESP_ERROR_CHECK(
        piece_matcher_init()
    );
#endif

    ESP_ERROR_CHECK(
        conveyor_init()
    );

    ESP_ERROR_CHECK(
        counter_sensor_init(queue)
    );

    ESP_ERROR_CHECK(
        result_indicator_init()
    );

    ESP_ERROR_CHECK(
        wifi_manager_init()
    );

    ESP_ERROR_CHECK(
        mqtt_manager_init(queue)
    );

    while (true) {
        app_event_t event;

        if (
            xQueueReceive(
                queue,
                &event,
                portMAX_DELAY
            ) != pdTRUE
        ) {
            continue;
        }

        switch (event.type) {
            case APP_EVENT_SENSOR_COUNT: {
                mqtt_publish_sensor_count(
                    event.count
                );

#if CONFIG_PNAAT_MATCH_VISION_WITH_SENSOR
                pnaat_piece_match_t match;

                if (
                    piece_matcher_on_sensor(
                        event.count,
                        &match
                    )
                ) {
                    apply_piece_match(
                        &match
                    );
                }
#endif
                break;
            }

            case APP_EVENT_SENSOR_STATE:
                mqtt_publish_sensor_presence(
                    event.sensor_active
                );
                break;

            case APP_EVENT_VISION_RESULT: {
                ESP_LOGI(
                    TAG,
                    "Visão recebida: câmera=%lu -> %s",
                    (unsigned long)event.vision_id,
                    result_name(
                        event.vision_result
                    )
                );

#if CONFIG_PNAAT_MATCH_VISION_WITH_SENSOR
                pnaat_piece_match_t match;

                if (
                    piece_matcher_on_vision(
                        event.vision_id,
                        event.vision_result,
                        event.vision_confidence,
                        &match
                    )
                ) {
                    apply_piece_match(
                        &match
                    );
                }
#else
                result_indicator_show(
                    event.vision_result
                );
#endif
                break;
            }

            case APP_EVENT_START: {
                if (
                    ota_update_is_running()
                ) {
                    ESP_LOGW(
                        TAG,
                        "START ignorado durante OTA."
                    );
                    break;
                }

                esp_err_t motor_err =
                    conveyor_start();

                if (motor_err == ESP_OK) {
                    mqtt_publish_conveyor_state(
                        true
                    );
                } else {
                    ESP_LOGW(
                        TAG,
                        "START não executado: %s",
                        esp_err_to_name(
                            motor_err
                        )
                    );
                }

                break;
            }

            case APP_EVENT_STOP:
                conveyor_stop();

                mqtt_publish_conveyor_state(
                    false
                );
                break;

            case APP_EVENT_RESET_COUNT:
                if (
                    counter_sensor_reset()
                    == ESP_OK
                ) {
                    mqtt_publish_sensor_count(
                        0
                    );

#if CONFIG_PNAAT_MATCH_VISION_WITH_SENSOR
                    piece_matcher_reset();
#endif

                    result_indicator_clear();
                }
                break;

            case APP_EVENT_OTA_UPDATE: {
                conveyor_stop();

                mqtt_publish_conveyor_state(
                    false
                );

                esp_err_t ota_err =
                    ota_update_start(
                        event.ota_url
                    );

                if (
                    ota_err != ESP_OK
                ) {
                    ESP_LOGE(
                        TAG,
                        "Não foi possível iniciar OTA: %s",
                        esp_err_to_name(
                            ota_err
                        )
                    );

                    mqtt_publish_ota_status(
                        "START_FAILED",
                        ota_err
                    );
                }

                break;
            }

            case APP_EVENT_MQTT_CONNECTED:
                mqtt_publish_sensor_count(
                    counter_sensor_get_count()
                );

                mqtt_publish_sensor_presence(
                    counter_sensor_is_active()
                );

                mqtt_publish_conveyor_state(
                    conveyor_is_running()
                );

                mqtt_publish_ota_status(
                    "IDLE",
                    ESP_OK
                );
                break;

            default:
                break;
        }
    }
}
