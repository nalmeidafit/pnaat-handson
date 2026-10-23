#include "counter_sensor.h"
#include "app_events.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sdkconfig.h"

/*
 * Opções bool desabilitadas pelo Kconfig podem não ser geradas como
 * macros em sdkconfig.h. Para o hardware PNAAT, pull-up interno desligado
 * é o comportamento esperado; portanto, trate a ausência da opção como 0.
 */
#ifndef CONFIG_PNAAT_COUNTER_SENSOR_PULLUP
#define CONFIG_PNAAT_COUNTER_SENSOR_PULLUP 0
#endif

static const char *TAG = "counter";
static QueueHandle_t s_queue;
static uint32_t s_count;

#if CONFIG_PNAAT_COUNTER_SENSOR_ENABLED
static bool s_raw;
static bool s_stable;
static int64_t s_changed_us;
#endif

static esp_err_t save_count(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("pnaat", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u32(h, "sensor_count", s_count);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }

    nvs_close(h);
    return err;
}

static void load_count(void)
{
    nvs_handle_t h;

    if (nvs_open("pnaat", NVS_READONLY, &h) != ESP_OK) {
        s_count = 0;
        return;
    }

    if (nvs_get_u32(h, "sensor_count", &s_count) != ESP_OK) {
        s_count = 0;
    }

    nvs_close(h);
}

#if CONFIG_PNAAT_COUNTER_SENSOR_ENABLED
static bool is_active(void)
{
    return gpio_get_level(CONFIG_PNAAT_COUNTER_SENSOR_GPIO)
        == CONFIG_PNAAT_COUNTER_SENSOR_ACTIVE_LEVEL;
}

static void queue_sensor_state(bool active)
{
    app_event_t event = {
        .type = APP_EVENT_SENSOR_STATE,
        .sensor_active = active,
    };

    if (xQueueSend(s_queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila cheia; estado do sensor descartado.");
    }
}

static void queue_sensor_count(void)
{
    app_event_t event = {
        .type = APP_EVENT_SENSOR_COUNT,
        .count = s_count,
        .sensor_active = true,
    };

    if (xQueueSend(s_queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila cheia; contagem física descartada.");
    }
}

static void task_counter(void *arg)
{
    (void)arg;

    const int64_t debounce_us =
        (int64_t)CONFIG_PNAAT_COUNTER_SENSOR_DEBOUNCE_MS * 1000LL;

    while (true) {
        const bool raw = is_active();
        const int64_t now = esp_timer_get_time();

        if (raw != s_raw) {
            const int level =
                gpio_get_level(
                    CONFIG_PNAAT_COUNTER_SENSOR_GPIO
                );

            ESP_LOGI(
                TAG,
                "Transição elétrica GPIO%d: nível=%d -> %s",
                CONFIG_PNAAT_COUNTER_SENSOR_GPIO,
                level,
                raw ? "ATIVO" : "INATIVO"
            );

            s_raw = raw;
            s_changed_us = now;
        }

        if (
            s_stable != s_raw &&
            (now - s_changed_us) >= debounce_us
        ) {
            const bool previous = s_stable;
            s_stable = s_raw;

            ESP_LOGI(
                TAG,
                "Sensor final: %s",
                s_stable ? "PEÇA PRESENTE" : "LIVRE"
            );

            queue_sensor_state(s_stable);

            if (!previous && s_stable) {
                ++s_count;

                esp_err_t err = save_count();
                if (err != ESP_OK) {
                    ESP_LOGW(
                        TAG,
                        "Falha ao salvar contagem: %s",
                        esp_err_to_name(err)
                    );
                }

                ESP_LOGI(
                    TAG,
                    "Contagem física: %lu",
                    (unsigned long)s_count
                );

                queue_sensor_count();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif

esp_err_t counter_sensor_init(QueueHandle_t queue)
{
    s_queue = queue;
    load_count();

    ESP_LOGI(
        TAG,
        "Contagem restaurada: %lu",
        (unsigned long)s_count
    );

#if CONFIG_PNAAT_COUNTER_SENSOR_ENABLED
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << CONFIG_PNAAT_COUNTER_SENSOR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = CONFIG_PNAAT_COUNTER_SENSOR_PULLUP
            ? GPIO_PULLUP_ENABLE
            : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&cfg));

    const int initial_level =
        gpio_get_level(
            CONFIG_PNAAT_COUNTER_SENSOR_GPIO
        );

    s_raw = is_active();
    s_stable = s_raw;
    s_changed_us = esp_timer_get_time();

    ESP_LOGI(
        TAG,
        "Sensor habilitado: GPIO%d, nível ativo=%d, pull-up=%s, debounce=%d ms",
        CONFIG_PNAAT_COUNTER_SENSOR_GPIO,
        CONFIG_PNAAT_COUNTER_SENSOR_ACTIVE_LEVEL,
        CONFIG_PNAAT_COUNTER_SENSOR_PULLUP
            ? "SIM"
            : "NAO",
        CONFIG_PNAAT_COUNTER_SENSOR_DEBOUNCE_MS
    );

    ESP_LOGI(
        TAG,
        "Estado elétrico inicial GPIO%d: nível=%d -> %s",
        CONFIG_PNAAT_COUNTER_SENSOR_GPIO,
        initial_level,
        s_raw ? "ATIVO" : "INATIVO"
    );

    return xTaskCreate(
        task_counter,
        "counter_task",
        4096,
        NULL,
        10,
        NULL
    ) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
#else
    ESP_LOGW(TAG, "Sensor desabilitado no menuconfig.");
    return ESP_OK;
#endif
}

uint32_t counter_sensor_get_count(void)
{
    return s_count;
}

bool counter_sensor_is_active(void)
{
#if CONFIG_PNAAT_COUNTER_SENSOR_ENABLED
    return s_stable;
#else
    return false;
#endif
}

esp_err_t counter_sensor_reset(void)
{
    s_count = 0;
    return save_count();
}
