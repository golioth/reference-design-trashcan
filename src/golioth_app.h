/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GOLIOTH_APP_H__
#define __GOLIOTH_APP_H__


extern struct golioth_client *client;


/* SENSOR_DATA_STRING_LEN must be a multiple of 4!! */
#define SENSOR_DATA_STRING_LEN	300
#define SENSOR_DATA_ARRAY_SIZE	16
#define MIN_Z_THRESHOLD -10
#define MAX_Z_THRESHOLD 10


#define GOLIOTH_STREAM_DEFAULT_ENDPOINT GOLIOTH_LIGHTDB_STREAM_PATH("sensor")

//prototype

void app_init(void);
void send_queued_data_to_golioth(char*, char*);
uint32_t get_sensor_interval(void); 
uint32_t get_transmit_every_x_reading(void); 
uint32_t get_trash_dist_25_pct_in_mm(void);
uint32_t get_trash_dist_50_pct_in_mm(void);
uint32_t get_trash_dist_75_pct_in_mm(void);
uint32_t get_trash_dist_90_pct_in_mm(void);
uint32_t get_trash_dist_95_pct_in_mm(void);
uint32_t get_trash_dist_98_pct_in_mm(void);
uint32_t get_trash_dist_100_pct_in_mm(void);
double get_z_threshold(void);


#endif