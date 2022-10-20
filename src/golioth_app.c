/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_app_c, LOG_LEVEL_INF);

#include <net/golioth/system_client.h>
#include <net/golioth/fw.h>
#include <net/golioth/settings.h>
#include <zephyr/init.h>
#include <zephyr/net/coap.h>

#include "main.h"
#include "golioth_ota.h"
#include "golioth_app.h"

K_SEM_DEFINE(sem_connected, 0, 1);

struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

extern char current_version_str[sizeof("255.255.65535")];	//TODO: refactor to remove link to flash.c
extern enum golioth_dfu_result dfu_initial_result; //TODO: refactor to remove link to golioth_ota.c

// Queue for sensor data being sent to Golioth LightDB Stream
K_MSGQ_DEFINE(sensor_data_msgq, SENSOR_DATA_STRING_LEN, SENSOR_DATA_ARRAY_SIZE, 4);

// Set default variables

uint32_t _sensor_interval = 60;
uint32_t _transmit_every_x_reading = 5;
uint32_t TRASH_DIST_25PCT_IN_MM = 500;
uint32_t TRASH_DIST_50PCT_IN_MM = 500;
uint32_t TRASH_DIST_75PCT_IN_MM = 250;
uint32_t TRASH_DIST_90PCT_IN_MM = 100;
uint32_t TRASH_DIST_95PCT_IN_MM = 50;
uint32_t TRASH_DIST_98PCT_IN_MM = 20;
uint32_t TRASH_DIST_100PCT_IN_MM = 0;
double Z_THRESHOLD = 0;	

enum golioth_settings_status on_setting(
		const char *key,
		const struct golioth_settings_value *value)
{
	LOG_DBG("Received setting: key = %s, type = %d", key, value->type);
	if (strcmp(key, "SENSOR_READING_LOOP_S") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 1000], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 1000) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		_sensor_interval = (int32_t)value->i64;
		LOG_INF("Set sensor interval to %d seconds", _sensor_interval);

		// Restart the timer with newly added value

		restart_timer();

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "TRANSMIT_DATA_EVERY_X_READING") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */

		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 16], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 16) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		_transmit_every_x_reading = (int32_t)value->i64;
		LOG_INF("Set to transmit every %d readings", _transmit_every_x_reading);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "TRASH_DIST_25PCT_IN_MM") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */

		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 5000], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 5000) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		TRASH_DIST_25PCT_IN_MM = (int32_t)value->i64;
		LOG_INF("25PCT Trash level set to %d mm", TRASH_DIST_25PCT_IN_MM);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "TRASH_DIST_50PCT_IN_MM") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */

		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 5000], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 5000) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		TRASH_DIST_50PCT_IN_MM = (int32_t)value->i64;
		LOG_INF("50PCT Trash level set to %d mm", TRASH_DIST_50PCT_IN_MM);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "TRASH_DIST_75PCT_IN_MM") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */

		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 5000], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 5000) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		TRASH_DIST_75PCT_IN_MM = (int32_t)value->i64;
		LOG_INF("75PCT Trash level set to %d mm", TRASH_DIST_75PCT_IN_MM);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "TRASH_DIST_90PCT_IN_MM") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */

		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 5000], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 5000) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		TRASH_DIST_90PCT_IN_MM = (int32_t)value->i64;
		LOG_INF("90PCT Trash level set to %d mm", TRASH_DIST_90PCT_IN_MM);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

		if (strcmp(key, "TRASH_DIST_95PCT_IN_MM") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */

		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 5000], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 5000) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		TRASH_DIST_95PCT_IN_MM = (int32_t)value->i64;
		LOG_INF("95PCT Trash distance set to %d mm", TRASH_DIST_95PCT_IN_MM);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "TRASH_DIST_98PCT_IN_MM") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */

		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 5000], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 5000) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		TRASH_DIST_98PCT_IN_MM = (int32_t)value->i64;
		LOG_INF("98PCT Trash distance set to %d mm", TRASH_DIST_98PCT_IN_MM);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "TRASH_DIST_100PCT_IN_MM") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */

		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [1, 5000], return an error if it's not */
		if (value->i64 < 1 || value->i64 > 5000) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		TRASH_DIST_100PCT_IN_MM = (int32_t)value->i64;
		LOG_INF("100PCT Trash distance set to %d mm", TRASH_DIST_100PCT_IN_MM);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "Z_THRESHOLD") == 0) {

		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_FLOAT) {
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* This setting must be in range [-10, 10], return an error if it's not */
		if (value->f < MIN_Z_THRESHOLD || value->f > MAX_Z_THRESHOLD) {
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Setting has passed all checks, so apply it to the loop delay */
		Z_THRESHOLD = (double)value->f;
		LOG_INF("Z Threshold set to %f", Z_THRESHOLD);

		return GOLIOTH_SETTINGS_SUCCESS;
	}

	/* If the setting is not recognized, we should return an error */
	return GOLIOTH_SETTINGS_KEY_NOT_RECOGNIZED;
}


static int lightdb_error_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_WRN("Failed to set lightdb: %d", rsp->err);
		return rsp->err;
	}

	return 0;
}

static void golioth_on_connect(struct golioth_client *client)
{
	int err;
	int i;

	k_sem_give(&sem_connected);

	err = golioth_fw_observe_desired(client, golioth_desired_update, &update_ctx);
	if (err) {
		LOG_ERR("Failed to start observation of desired FW: %d", err);
	}

	if (IS_ENABLED(CONFIG_GOLIOTH_SETTINGS)) 
	{
		err = golioth_settings_register_callback(client, on_setting);

		if (err) 
		{
			LOG_ERR("Failed to register settings callback: %d", err);
		}
	}
}

void golioth_lightdb_stream_handler(struct k_work *work) 

{
	int err;
	char stream_data[SENSOR_DATA_STRING_LEN];

	LOG_DBG("Pulling data off of queue");
	err = k_msgq_get(&sensor_data_msgq, &stream_data, K_NO_WAIT);
	if (err)
	{
		LOG_DBG("Error getting data from the queue: %d\n", err);	
	}
	
	err = golioth_stream_push_cb(client, "sensor",
				     GOLIOTH_CONTENT_FORMAT_APP_JSON,
				     stream_data, strlen(stream_data),
				     lightdb_error_handler, NULL);
	if (err) {
		LOG_WRN("Failed to send sensor data: %d", err);
		return;
	}

	LOG_DBG("Sent the following to LightDB Stream: %s", stream_data);

}

K_WORK_DEFINE(lightdb_stream_submit_worker, golioth_lightdb_stream_handler);


void send_queued_data_to_golioth(char* sensor_data_array, char* golioth_endpoint)
{
	int err;
	err = k_msgq_put(&sensor_data_msgq, sensor_data_array, K_NO_WAIT);
	if (err){
		LOG_ERR("Queue is full");
	}
	LOG_DBG("Kicking off LightDB Stream worker");
	k_work_submit(&lightdb_stream_submit_worker);

}

uint32_t get_sensor_interval(void)
{
	return _sensor_interval;
}

uint32_t get_transmit_every_x_reading(void)
{
	return _transmit_every_x_reading;
}

uint32_t get_trash_dist_25_pct_in_mm(void)
{
	return TRASH_DIST_25PCT_IN_MM;
}

uint32_t get_trash_dist_50_pct_in_mm(void)
{
	return TRASH_DIST_50PCT_IN_MM;
}

uint32_t get_trash_dist_75_pct_in_mm(void)
{
	return TRASH_DIST_75PCT_IN_MM;
}

uint32_t get_trash_dist_90_pct_in_mm(void)
{
	return TRASH_DIST_90PCT_IN_MM;
}

uint32_t get_trash_dist_95_pct_in_mm(void)
{
	return TRASH_DIST_95PCT_IN_MM;
}

uint32_t get_trash_dist_98_pct_in_mm(void)
{
	return TRASH_DIST_98PCT_IN_MM;
}

uint32_t get_trash_dist_100_pct_in_mm(void)
{
	return TRASH_DIST_100PCT_IN_MM;
}

double get_z_threshold(void)
{
	return Z_THRESHOLD;
}

void app_init(void)
{
	client->on_connect = golioth_on_connect;
	golioth_system_client_start();
	LOG_INF("Waiting for Golioth connection");
	k_sem_take(&sem_connected, K_FOREVER);
	LOG_INF("Connected to Golioth");
}