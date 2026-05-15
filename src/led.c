#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "beacon.h"

LOG_MODULE_REGISTER(led, LOG_LEVEL_DBG);

/*
 * Status LED — P1.06, active HIGH, 1 kΩ series resistor R1.
 * Blinks once every advertisement cycle to show the beacon is alive.
 */
static const struct gpio_dt_spec led =
    GPIO_DT_SPEC_GET(DT_NODELABEL(beacon_led), gpios);

void led_init(void)
{
    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("Beacon LED GPIO not ready");
        return;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    LOG_INF("Beacon LED ready (P1.06)");
}

void led_blink_once(void)
{
    gpio_pin_set_dt(&led, 1);
    k_msleep(LED_ON_MS);
    gpio_pin_set_dt(&led, 0);
}