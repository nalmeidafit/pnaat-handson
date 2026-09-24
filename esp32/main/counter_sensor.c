#include "counter_sensor.h"
#include "app_events.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "counter";
static QueueHandle_t s_queue;
static uint32_t s_count;

static esp_err_t save_count(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("pnaat", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_u32(h, "sensor_count", s_count);
    if (err == ESP_OK) err = nvs_commit(h);
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

static bool s_raw;
static bool s_stable;
static int64_t s_changed_us;

static bool is_active(void)
{
    return gpio_get_level(CONFIG_PNAAT_COUNTER_SENSOR_GPIO)
        == CONFIG_PNAAT_COUNTER_SENSOR_ACTIVE_LEVEL;
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
            s_raw = raw;
            s_changed_us = now;
        }

        if (s_stable != s_raw && (now - s_changed_us) >= debounce_us) {
            const bool previous = s_stable;
            s_stable = s_raw;

            if (!previous && s_stable) {
                ++s_count;
                save_count();

                ESP_LOGI(TAG, "Contagem física: %lu", (unsigned long)s_count);

                app_event_t event = {
                    .type = APP_EVENT_SENSOR_COUNT,
                    .count = s_count
                };
                xQueueSend(s_queue, &event, 0);
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

    ESP_LOGI(TAG, "Contagem restaurada: %lu", (unsigned long)s_count);

#if CONFIG_PNAAT_COUNTER_SENSOR_ENABLED
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << CONFIG_PNAAT_COUNTER_SENSOR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = CONFIG_PNAAT_COUNTER_SENSOR_PULLUP
            ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    s_raw = is_active();
    s_stable = s_raw;
    s_changed_us = esp_timer_get_time();

    return xTaskCreate(
        task_counter, "counter_task", 4096, NULL, 10, NULL
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

esp_err_t counter_sensor_reset(void)
{
    s_count = 0;
    return save_count();
}
