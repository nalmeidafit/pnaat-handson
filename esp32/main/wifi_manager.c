#include "wifi_manager.h"

#include <string.h>

#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

static const char *TAG = "wifi";
static QueueHandle_t s_queue;

static void queue_event(app_event_type_t type)
{
    if (s_queue == NULL) {
        return;
    }

    app_event_t event = {
        .type = type,
        .vision_confidence = -1.0f,
    };

    if (
        xQueueSend(
            s_queue,
            &event,
            0
        ) != pdTRUE
    ) {
        ESP_LOGW(
            TAG,
            "Fila principal cheia; evento Wi-Fi descartado."
        );
    }
}

static void handler(
    void *arg, esp_event_base_t base, int32_t id, void *data
)
{
    (void)arg;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Desconectado; reconectando...");

        queue_event(
            APP_EVENT_WIFI_DISCONNECTED
        );

        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;

        ESP_LOGI(
            TAG,
            "IP: " IPSTR,
            IP2STR(&e->ip_info.ip)
        );

        queue_event(
            APP_EVENT_WIFI_GOT_IP
        );
    }
}

esp_err_t wifi_manager_init(QueueHandle_t queue)
{
    if (queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_queue = queue;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, handler, NULL
        )
    );
    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, handler, NULL
        )
    );

    wifi_config_t cfg = {0};

    strlcpy(
        (char *)cfg.sta.ssid,
        CONFIG_PNAAT_WIFI_SSID,
        sizeof(cfg.sta.ssid)
    );
    strlcpy(
        (char *)cfg.sta.password,
        CONFIG_PNAAT_WIFI_PASSWORD,
        sizeof(cfg.sta.password)
    );

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}
