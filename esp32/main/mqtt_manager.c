#include "mqtt_manager.h"
#include "app_events.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "sdkconfig.h"

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t s_client;
static QueueHandle_t s_queue;
static bool s_connected;

static void make_topic(char *out, size_t n, const char *suffix)
{
    snprintf(out, n, "%s/%s", CONFIG_PNAAT_TOPIC_PREFIX, suffix);
}

static void publish(const char *suffix, const char *payload, int retain)
{
    if (!s_connected || !s_client) {
        return;
    }

    char topic[128];
    make_topic(topic, sizeof(topic), suffix);
    esp_mqtt_client_publish(s_client, topic, payload, 0, 1, retain);
}

static bool topic_is(esp_mqtt_event_handle_t e, const char *suffix)
{
    char expected[128];
    make_topic(expected, sizeof(expected), suffix);
    size_t n = strlen(expected);

    return e->topic_len == (int)n && strncmp(e->topic, expected, n) == 0;
}

static void queue_simple(app_event_type_t type)
{
    app_event_t e = {
        .type = type,
        .count = 0,
        .ota_url = {0},
    };

    if (xQueueSend(s_queue, &e, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila cheia; evento descartado.");
    }
}

static void handle_command(const char *payload)
{
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        ESP_LOGW(TAG, "JSON inválido em esteira/comando");
        return;
    }

    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");

    if (!cJSON_IsString(cmd)) {
        cJSON_Delete(root);
        return;
    }

    if (!strcasecmp(cmd->valuestring, "START")) {
        queue_simple(APP_EVENT_START);
    } else if (!strcasecmp(cmd->valuestring, "STOP")) {
        queue_simple(APP_EVENT_STOP);
    } else if (!strcasecmp(cmd->valuestring, "RESET_CONTAGEM_SENSOR")) {
        queue_simple(APP_EVENT_RESET_COUNT);
    } else if (!strcasecmp(cmd->valuestring, "OTA_UPDATE")) {
#if CONFIG_PNAAT_OTA_ENABLED
        cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");

        if (!cJSON_IsString(url) || url->valuestring[0] == '\0') {
            ESP_LOGW(TAG, "OTA_UPDATE sem URL válida.");
        } else {
            app_event_t event = {
                .type = APP_EVENT_OTA_UPDATE,
                .count = 0,
                .ota_url = {0},
            };

            strlcpy(event.ota_url, url->valuestring, sizeof(event.ota_url));

            if (xQueueSend(s_queue, &event, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Fila cheia; OTA_UPDATE descartado.");
            }
        }
#else
        ESP_LOGW(TAG, "OTA desabilitado no menuconfig.");
#endif
    } else {
        ESP_LOGW(TAG, "Comando desconhecido: %s", cmd->valuestring);
    }

    cJSON_Delete(root);
}

static void mqtt_handler(
    void *args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    (void)args;
    (void)base;

    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)event_data;

    if (event_id == MQTT_EVENT_CONNECTED) {
        s_connected = true;

        char topic[128];
        make_topic(topic, sizeof(topic), "esteira/comando");
        esp_mqtt_client_subscribe(s_client, topic, 1);

        publish("sistema/status", "online", 1);
        queue_simple(APP_EVENT_MQTT_CONNECTED);
        return;
    }

    if (event_id == MQTT_EVENT_DISCONNECTED) {
        s_connected = false;
        return;
    }

    if (event_id == MQTT_EVENT_DATA && topic_is(e, "esteira/comando")) {
        char payload[384];
        int n = e->data_len;

        if (n >= (int)sizeof(payload)) {
            n = sizeof(payload) - 1;
        }

        memcpy(payload, e->data, n);
        payload[n] = '\0';
        handle_command(payload);
    }
}

esp_err_t mqtt_manager_init(QueueHandle_t queue)
{
    s_queue = queue;

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_PNAAT_MQTT_URI,
        .credentials.username = CONFIG_PNAAT_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_PNAAT_MQTT_PASSWORD,
        .session.last_will.topic =
            CONFIG_PNAAT_TOPIC_PREFIX "/sistema/status",
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            s_client,
            ESP_EVENT_ANY_ID,
            mqtt_handler,
            NULL
        )
    );

    return esp_mqtt_client_start(s_client);
}

void mqtt_publish_sensor_count(uint32_t count)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "count", count);
    cJSON_AddStringToObject(root, "source", "sensor_final");

    char *text = cJSON_PrintUnformatted(root);
    if (text) {
        publish("contagem/sensor", text, 1);
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

void mqtt_publish_conveyor_state(bool running)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(
        root,
        "estado",
        running ? "RUNNING" : "STOPPED"
    );

    char *text = cJSON_PrintUnformatted(root);
    if (text) {
        publish("esteira/estado", text, 1);
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

void mqtt_publish_ota_status(const char *status, esp_err_t error_code)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", status ? status : "UNKNOWN");
    cJSON_AddNumberToObject(root, "error_code", (double)error_code);

    if (error_code != ESP_OK) {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(error_code));
    }

    char *text = cJSON_PrintUnformatted(root);
    if (text) {
        publish("ota/status", text, 1);
        cJSON_free(text);
    }

    cJSON_Delete(root);
}
