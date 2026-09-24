#include "conveyor.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "conveyor";
static bool s_running;

#if CONFIG_PNAAT_MOTOR_ENABLED
static int level_for(bool running)
{
    const int active = CONFIG_PNAAT_MOTOR_RUN_ACTIVE_LEVEL;
    return running ? active : !active;
}
#endif

esp_err_t conveyor_init(void)
{
#if CONFIG_PNAAT_MOTOR_ENABLED
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << CONFIG_PNAAT_MOTOR_RUN_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(CONFIG_PNAAT_MOTOR_RUN_GPIO, level_for(false));
#else
    ESP_LOGW(TAG, "Controle de motor desabilitado no menuconfig.");
#endif
    s_running = false;
    return ESP_OK;
}

esp_err_t conveyor_start(void)
{
#if CONFIG_PNAAT_MOTOR_ENABLED
    gpio_set_level(CONFIG_PNAAT_MOTOR_RUN_GPIO, level_for(true));
#endif
    s_running = true;
    ESP_LOGI(TAG, "RUN");
    return ESP_OK;
}

esp_err_t conveyor_stop(void)
{
#if CONFIG_PNAAT_MOTOR_ENABLED
    gpio_set_level(CONFIG_PNAAT_MOTOR_RUN_GPIO, level_for(false));
#endif
    s_running = false;
    ESP_LOGI(TAG, "STOP");
    return ESP_OK;
}

bool conveyor_is_running(void)
{
    return s_running;
}
