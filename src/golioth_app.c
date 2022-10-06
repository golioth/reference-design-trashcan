/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <logging/log.h>
LOG_MODULE_REGISTER(golioth_app_c, LOG_LEVEL_INF);

#include <net/golioth/system_client.h>
#include <net/golioth/fw.h>
#include <net/golioth/settings.h>
#include <zephyr/init.h>
#include <zephyr/net/coap.h>

#include "main.h"
#include "golioth_ota.h"
#include "golioth_app.h"


struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

struct coap_reply coap_replies[4]; // TODO: Refactor to remove coap_reply global variable


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


static void golioth_on_connect(struct golioth_client *client)
{
	int err;
	int i;

	err = golioth_fw_report_state(client, "main",
				      current_version_str,
				      NULL,
				      GOLIOTH_FW_STATE_IDLE,
				      dfu_initial_result);
	if (err) {
		LOG_ERR("Failed to report firmware state: %d", err);
	}

	err = golioth_fw_observe_desired(client, golioth_desired_update, &update_ctx);
	if (err) {
		LOG_ERR("Failed to start observation of desired FW: %d", err);
	}

	
	for (i = 0; i < ARRAY_SIZE(coap_replies); i++) {
		coap_reply_clear(&coap_replies[i]);
	}

	reply = coap_reply_next_unused(coap_replies, ARRAY_SIZE(coap_replies));
	if (!reply) {
		LOG_ERR("No more reply handlers");
	}

	err = golioth_fw_observe_desired(client, reply, golioth_desired_update);
	if (err) {
		coap_reply_clear(reply);
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

static void golioth_on_message(struct golioth_client *client,
			       struct coap_packet *rx)
{
	uint16_t payload_len;
	const uint8_t *payload;
	uint8_t type;

	type = coap_header_get_type(rx);
	payload = coap_packet_get_payload(rx, &payload_len);

	(void)coap_response_received(rx, NULL, coap_replies,
				     ARRAY_SIZE(coap_replies));

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
	
	// LOG_DBG("Sending to LightDB Stream: %s", log_strdup(stream_data));
	
	err = golioth_lightdb_set(client,
					GOLIOTH_LIGHTDB_STREAM_PATH("sensor"),
					COAP_CONTENT_FORMAT_TEXT_PLAIN,
					stream_data, 
					strlen(stream_data));
	if (err) {
		LOG_WRN("Failed to send sensor: %d", err);
		printk("Failed to send sensor: %d\n", err);	
	}

	LOG_DBG("Sent the following to LightDB Stream: %s", log_strdup(stream_data));

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
	client->on_message = golioth_on_message;
	golioth_system_client_start();
}