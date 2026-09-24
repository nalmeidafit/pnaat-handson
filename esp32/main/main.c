#include "app_events.h"
#include "conveyor.h"
#include "counter_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_manager.h"
#include "nvs_flash.h"
#include "ota_update.h"
#include "wifi_manager.h"

static const char *TAG = "pnaat";

void app_main(void)
{
    ESP_LOGI(TAG, "PNAAT - contador físico + esteira + OTA de desenvolvimento");
    ESP_LOGI(TAG, "ESP-IDF v5.5.2 / target ESP32-S3");

    esp_err_t err = nvs_flash_init();

    if (
        err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    QueueHandle_t queue = xQueueCreate(16, sizeof(app_event_t));
    if (queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar fila principal.");
        return;
    }

    ESP_ERROR_CHECK(conveyor_init());
    ESP_ERROR_CHECK(counter_sensor_init(queue));
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(mqtt_manager_init(queue));

    while (true) {
        app_event_t event;

        if (xQueueReceive(queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
            case APP_EVENT_SENSOR_COUNT:
                mqtt_publish_sensor_count(event.count);
                break;

            case APP_EVENT_START:
                if (!ota_update_is_running()) {
                    conveyor_start();
                    mqtt_publish_conveyor_state(true);
                }
                break;

            case APP_EVENT_STOP:
                conveyor_stop();
                mqtt_publish_conveyor_state(false);
                break;

            case APP_EVENT_RESET_COUNT:
                if (counter_sensor_reset() == ESP_OK) {
                    mqtt_publish_sensor_count(0);
                }
                break;

            case APP_EVENT_OTA_UPDATE: {
                /* Desenvolvimento: nunca atualiza firmware com a esteira rodando. */
                conveyor_stop();
                mqtt_publish_conveyor_state(false);

                esp_err_t ota_err = ota_update_start(event.ota_url);
                if (ota_err != ESP_OK) {
                    ESP_LOGE(
                        TAG,
                        "Não foi possível iniciar OTA: %s",
                        esp_err_to_name(ota_err)
                    );
                    mqtt_publish_ota_status("START_FAILED", ota_err);
                }
                break;
            }

            case APP_EVENT_MQTT_CONNECTED:
                mqtt_publish_sensor_count(counter_sensor_get_count());
                mqtt_publish_conveyor_state(conveyor_is_running());
                mqtt_publish_ota_status("IDLE", ESP_OK);
                break;

            default:
                break;
        }
    }
}
