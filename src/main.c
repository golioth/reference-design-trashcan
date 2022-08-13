/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(golioth_dfu, LOG_LEVEL_DBG);

#include <net/coap.h>

#include <net/golioth/system_client.h>
#include <samples/common/wifi.h>


#include <stdlib.h>
#include <stdio.h>
//#include <sys/reboot.h>

#include "golioth_app.h"
#include "golioth_ota.h"

#include <drivers/sensor.h>
#include <device.h>

int sensor_interval = 5;
int counter = 0;

struct device *voc_sensor;
struct device *imu_sensor;
struct device *weather_sensor;

static struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

void sensor_init(void)
{

	LOG_DBG("CCS811 Init");
	voc_sensor = (void *)DEVICE_DT_GET_ANY(ams_ccs811);

    if (voc_sensor == NULL) {
        printk("Could not get ccs811 device\n");
        return;
    }

	LOG_DBG("LIS2DH Init");
	imu_sensor = (void *)DEVICE_DT_GET_ANY(st_lis2dh);

    if (imu_sensor == NULL) {
        printk("Could not get lis2dh device\n");
        return;
    }

	LOG_DBG("BME280 Init");
	weather_sensor = (void *)DEVICE_DT_GET_ANY(bosch_bme280);

    if (weather_sensor == NULL) {
        printk("Could not get bme280 device\n");
        return;
    }

}



// This work function will submit a LightDB Stream output
// It should be called every time the sensor takes a reading

void my_sensorstream_work_handler(struct k_work *work)
{
	int err;
	struct sensor_value temp;
	struct sensor_value pressure;	
	struct sensor_value humidity;
	struct sensor_value accel_x;
	struct sensor_value accel_y;
	struct sensor_value accel_z;
	struct sensor_value co2;
	struct sensor_value voc;
	struct sensor_value distance;
	char sbuf[100];
	
	LOG_DBG("Sensor Stream Work");

	// kick off an IMU sensor reading!
	LOG_DBG("Fetching IMU Reading");
	sensor_sample_fetch(imu_sensor);
	sensor_channel_get(imu_sensor, SENSOR_CHAN_ACCEL_X, &accel_x);
	LOG_DBG("Accel X is %d.%06d", accel_x.val1, abs(accel_x.val2));
	sensor_channel_get(imu_sensor, SENSOR_CHAN_ACCEL_Y, &accel_y);
	LOG_DBG("Accel Y is %d.%06d", accel_y.val1, abs(accel_y.val2));	
	sensor_channel_get(imu_sensor, SENSOR_CHAN_ACCEL_Z, &accel_z);
	LOG_DBG("Accel Z is %d.%06d", accel_z.val1, abs(accel_z.val2));



	// // kick off a weather sensor reading!
	// sensor_sample_fetch(weather_sensor);
	// sensor_channel_get(weather_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	// LOG_DBG("Temp is %d.%06d", temp.val1, abs(temp.val2));





	// snprintk(sbuf, sizeof(sbuf) - 1,
	// 		"{\"accel_x\":%f,\"accel_y\":%f,\"accel_z\":%f,\"mag\":%f,\"temp\":%f}",
	// 		sensor_value_to_double(&accel_x),
	// 		sensor_value_to_double(&accel_y),
	// 		sensor_value_to_double(&accel_z),
	// 		sensor_value_to_double(&mag),
	// 		sensor_value_to_double(&temp)
	// 		);


	// // printk("string being sent to Golioth is %s\n", sbuf);

	// err = golioth_lightdb_set(client,
	// 				  GOLIOTH_LIGHTDB_STREAM_PATH("redSensor"),
	// 				  COAP_CONTENT_FORMAT_TEXT_PLAIN,
	// 				  sbuf, 
	// 				  strlen(sbuf));
	// if (err) {
	// 	LOG_WRN("Failed to send sensor: %d", err);
	// 	printk("Failed to send sensor: %d\n", err);	
	//}

	LOG_DBG("Sensor data sent");
}

K_WORK_DEFINE(my_sensorstream_work, my_sensorstream_work_handler);



void my_timer_handler(struct k_timer *dummy) {

	char sbuf[sizeof("4294967295")];
	int err;

	snprintk(sbuf, sizeof(sbuf) - 1, "%d", counter);

	LOG_INF("Interval of %d seconds is up, taking a reading", sensor_interval);
	
	// err = golioth_lightdb_set(client,
	// 			  GOLIOTH_LIGHTDB_PATH("number_of_timed_updates"),
	// 			  COAP_CONTENT_FORMAT_TEXT_PLAIN,
	// 			  sbuf, strlen(sbuf));
	// if (err) {
	// 	LOG_WRN("Failed to update counter: %d", err);
	// }
	
	// counter++;


	k_work_submit(&my_sensorstream_work);

}

K_TIMER_DEFINE(my_timer, my_timer_handler, NULL);




void main(void)
{

	LOG_DBG("Start Vertical Trashcan sample");

	ota_init();

	if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_WIFI)) {
		LOG_INF("Connecting to WiFi");
		wifi_connect();
	}

	app_init();
	sensor_init();

	LOG_INF("Starting timer with interval of %d seconds", sensor_interval);

	k_timer_start(&my_timer, K_SECONDS(sensor_interval), K_SECONDS(sensor_interval));

}
