/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/logging/log.h>
#include <sys/_stdint.h>
LOG_MODULE_REGISTER(app_sensors, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <zephyr/drivers/sensor.h>
#include <golioth/stream.h>
#include <zcbor_encode.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "app_sensors.h"
#include "app_settings.h"

#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

static struct golioth_client *client;

/* Add Sensor structs here */
const struct device *vl53_dev = DEVICE_DT_GET(DT_NODELABEL(st_vl53l0x));
const struct device *bme280_dev = DEVICE_DT_GET(DT_NODELABEL(bme280));
const struct device *ccs811_dev = DEVICE_DT_GET(DT_NODELABEL(ccs811));
const struct device *accel_dev = DEVICE_DT_GET(DT_ALIAS(accel0));

struct bme280_sensor_measurement {
	struct sensor_value temperature;
	struct sensor_value pressure;
	struct sensor_value humidity;
};

static struct bme280_sensor_measurement bme280_sm;

/* Callback for LightDB Stream */
static void async_error_handler(struct golioth_client *client,
				const struct golioth_response *response,
				const char *path,
				void *arg)
{
	if (response->status != GOLIOTH_OK) {
		LOG_ERR("Async task failed: %d", response->status);
		return;
	}
}

static void stream_payload(uint8_t* cbor_buf, size_t cbor_size, char* path)
{
	int err;

	/* Stream data to Golioth */
	err = golioth_stream_set_async(client,
				       path,
				       GOLIOTH_CONTENT_TYPE_CBOR,
				       cbor_buf,
				       cbor_size,
				       async_error_handler,
				       NULL);
	if (err) {
		LOG_ERR("Failed to send %s data to Golioth: %d", path, err);
	}
}

void app_sensors_init(void)
{
	LOG_DBG("Initializing VL53 sensor");
	if (!device_is_ready(vl53_dev)) {
		LOG_ERR("Device %s is not ready", vl53_dev->name);
	}

	LOG_DBG("Initializing BME280 sensor");
	if (!device_is_ready(bme280_dev)) {
		LOG_ERR("Device %s is not ready", bme280_dev->name);
	}

	LOG_DBG("Initializing CCS811 sensor");
	if (!device_is_ready(ccs811_dev)) {
		LOG_ERR("Device %s is not ready", ccs811_dev->name);
	}

	LOG_DBG("Initializing LIS2DH sensor");
	if (!device_is_ready(accel_dev)) {
		LOG_ERR("Device %s is not ready", accel_dev->name);
	}
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_sensors_read_and_stream(void)
{
	int err;

	/* Golioth custom hardware for demos */
	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (
		read_and_report_battery(client);
		IF_ENABLED(CONFIG_LIB_OSTENTUS, (
			slide_set(BATTERY_V, get_batt_v_str(), strlen(get_batt_v_str()));
			slide_set(BATTERY_LVL, get_batt_lvl_str(), strlen(get_batt_lvl_str()));
		));
	));

	/* Get VL53X sensor data */
	struct sensor_value distance_value, proximity_value;
	int dist_in_mm = 0;
	int fill_level = -1;

	err = sensor_sample_fetch(vl53_dev);
	if (err) {
		LOG_ERR("Failed to fetch VL53 sensor data: %d", err);
	} else {
		sensor_channel_get(vl53_dev, SENSOR_CHAN_DISTANCE, &distance_value);
		sensor_channel_get(vl53_dev, SENSOR_CHAN_PROX, &proximity_value);
		dist_in_mm = sensor_value_to_milli(&distance_value);

		LOG_DBG("Distance is %d mm", dist_in_mm);
		LOG_DBG("Proximity is %d", proximity_value.val1);

		fill_level = (100 - ((float)dist_in_mm/get_trash_can_heigth_s()) * 100);

		if (fill_level < 0) {
			fill_level = -1;
			LOG_WRN("VL53 sensor out of range");
		} else {
			LOG_DBG("Trash Can fill level = %d%%", fill_level);
		}
	}

	/* Get BME280 sensor data */
	err = sensor_sample_fetch(bme280_dev);
	if (err) {
		LOG_ERR("Error fetching BME280 weather sensor data: %d", err);
	} else {
		sensor_channel_get(bme280_dev, SENSOR_CHAN_AMBIENT_TEMP, &bme280_sm.temperature);
		sensor_channel_get(bme280_dev, SENSOR_CHAN_PRESS, &bme280_sm.pressure);
		sensor_channel_get(bme280_dev, SENSOR_CHAN_HUMIDITY, &bme280_sm.humidity);

		LOG_DBG("BME280: Temperature= %.2f C, Pressure= %.2f kPa, Humidity=%.2f %%RH",
			sensor_value_to_float(&bme280_sm.temperature),
			sensor_value_to_float(&bme280_sm.pressure),
			sensor_value_to_float(&bme280_sm.humidity));
	}

	/* Get CCS811 sensor data */
	struct sensor_value co2, voc;

	err = sensor_sample_fetch(ccs811_dev);
	if (err) {
		LOG_ERR("Failed to fetch CCS811 sensor data: %d", err);
	} else {
		sensor_channel_get(ccs811_dev, SENSOR_CHAN_CO2, &co2);
		sensor_channel_get(ccs811_dev, SENSOR_CHAN_VOC, &voc);
		LOG_DBG("CCS811:  eCO2= %u ppm; eTVOC= %u ppb", co2.val1, voc.val1);
	}

	/* Get accelerometer sensor data */
	struct sensor_value accel[3];
	sensor_sample_fetch(accel_dev);
	if (err) {
		LOG_ERR("Failed to fetch accelerometer sensor data: %d", err);
	} else {
		sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
		LOG_DBG("Accel: x= %.2f , y= %.2f , z= %.2f",
			sensor_value_to_float(&accel[0]),
			sensor_value_to_float(&accel[1]),
			sensor_value_to_float(&accel[2]));
	}

	/* Encode accelerometer sensor data using CBOR serialization */
	uint8_t accel_buf[64];
	size_t cbor_size;

	/* Create zcbor state variable for encoding */
	ZCBOR_STATE_E(zse, 1, accel_buf, sizeof(accel_buf), 0);
	bool ok = zcbor_map_start_encode(zse, 3) &&
		  zcbor_tstr_put_lit(zse, "x") &&
		  zcbor_float32_put(zse, sensor_value_to_float(&accel[0])) &&
		  zcbor_tstr_put_lit(zse, "y") &&
		  zcbor_float32_put(zse, sensor_value_to_float(&accel[1])) &&
		  zcbor_tstr_put_lit(zse, "z") &&
		  zcbor_float32_put(zse, sensor_value_to_float(&accel[2])) &&
		  zcbor_map_end_encode(zse, 3);

	if (!ok) {
		GLTH_LOGE(TAG, "Failed to close accelerometer CBOR map");
	} else {
		cbor_size = (intptr_t) zse->payload - (intptr_t) accel_buf;
		stream_payload(accel_buf, cbor_size, "accel");
	}

	/* Encode VL53 sensor data using CBOR serialization */
	uint8_t vl53_buf[64];

	/* Create zcbor state variable for encoding */
	ZCBOR_STATE_E(zse_vl, 1, vl53_buf, sizeof(vl53_buf), 0);
	ok = zcbor_map_start_encode(zse_vl, 3) &&
	     zcbor_tstr_put_lit(zse_vl, "distance") &&
	     zcbor_int32_put(zse_vl, (sensor_value_to_milli(&distance_value))) &&
	     zcbor_tstr_put_lit(zse_vl, "proximity") &&
	     zcbor_int32_put(zse_vl, proximity_value.val1) &&
	     zcbor_tstr_put_lit(zse_vl, "fill level") &&
	     zcbor_int32_put(zse_vl, fill_level) &&
	     zcbor_map_end_encode(zse_vl, 3);

	if (!ok) {
		GLTH_LOGE(TAG, "Failed to close accelerometer CBOR map");
	} else {
		cbor_size = (intptr_t) zse_vl->payload - (intptr_t) vl53_buf;
		stream_payload(vl53_buf, cbor_size, "VL53");
	}

	/* Encode BME280 and CCS811 sensor data using CBOR serialization */
	uint8_t weather_buf[128];

	/* Create zcbor state variable for encoding */
	ZCBOR_STATE_E(zse_weather, 1, weather_buf, sizeof(zse_weather), 0);
	ok = zcbor_map_start_encode(zse_weather, 4) &&
	     zcbor_tstr_put_lit(zse_weather, "temp") &&
             zcbor_float32_put(zse_weather, sensor_value_to_float(&bme280_sm.temperature)) &&
	     zcbor_tstr_put_lit(zse_weather, "hum") &&
             zcbor_float32_put(zse_weather, sensor_value_to_float(&bme280_sm.humidity)) &&
	     zcbor_tstr_put_lit(zse_weather, "pre") &&
             zcbor_float32_put(zse_weather, sensor_value_to_float(&bme280_sm.pressure)) &&
	     zcbor_tstr_put_lit(zse_weather, "gas") &&
	     zcbor_map_start_encode(zse_weather, 2) &&
	     zcbor_tstr_put_lit(zse_weather, "co2") &&
             zcbor_int32_put(zse_weather, (co2.val1)) &&
	     zcbor_tstr_put_lit(zse_weather, "voc") &&
             zcbor_int32_put(zse_weather, (voc.val1)) &&
	     zcbor_map_end_encode(zse_weather, 2) &&
	     zcbor_map_end_encode(zse_weather, 4);

	if (!ok) {
		GLTH_LOGE(TAG, "Failed to close weather CBOR map");
	} else {
		cbor_size = (intptr_t) zse_weather->payload - (intptr_t) weather_buf;
		stream_payload(weather_buf, cbor_size, "weather");
	}
}

void app_sensors_set_client(struct golioth_client *sensors_client)
{
	client = sensors_client;
}
