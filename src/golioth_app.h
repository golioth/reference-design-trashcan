/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GOLIOTH_APP_H__
#define __GOLIOTH_APP_H__


extern struct golioth_client *client;

// set initial sensor check interval to 60, will be modified by settings service
static uint32_t _sensor_interval = 10; 
static uint32_t _transmit_every_x_reading = 5; 

/* SENSOR_DATA_STRING_LEN must be a multiple of 4!! */
#define SENSOR_DATA_STRING_LEN	256
#define SENSOR_DATA_ARRAY_SIZE	16


#define GOLIOTH_STREAM_DEFAULT_ENDPOINT GOLIOTH_LIGHTDB_STREAM_PATH("sensor")

//prototype

void app_init(void);
void send_queued_data_to_golioth(char*, char*);

#endif