#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

int main(void)
{
    const struct device *port = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    gpio_pin_configure(port, 6, GPIO_OUTPUT_ACTIVE);

    while (1) {
        gpio_pin_set(port, 6, 1);
        k_msleep(500);
        gpio_pin_set(port, 6, 0);
        k_msleep(500);
    }

    return 0;
}