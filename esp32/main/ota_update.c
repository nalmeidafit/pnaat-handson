#include "ota_update.h"

#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_manager.h"
#include "sdkconfig.h"

static const char *TAG = "ota";
static bool s_ota_running = false;

typedef struct {
    char url[256];
} ota_task_context_t;

static void ota_task(void *arg)
{
#if CONFIG_PNAAT_OTA_ENABLED
    ota_task_context_t *ctx = (ota_task_context_t *)arg;

    ESP_LOGW(TAG, "Iniciando OTA: %s", ctx->url);
    mqtt_publish_ota_status("STARTING", ESP_OK);

    esp_http_client_config_t http_config = {
        .url = ctx->url,
        .timeout_ms = 20000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGW(TAG, "OTA concluído. Reiniciando...");
        mqtt_publish_ota_status("SUCCESS_REBOOTING", ESP_OK);
        vTaskDelay(pdMS_TO_TICKS(1000));
        free(ctx);
        esp_restart();
    }

    ESP_LOGE(TAG, "Falha no OTA: %s", esp_err_to_name(ret));
    mqtt_publish_ota_status("FAILED", ret);

    free(ctx);
    s_ota_running = false;
    vTaskDelete(NULL);
#else
    (void)arg;
    vTaskDelete(NULL);
#endif
}

esp_err_t ota_update_start(const char *url)
{
#if !CONFIG_PNAAT_OTA_ENABLED
    (void)url;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_ota_running) {
        return ESP_ERR_INVALID_STATE;
    }

    if (
        strncmp(url, "http://", 7) != 0 &&
        strncmp(url, "https://", 8) != 0
    ) {
        ESP_LOGE(TAG, "URL OTA inválida: %s", url);
        return ESP_ERR_INVALID_ARG;
    }

    ota_task_context_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    strlcpy(ctx->url, url, sizeof(ctx->url));
    s_ota_running = true;

    BaseType_t ok = xTaskCreate(
        ota_task,
        "ota_task",
        CONFIG_PNAAT_OTA_TASK_STACK_SIZE,
        ctx,
        8,
        NULL
    );

    if (ok != pdPASS) {
        s_ota_running = false;
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
#endif
}

bool ota_update_is_running(void)
{
    return s_ota_running;
}
