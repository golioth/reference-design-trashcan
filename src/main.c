/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(trashcan_main, LOG_LEVEL_DBG);

#include <net/coap.h>

#include <net/golioth/system_client.h>
#include <samples/common/wifi.h>


#include <stdlib.h>
#include <stdio.h>
//#include <sys/reboot.h>

#include "golioth_app.h"
#include "golioth_ota.h"
#include "main.h"


#include <drivers/sensor.h>
#include <device.h>

static uint32_t timer_interval;

int counter = 0;

struct device *voc_sensor;
struct device *imu_sensor;
struct device *weather_sensor;
struct device *distance_sensor;

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

	LOG_DBG("VL53L0X Init");
	distance_sensor = (void *)DEVICE_DT_GET_ANY(st_vl53l0x);

    if (distance_sensor == NULL) {
        printk("Could not get vl53l0x device\n");
        return;
    }

}



// This work function will submit a LightDB Stream output
// It should be called every time the sensor takes a reading

void my_sensorstream_work_handler(struct k_work *work)
{
	// int err;
	struct sensor_value temp;
	struct sensor_value pressure;	
	struct sensor_value humidity;
	struct sensor_value accel_x;
	struct sensor_value accel_y;
	struct sensor_value accel_z;
	struct sensor_value co2;
	struct sensor_value voc;
	struct sensor_value distance;
	struct sensor_value prox;
	double trash_level = 0;
	char sbuf[SENSOR_DATA_STRING_LEN];
	
	LOG_DBG("Sensor Stream Work");

	// kick off an IMU sensor reading!
	LOG_DBG("Fetching IMU Reading");
	sensor_sample_fetch(imu_sensor);
	sensor_channel_get(imu_sensor, SENSOR_CHAN_ACCEL_X, &accel_x);
	LOG_DBG("  Accel X is %d.%06d", accel_x.val1, abs(accel_x.val2));
	sensor_channel_get(imu_sensor, SENSOR_CHAN_ACCEL_Y, &accel_y);
	LOG_DBG("  Accel Y is %d.%06d", accel_y.val1, abs(accel_y.val2));	
	sensor_channel_get(imu_sensor, SENSOR_CHAN_ACCEL_Z, &accel_z);
	LOG_DBG("  Accel Z is %d.%06d", accel_z.val1, abs(accel_z.val2));



	// kick off a weather sensor reading!
	LOG_DBG("Fetching Weather Reading");
	sensor_sample_fetch(weather_sensor);
	sensor_channel_get(weather_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	LOG_DBG("  Temp is %d.%06d", temp.val1, abs(temp.val2));
	sensor_channel_get(weather_sensor, SENSOR_CHAN_PRESS, &pressure);
	LOG_DBG("  Pressure is %d.%06d", pressure.val1, abs(pressure.val2));
	sensor_channel_get(weather_sensor, SENSOR_CHAN_HUMIDITY, &humidity);
	LOG_DBG("  Humidity is %d.%06d", humidity.val1, abs(humidity.val2));


	// kick off a VOC sensor reading!
	LOG_DBG("Fetching VOC Reading");
	sensor_sample_fetch(voc_sensor);
	sensor_channel_get(voc_sensor, SENSOR_CHAN_CO2, &co2);
	LOG_DBG("  co2 is %d.%06d", co2.val1, abs(co2.val2));
	sensor_channel_get(voc_sensor, SENSOR_CHAN_VOC, &voc);
	LOG_DBG("  voc is %d.%06d", voc.val1, abs(voc.val2));

	// kick off a distance sensor reading!
	LOG_DBG("Fetching Distance Reading");
	sensor_sample_fetch(distance_sensor);
	sensor_channel_get(distance_sensor, SENSOR_CHAN_PROX, &prox);
	LOG_DBG("  prox is %d.%06d", prox.val1, abs(prox.val2));
	sensor_channel_get(distance_sensor, SENSOR_CHAN_DISTANCE, &distance);
	LOG_DBG("  distance is %d.%06d", distance.val1, abs(distance.val2));

	// Set the percentage threshhold for trash from distance sensor

	double distance_to_trash_in_mm = 1000*sensor_value_to_double(&distance);
	LOG_DBG("Trash distance is %f mm", distance_to_trash_in_mm);
	
	if (distance_to_trash_in_mm > get_trash_dist_25_pct_in_mm())
	{
		trash_level = 0;
	}
	else if (distance_to_trash_in_mm < get_trash_dist_25_pct_in_mm() && distance_to_trash_in_mm > get_trash_dist_50_pct_in_mm())
	{
		trash_level = 25;
	}
	else if (distance_to_trash_in_mm < get_trash_dist_50_pct_in_mm() && distance_to_trash_in_mm > get_trash_dist_75_pct_in_mm())
	{
		trash_level = 50;
	}
	else if (distance_to_trash_in_mm < get_trash_dist_75_pct_in_mm() && distance_to_trash_in_mm > get_trash_dist_90_pct_in_mm())
	{
		trash_level = 75;
	}
	else if (distance_to_trash_in_mm < get_trash_dist_90_pct_in_mm() && distance_to_trash_in_mm > get_trash_dist_95_pct_in_mm())
	{
		trash_level = 90;
	}
	else if (distance_to_trash_in_mm < get_trash_dist_95_pct_in_mm() && distance_to_trash_in_mm > get_trash_dist_98_pct_in_mm())
	{
		trash_level = 95;
	}
	else if (distance_to_trash_in_mm < get_trash_dist_98_pct_in_mm() && distance_to_trash_in_mm > get_trash_dist_100_pct_in_mm())
	{
		trash_level = 98;
	}
	else if (distance_to_trash_in_mm < get_trash_dist_100_pct_in_mm())
	{
		trash_level = 100;
	}
	else
	{
		// Error state
		LOG_ERR("Your math or your distance limits are wrong. Check settings.");

	}


	snprintk(sbuf, sizeof(sbuf) - 1,
			"{\"imu\":{\"accel_x\":%f,\"accel_y\":%f,\"accel_z\":%f},\"weather\":{\"temp\":%f,\"pressure\":%f,\"humidity\":%f},\"gas\":{\"co2\":%f,\"voc\":%f},\"distance\":{\"distance\":%f,\"prox\":%f,\"level\":%f}}",
			sensor_value_to_double(&accel_x),
			sensor_value_to_double(&accel_y),
			sensor_value_to_double(&accel_z),
			sensor_value_to_double(&temp),
			sensor_value_to_double(&pressure),
			sensor_value_to_double(&humidity),
			sensor_value_to_double(&co2),
			sensor_value_to_double(&voc),
			sensor_value_to_double(&distance),
			sensor_value_to_double(&prox),
			trash_level
			);

	send_queued_data_to_golioth(sbuf, "sensor");

	//add a ksleep

}

K_WORK_DEFINE(my_sensorstream_work, my_sensorstream_work_handler);



void my_timer_handler(struct k_timer *dummy) {

	char sbuf[sizeof("4294967295")];
	// int err;

	snprintk(sbuf, sizeof(sbuf) - 1, "%d", counter);

	LOG_INF("Interval of %d seconds is up, taking a reading", timer_interval);
	
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



void restart_timer(void)
{
	uint32_t timer_interval = get_sensor_interval();
	k_timer_stop(&my_timer);
	LOG_INF("Restarting timer with interval of %d seconds", timer_interval);
	k_timer_start(&my_timer, K_SECONDS(timer_interval), K_SECONDS(timer_interval));
}


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

	uint32_t timer_interval = get_sensor_interval();
	LOG_INF("Starting timer with interval of %d seconds", timer_interval);
	k_timer_start(&my_timer, K_SECONDS(timer_interval), K_SECONDS(timer_interval));

}
