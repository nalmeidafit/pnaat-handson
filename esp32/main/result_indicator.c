#include "result_indicator.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "indicator";

#if CONFIG_PNAAT_RESULT_LED_ENABLED
static int active_level(void)
{
    return CONFIG_PNAAT_RESULT_LED_ACTIVE_LEVEL;
}

static int inactive_level(void)
{
    return !CONFIG_PNAAT_RESULT_LED_ACTIVE_LEVEL;
}

static void set_leds(bool approved, bool rejected)
{
    gpio_set_level(
        CONFIG_PNAAT_RESULT_LED_GOOD_GPIO,
        approved ? active_level() : inactive_level()
    );

    gpio_set_level(
        CONFIG_PNAAT_RESULT_LED_BAD_GPIO,
        rejected ? active_level() : inactive_level()
    );
}
#endif

esp_err_t result_indicator_init(void)
{
#if CONFIG_PNAAT_RESULT_LED_ENABLED
    if (
        CONFIG_PNAAT_RESULT_LED_GOOD_GPIO ==
        CONFIG_PNAAT_RESULT_LED_BAD_GPIO
    ) {
        ESP_LOGE(TAG, "GPIO dos LEDs APROVADA e REJEITADA não podem ser iguais.");
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t cfg = {
        .pin_bit_mask =
            (1ULL << CONFIG_PNAAT_RESULT_LED_GOOD_GPIO) |
            (1ULL << CONFIG_PNAAT_RESULT_LED_BAD_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&cfg));
    set_leds(false, false);

    ESP_LOGI(
        TAG,
        "Indicador habilitado: APROVADA=GPIO%d REJEITADA=GPIO%d",
        CONFIG_PNAAT_RESULT_LED_GOOD_GPIO,
        CONFIG_PNAAT_RESULT_LED_BAD_GPIO
    );
#else
    ESP_LOGW(TAG, "Indicador APROVADA/REJEITADA desabilitado no menuconfig.");
#endif

    return ESP_OK;
}

esp_err_t result_indicator_show(pnaat_piece_result_t result)
{
#if CONFIG_PNAAT_RESULT_LED_ENABLED
    switch (result) {
        case PNAAT_PIECE_RESULT_APPROVED:
            set_leds(true, false);
            ESP_LOGI(TAG, "LED APROVADA ligado.");
            break;

        case PNAAT_PIECE_RESULT_REJECTED:
            set_leds(false, true);
            ESP_LOGI(TAG, "LED REJEITADA ligado.");
            break;

        default:
            set_leds(false, false);
            ESP_LOGW(TAG, "Resultado desconhecido; LEDs desligados.");
            break;
    }
#else
    (void)result;
#endif

    return ESP_OK;
}

esp_err_t result_indicator_clear(void)
{
#if CONFIG_PNAAT_RESULT_LED_ENABLED
    set_leds(false, false);
#endif
    return ESP_OK;
}
