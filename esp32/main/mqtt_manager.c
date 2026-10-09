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
    snprintf(
        out,
        n,
        "%s/%s",
        CONFIG_PNAAT_TOPIC_PREFIX,
        suffix
    );
}

static void publish(
    const char *suffix,
    const char *payload,
    int retain
)
{
    if (!s_connected || s_client == NULL) {
        ESP_LOGD(
            TAG,
            "MQTT desconectado; publicação %s ignorada.",
            suffix
        );
        return;
    }

    char topic[128];
    make_topic(topic, sizeof(topic), suffix);

    int msg_id = esp_mqtt_client_publish(
        s_client,
        topic,
        payload,
        0,
        1,
        retain
    );

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Falha ao publicar em %s.", topic);
    }
}

static bool topic_is(
    esp_mqtt_event_handle_t e,
    const char *suffix
)
{
    char expected[128];
    make_topic(expected, sizeof(expected), suffix);

    const size_t n = strlen(expected);

    return e->topic_len == (int)n &&
        strncmp(e->topic, expected, n) == 0;
}

static void queue_simple(app_event_type_t type)
{
    app_event_t e = {
        .type = type,
        .vision_confidence = -1.0f,
    };

    if (xQueueSend(s_queue, &e, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila cheia; evento descartado.");
    }
}

static pnaat_piece_result_t parse_result_text(
    const char *text
)
{
    if (text == NULL) {
        return PNAAT_PIECE_RESULT_UNKNOWN;
    }

    if (
        !strcasecmp(text, "APROVADA") ||
        !strcasecmp(text, "APROVADO") ||
        !strcasecmp(text, "OK")
    ) {
        return PNAAT_PIECE_RESULT_APPROVED;
    }

    if (
        !strcasecmp(text, "REJEITADA") ||
        !strcasecmp(text, "REJEITADO") ||
        !strcasecmp(text, "NOK")
    ) {
        return PNAAT_PIECE_RESULT_REJECTED;
    }

    return PNAAT_PIECE_RESULT_UNKNOWN;
}

static const char *result_to_text(
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

static void handle_command(const char *payload)
{
    cJSON *root = cJSON_Parse(payload);

    if (root == NULL) {
        ESP_LOGW(TAG, "JSON inválido em esteira/comando");
        return;
    }

    cJSON *cmd =
        cJSON_GetObjectItemCaseSensitive(root, "cmd");

    if (!cJSON_IsString(cmd)) {
        cJSON_Delete(root);
        return;
    }

    if (!strcasecmp(cmd->valuestring, "START")) {
        queue_simple(APP_EVENT_START);
    } else if (!strcasecmp(cmd->valuestring, "STOP")) {
        queue_simple(APP_EVENT_STOP);
    } else if (
        !strcasecmp(
            cmd->valuestring,
            "RESET_CONTAGEM_SENSOR"
        )
    ) {
        queue_simple(APP_EVENT_RESET_COUNT);
    } else if (
        !strcasecmp(cmd->valuestring, "OTA_UPDATE")
    ) {
#if CONFIG_PNAAT_OTA_ENABLED
        cJSON *url =
            cJSON_GetObjectItemCaseSensitive(root, "url");

        if (
            !cJSON_IsString(url) ||
            url->valuestring[0] == '\0'
        ) {
            ESP_LOGW(
                TAG,
                "OTA_UPDATE sem URL válida."
            );
        } else {
            app_event_t event = {
                .type = APP_EVENT_OTA_UPDATE,
                .vision_confidence = -1.0f,
            };

            strlcpy(
                event.ota_url,
                url->valuestring,
                sizeof(event.ota_url)
            );

            if (
                xQueueSend(
                    s_queue,
                    &event,
                    0
                ) != pdTRUE
            ) {
                ESP_LOGW(
                    TAG,
                    "Fila cheia; OTA_UPDATE descartado."
                );
            }
        }
#else
        ESP_LOGW(
            TAG,
            "OTA desabilitado no menuconfig."
        );
#endif
    } else {
        ESP_LOGW(
            TAG,
            "Comando desconhecido: %s",
            cmd->valuestring
        );
    }

    cJSON_Delete(root);
}

static void handle_vision_result(const char *payload)
{
    cJSON *root = cJSON_Parse(payload);

    if (root == NULL) {
        ESP_LOGW(
            TAG,
            "JSON inválido em visao/resultado."
        );
        return;
    }

    cJSON *result =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "resultado"
        );

    if (!cJSON_IsString(result)) {
        result =
            cJSON_GetObjectItemCaseSensitive(
                root,
                "classificacao"
            );
    }

    if (!cJSON_IsString(result)) {
        ESP_LOGW(
            TAG,
            "visao/resultado sem resultado/classificacao."
        );
        cJSON_Delete(root);
        return;
    }

    const pnaat_piece_result_t parsed_result =
        parse_result_text(result->valuestring);

    if (
        parsed_result ==
        PNAAT_PIECE_RESULT_UNKNOWN
    ) {
        ESP_LOGW(
            TAG,
            "Resultado da visão ainda não é final: %s",
            result->valuestring
        );
        cJSON_Delete(root);
        return;
    }

    app_event_t event = {
        .type = APP_EVENT_VISION_RESULT,
        .vision_result = parsed_result,
        .vision_confidence = -1.0f,
    };

    cJSON *id_camera =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "id_camera"
        );

    if (!cJSON_IsNumber(id_camera)) {
        id_camera =
            cJSON_GetObjectItemCaseSensitive(
                root,
                "count"
            );
    }

    if (
        cJSON_IsNumber(id_camera) &&
        id_camera->valuedouble >= 0
    ) {
        event.vision_id =
            (uint32_t)id_camera->valuedouble;
    }

    cJSON *confidence =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "confianca"
        );

    if (!cJSON_IsNumber(confidence)) {
        confidence =
            cJSON_GetObjectItemCaseSensitive(
                root,
                "confidence"
            );
    }

    if (cJSON_IsNumber(confidence)) {
        event.vision_confidence =
            (float)confidence->valuedouble;
    }

    if (
        xQueueSend(
            s_queue,
            &event,
            0
        ) != pdTRUE
    ) {
        ESP_LOGW(
            TAG,
            "Fila cheia; resultado da visão descartado."
        );
    }

    cJSON_Delete(root);
}

static void handle_mqtt_data(
    esp_mqtt_event_handle_t e
)
{
    char payload[512];
    int n = e->data_len;

    if (n >= (int)sizeof(payload)) {
        ESP_LOGW(
            TAG,
            "Payload MQTT grande demais (%d bytes).",
            e->data_len
        );
        n = sizeof(payload) - 1;
    }

    memcpy(payload, e->data, n);
    payload[n] = '\0';

    if (topic_is(e, "esteira/comando")) {
        handle_command(payload);
        return;
    }

    if (topic_is(e, "visao/resultado")) {
        handle_vision_result(payload);
    }
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

    esp_mqtt_event_handle_t e =
        (esp_mqtt_event_handle_t)event_data;

    if (event_id == MQTT_EVENT_CONNECTED) {
        s_connected = true;

        char command_topic[128];
        char vision_topic[128];

        make_topic(
            command_topic,
            sizeof(command_topic),
            "esteira/comando"
        );

        make_topic(
            vision_topic,
            sizeof(vision_topic),
            "visao/resultado"
        );

        esp_mqtt_client_subscribe(
            s_client,
            command_topic,
            1
        );

        esp_mqtt_client_subscribe(
            s_client,
            vision_topic,
            1
        );

        ESP_LOGI(
            TAG,
            "Inscrito em %s",
            command_topic
        );

        ESP_LOGI(
            TAG,
            "Inscrito em %s",
            vision_topic
        );

        publish(
            "sistema/status",
            "online",
            1
        );

        queue_simple(
            APP_EVENT_MQTT_CONNECTED
        );

        return;
    }

    if (
        event_id ==
        MQTT_EVENT_DISCONNECTED
    ) {
        s_connected = false;
        ESP_LOGW(TAG, "MQTT desconectado.");
        return;
    }

    if (event_id == MQTT_EVENT_ERROR) {
        ESP_LOGW(TAG, "Evento de erro MQTT.");
        return;
    }

    if (event_id == MQTT_EVENT_DATA) {
        handle_mqtt_data(e);
    }
}

esp_err_t mqtt_manager_init(
    QueueHandle_t queue
)
{
    if (queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_queue = queue;

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri =
            CONFIG_PNAAT_MQTT_URI,
        .credentials.username =
            CONFIG_PNAAT_MQTT_USERNAME,
        .credentials.authentication.password =
            CONFIG_PNAAT_MQTT_PASSWORD,
        .session.last_will.topic =
            CONFIG_PNAAT_TOPIC_PREFIX
            "/sistema/status",
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
    };

    s_client =
        esp_mqtt_client_init(&cfg);

    if (s_client == NULL) {
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

    return esp_mqtt_client_start(
        s_client
    );
}

void mqtt_publish_sensor_count(
    uint32_t count
)
{
    cJSON *root =
        cJSON_CreateObject();

    if (root == NULL) {
        return;
    }

    cJSON_AddNumberToObject(
        root,
        "count",
        count
    );

    cJSON_AddStringToObject(
        root,
        "source",
        "sensor_final"
    );

    char *text =
        cJSON_PrintUnformatted(root);

    if (text != NULL) {
        publish(
            "contagem/sensor",
            text,
            1
        );
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

void mqtt_publish_sensor_presence(
    bool present
)
{
    cJSON *root =
        cJSON_CreateObject();

    if (root == NULL) {
        return;
    }

    cJSON_AddBoolToObject(
        root,
        "presente",
        present
    );

    cJSON_AddStringToObject(
        root,
        "source",
        "sensor_final"
    );

    char *text =
        cJSON_PrintUnformatted(root);

    if (text != NULL) {
        publish(
            "sensor/presenca",
            text,
            1
        );
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

void mqtt_publish_conveyor_state(
    bool running
)
{
    cJSON *root =
        cJSON_CreateObject();

    if (root == NULL) {
        return;
    }

    cJSON_AddStringToObject(
        root,
        "estado",
        running
            ? "RUNNING"
            : "STOPPED"
    );

    char *text =
        cJSON_PrintUnformatted(root);

    if (text != NULL) {
        publish(
            "esteira/estado",
            text,
            1
        );
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

void mqtt_publish_piece_match(
    uint32_t sensor_count,
    uint32_t camera_id,
    pnaat_piece_result_t result,
    float confidence
)
{
    cJSON *root =
        cJSON_CreateObject();

    if (root == NULL) {
        return;
    }

    cJSON_AddNumberToObject(
        root,
        "sensor_count",
        sensor_count
    );

    cJSON_AddNumberToObject(
        root,
        "id_camera",
        camera_id
    );

    cJSON_AddStringToObject(
        root,
        "resultado",
        result_to_text(result)
    );

    if (confidence >= 0.0f) {
        cJSON_AddNumberToObject(
            root,
            "confianca",
            confidence
        );
    }

    cJSON_AddStringToObject(
        root,
        "source",
        "esp32_matcher"
    );

    char *text =
        cJSON_PrintUnformatted(root);

    if (text != NULL) {
        publish(
            "peca/resultado_final",
            text,
            0
        );
        cJSON_free(text);
    }

    cJSON_Delete(root);
}

void mqtt_publish_ota_status(
    const char *status,
    esp_err_t error_code
)
{
    cJSON *root =
        cJSON_CreateObject();

    if (root == NULL) {
        return;
    }

    cJSON_AddStringToObject(
        root,
        "status",
        status != NULL
            ? status
            : "UNKNOWN"
    );

    cJSON_AddNumberToObject(
        root,
        "error_code",
        (double)error_code
    );

    if (error_code != ESP_OK) {
        cJSON_AddStringToObject(
            root,
            "error",
            esp_err_to_name(
                error_code
            )
        );
    }

    char *text =
        cJSON_PrintUnformatted(root);

    if (text != NULL) {
        publish(
            "ota/status",
            text,
            1
        );
        cJSON_free(text);
    }

    cJSON_Delete(root);
}
