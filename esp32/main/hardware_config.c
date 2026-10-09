#include "hardware_config.h"

#include <stddef.h>

#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "hw_config";

typedef struct {
    const char *name;
    int gpio;
} gpio_use_t;

esp_err_t hardware_config_validate(void)
{
    gpio_use_t used[5];
    size_t count = 0;

#if CONFIG_PNAAT_COUNTER_SENSOR_ENABLED
    used[count++] = (gpio_use_t){
        .name = "sensor final",
        .gpio = CONFIG_PNAAT_COUNTER_SENSOR_GPIO,
    };
#endif

#if CONFIG_PNAAT_MOTOR_ENABLED
    used[count++] = (gpio_use_t){
        .name = "RUN/STOP esteira",
        .gpio = CONFIG_PNAAT_MOTOR_RUN_GPIO,
    };
#endif

#if CONFIG_PNAAT_RESULT_LED_ENABLED
    used[count++] = (gpio_use_t){
        .name = "LED APROVADA",
        .gpio = CONFIG_PNAAT_RESULT_LED_GOOD_GPIO,
    };

    used[count++] = (gpio_use_t){
        .name = "LED REJEITADA",
        .gpio = CONFIG_PNAAT_RESULT_LED_BAD_GPIO,
    };
#endif

    for (size_t i = 0; i < count; ++i) {
        ESP_LOGI(
            TAG,
            "%s -> GPIO%d",
            used[i].name,
            used[i].gpio
        );

        for (size_t j = i + 1; j < count; ++j) {
            if (used[i].gpio == used[j].gpio) {
                ESP_LOGE(
                    TAG,
                    "Conflito de GPIO: %s e %s usam GPIO%d.",
                    used[i].name,
                    used[j].name,
                    used[i].gpio
                );

                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    return ESP_OK;
}
