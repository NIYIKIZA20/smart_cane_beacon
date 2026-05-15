#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "beacon.h"

LOG_MODULE_REGISTER(sensors, LOG_LEVEL_INF);

beacon_sensors_t g_sensors = {0};

/* BME280 — via Zephyr sensor API */
static const struct device *bme280_dev;

/*
 * TMP117 — raw I2C (no Zephyr driver in NCS v3.2.4).
 * We get the I2C bus device and address from DT directly.
 *
 * TMP117 register map (relevant):
 *   0x00 = TEMP_RESULT  — 16-bit two's complement, 1 LSB = 0.0078125 °C
 *   0x01 = CONFIGURATION
 *   0x0F = DEVICE_ID    — reads 0x0117 on genuine TMP117
 */
#define TMP117_ADDR          0x48
#define TMP117_REG_TEMP      0x00
#define TMP117_REG_DEVICE_ID 0x0F
#define TMP117_DEVICE_ID     0x0117

static const struct device *i2c_bus;
static bool tmp117_ok = false;

/* ── TMP117 raw I2C helpers ─────────────────────────────────────────────── */
static int tmp117_read_reg16(uint8_t reg, uint16_t *val)
{
    uint8_t buf[2];
    int rc = i2c_write_read(i2c_bus,
                            TMP117_ADDR,
                            &reg, 1,
                            buf, 2);
    if (rc == 0) {
        *val = ((uint16_t)buf[0] << 8) | buf[1];
    }
    return rc;
}

static int tmp117_read_temp_c(int8_t *temp_out)
{
    uint16_t raw;
    int rc = tmp117_read_reg16(TMP117_REG_TEMP, &raw);
    if (rc != 0) return rc;

    /*
     * TMP117 result: 16-bit two's complement, resolution 7.8125 m°C/LSB.
     * Integer approximation: divide by 128 to get °C (1/128 ≈ 7.8125e-3).
     */
    int16_t signed_raw = (int16_t)raw;
    *temp_out = (int8_t)(signed_raw / 128);
    return 0;
}

/* ── Init ─────────────────────────────────────────────────────────────────── */
int sensors_init(void)
{
    int rc = 0;

    /* BME280 via sensor API */
    bme280_dev = DEVICE_DT_GET(DT_NODELABEL(bme280_beacon));
    if (!device_is_ready(bme280_dev)) {
        LOG_ERR("BME280 not ready");
        bme280_dev = NULL;
        rc = -ENODEV;
    } else {
        LOG_INF("BME280 ready (0x76)");
    }

    /* TMP117 via raw I2C — get the bus from DT */
    i2c_bus = DEVICE_DT_GET(DT_NODELABEL(i2c21));
    if (!device_is_ready(i2c_bus)) {
        LOG_ERR("I2C21 bus not ready");
        rc = -ENODEV;
    } else {
        /* Verify TMP117 responds with correct device ID */
        uint16_t dev_id = 0;
        int id_rc = tmp117_read_reg16(TMP117_REG_DEVICE_ID, &dev_id);
        if (id_rc == 0 && dev_id == TMP117_DEVICE_ID) {
            tmp117_ok = true;
            LOG_INF("TMP117 ready (0x48)  ID=0x%04X", dev_id);
        } else {
            LOG_WRN("TMP117 not found or wrong ID=0x%04X rc=%d",
                    dev_id, id_rc);
            /* Don't hard-fail — BME280 alone is sufficient */
        }
    }

    return rc;
}

/* ── Read both sensors, update g_sensors ─────────────────────────────────── */
void sensors_read(void)
{
    int32_t temp_sum   = 0;
    uint8_t temp_count = 0;

    /* ── BME280 ── */
    if (bme280_dev != NULL &&
        sensor_sample_fetch(bme280_dev) == 0) {

        struct sensor_value bme_temp, bme_hum, bme_press;

        sensor_channel_get(bme280_dev, SENSOR_CHAN_AMBIENT_TEMP, &bme_temp);
        sensor_channel_get(bme280_dev, SENSOR_CHAN_HUMIDITY,     &bme_hum);
        sensor_channel_get(bme280_dev, SENSOR_CHAN_PRESS,        &bme_press);

        g_sensors.humidity = (uint8_t)bme_hum.val1;
        g_sensors.pressure = bme_press.val1;

        temp_sum += bme_temp.val1;
        temp_count++;

        LOG_DBG("BME280  T=%d°C  H=%d%%  P=%d Pa",
                bme_temp.val1, bme_hum.val1, bme_press.val1);
    } else {
        LOG_WRN("BME280 read failed");
    }

    /* ── TMP117 (raw I2C) ── */
    if (tmp117_ok) {
        int8_t tmp_c = 0;
        if (tmp117_read_temp_c(&tmp_c) == 0) {
            temp_sum += tmp_c;
            temp_count++;
            LOG_DBG("TMP117  T=%d°C", tmp_c);
        } else {
            LOG_WRN("TMP117 read failed");
        }
    }

    /* Average temperature from all available sensors */
    if (temp_count > 0) {
        g_sensors.temperature = (int8_t)(temp_sum / temp_count);
        g_sensors.valid       = true;
        LOG_INF("Sensors → T=%d°C  H=%d%%  P=%d Pa",
                g_sensors.temperature,
                g_sensors.humidity,
                g_sensors.pressure);
    } else {
        LOG_ERR("No valid sensor readings");
        g_sensors.valid = false;
    }
}