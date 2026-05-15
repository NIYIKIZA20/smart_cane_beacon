#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "beacon.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/*
 * =====================================================================
 * BEACON MAIN LOOP
 *
 * Sequence on boot:
 *   1. Init LED, sensors, BLE advertiser
 *   2. Read sensors once immediately
 *   3. Start advertising with live data
 *   4. Every SENSOR_READ_INTERVAL_MS:
 *        a. Read sensors
 *        b. Update advertisement payload
 *        c. Blink LED to confirm cycle
 *
 * The cane scans passively and parses the manufacturer-specific
 * advertisement data.  No BLE connection is ever formed.
 * =====================================================================
 */

int main(void)
{
    LOG_INF("=== Smart Cane BEACON booting ===");
    LOG_INF("Location: \"%s\"", BEACON_LOCATION);

    /* ── Peripheral init ── */
    led_init();

    if (sensors_init() != 0) {
        LOG_WRN("One or more sensors failed init — continuing anyway");
    }

    if (advertiser_init() != 0) {
        LOG_ERR("BLE advertiser init failed — halting");
        return -1;
    }

    /* ── First sensor read before advertising starts ── */
    sensors_read();

    /* ── Start advertising ── */
    if (advertiser_update() != 0) {
        LOG_ERR("Failed to start advertising — halting");
        return -1;
    }

    /* Startup blink: 3 quick flashes to confirm boot */
    for (int i = 0; i < 3; i++) {
        led_blink_once();
        k_msleep(150);
    }

    LOG_INF("Beacon running — advertising every ~%d ms, "
            "sensor update every %d ms",
            ADV_INTERVAL_MS, SENSOR_READ_INTERVAL_MS);

    /* ── Main loop ── */
    while (1) {
        /* Sleep for sensor update interval */
        k_msleep(SENSOR_READ_INTERVAL_MS);

        /* Read fresh sensor data */
        sensors_read();

        /* Rebuild and restart advertisement with updated values */
        advertiser_update();

        /* Blink LED once to show the cycle completed */
        led_blink_once();

        LOG_DBG("Cycle complete — T=%d°C  H=%d%%",
                g_sensors.temperature,
                g_sensors.humidity);
    }

    return 0;
}