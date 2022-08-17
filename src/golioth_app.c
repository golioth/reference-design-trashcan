/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <logging/log.h>
LOG_MODULE_REGISTER(golioth_c, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <net/golioth/fw.h>

#include "golioth_ota.h"
#include "golioth_app.h"

struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

struct coap_reply coap_replies[4]; // TODO: Refactor to remove coap_reply global variable


extern char current_version_str[sizeof("255.255.65535")];	//TODO: refactor to remove link to flash.c
extern enum golioth_dfu_result dfu_initial_result; //TODO: refactor to remove link to golioth_ota.c



K_MSGQ_DEFINE(sensor_data_msgq, SENSOR_DATA_STRING_LEN, SENSOR_DATA_ARRAY_SIZE, 4);




static void golioth_on_connect(struct golioth_client *client)
{
	struct coap_reply *reply;
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

	err = golioth_lightdb_set(client,
					  GOLIOTH_LIGHTDB_STREAM_PATH("sensor"),
					  COAP_CONTENT_FORMAT_TEXT_PLAIN,
					  stream_data, 
					  strlen(stream_data));
	if (err) {
		LOG_WRN("Failed to send sensor: %d", err);
		printk("Failed to send sensor: %d\n", err);	
	}

	LOG_DBG("Sent the following to LightDB Stream: %s\n", log_strdup(stream_data));

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

void app_init(void)
{
	client->on_connect = golioth_on_connect;
	client->on_message = golioth_on_message;
	golioth_system_client_start();
}