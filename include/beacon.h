#ifndef BEACON_H
#define BEACON_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * =====================================================================
 * BEACON ADVERTISEMENT PACKET FORMAT
 * =====================================================================
 * Manufacturer-specific AD type (0xFF):
 *
 *  Byte  Field
 *  ────────────────────────────────────────────────────
 *  0     AD length  (total bytes that follow, set by BT stack)
 *  1     AD type    0xFF = manufacturer specific
 *  2-3   Company ID 0xFF 0xFF  (little-endian, 0xFFFF prototype)
 *  4     Tag        0xBE  ("BENO beacon" identifier)
 *  5     Temperature int8_t °C  (average of BME280 + TMP117)
 *  6     Humidity    uint8_t %RH (from BME280)
 *  7     PIR status  0x00 (beacon has no PIR — reserved for future)
 *  8-N   Location    null-terminated string, max 24 chars
 *
 * The cane's ble_scanner.c already parses this exact format.
 * =====================================================================
 */

#define BEACON_COMPANY_ID    0xFFFF
#define BEACON_TAG           0xBE
#define BEACON_LOCATION_MAX  24

/*
 * Location string stored here.
 * Change BEACON_LOCATION to whatever physical location this beacon
 * is installed at.  The cane will announce this string via audio.
 *
 * Examples:
 *   "Main Entrance"
 *   "Corridor B"
 *   "Lift Lobby"
 *   "Classroom 101"
 */
#define BEACON_LOCATION      "Kimihurura"

/* How often to update sensor readings (ms) */
#define SENSOR_READ_INTERVAL_MS    5000

/* BLE advertisement interval (ms) — 500 ms is a good balance */
#define ADV_INTERVAL_MS            500

/* LED blink pattern while advertising */
#define LED_ON_MS     50
#define LED_OFF_MS   (ADV_INTERVAL_MS - LED_ON_MS)

/* ── Shared sensor data ─────────────────────────────────────────────────── */
typedef struct {
    int8_t  temperature;   /* average of BME280 + TMP117, °C */
    uint8_t humidity;      /* from BME280, %RH               */
    int32_t pressure;      /* from BME280, Pa                */
    bool    valid;
} beacon_sensors_t;

extern beacon_sensors_t g_sensors;

/* ── Module prototypes ──────────────────────────────────────────────────── */
int  sensors_init(void);
void sensors_read(void);          /* blocking, updates g_sensors */

int  advertiser_init(void);
int  advertiser_update(void);     /* rebuilds & restarts adv with new data */

void led_init(void);
void led_blink_once(void);        /* short blink to show activity */

#endif /* BEACON_H */