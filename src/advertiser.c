#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "beacon.h"

LOG_MODULE_REGISTER(advertiser, LOG_LEVEL_INF);

/*
 * Manufacturer-specific payload layout:
 *
 *  [0-1]  Company ID  0xFF 0xFF  (little-endian)
 *  [2]    Tag         0xBE
 *  [3]    Temperature int8_t °C
 *  [4]    Humidity    uint8_t %RH
 *  [5]    PIR         0x00 (no PIR on beacon hardware)
 *  [6-N]  Location    null-terminated string
 *
 * This matches exactly what ble_scanner.c in the cane parses.
 */

#define MFR_HEADER_LEN   6                          /* bytes before location */
#define LOCATION_LEN     sizeof(BEACON_LOCATION)    /* includes null term     */
#define MFR_DATA_LEN     (MFR_HEADER_LEN + LOCATION_LEN)

/* Static buffer — updated in advertiser_update() before each adv restart */
static uint8_t mfr_data[MFR_DATA_LEN];

static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  BT_LE_AD_NO_BREDR | BT_LE_AD_GENERAL),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, mfr_data, MFR_DATA_LEN),
};

/* Advertisement parameters — non-connectable, non-scannable (pure beacon) */
static const struct bt_le_adv_param adv_param =
    BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_USE_IDENTITY,            /* use public address */
        BT_GAP_ADV_SLOW_INT_MIN,               /* ~1 s interval min  */
        BT_GAP_ADV_SLOW_INT_MAX,               /* ~1 s interval max  */
        NULL                                   /* no peer (broadcast)*/
    );

/* ── Init ─────────────────────────────────────────────────────────────────── */
int advertiser_init(void)
{
    int rc = bt_enable(NULL);
    if (rc && rc != -EALREADY) {
        LOG_ERR("BT enable failed: %d", rc);
        return rc;
    }

    /* Pre-fill static fields */
    mfr_data[0] = (uint8_t)(BEACON_COMPANY_ID & 0xFF);   /* LSB first */
    mfr_data[1] = (uint8_t)(BEACON_COMPANY_ID >> 8);
    mfr_data[2] = BEACON_TAG;
    mfr_data[3] = 0;    /* temperature — filled by advertiser_update() */
    mfr_data[4] = 0;    /* humidity    — filled by advertiser_update() */
    mfr_data[5] = 0;    /* PIR = 0 (no PIR sensor on beacon)           */
    memcpy(&mfr_data[6], BEACON_LOCATION, LOCATION_LEN);

    LOG_INF("Advertiser ready  location=\"%s\"", BEACON_LOCATION);
    return 0;
}

/* ── Rebuild payload and restart advertising ──────────────────────────────── */
int advertiser_update(void)
{
    /* Stop current advertising (ignore error if not running) */
    bt_le_adv_stop();

    /* Update live sensor values in the manufacturer payload */
    mfr_data[3] = (uint8_t)(int8_t)g_sensors.temperature;
    mfr_data[4] = g_sensors.humidity;

    /* Restart advertising with updated data */
    int rc = bt_le_adv_start(&adv_param,
                             ad, ARRAY_SIZE(ad),
                             NULL, 0);
    if (rc != 0) {
        LOG_ERR("bt_le_adv_start failed: %d", rc);
        return rc;
    }

    LOG_INF("Advertising: T=%d°C  H=%d%%  loc=\"%s\"",
            (int8_t)mfr_data[3],
            mfr_data[4],
            BEACON_LOCATION);

    return 0;
}