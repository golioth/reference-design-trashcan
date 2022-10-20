/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(trashcan_main, LOG_LEVEL_INF);

#include <zephyr/net/coap.h>

#include <net/golioth/system_client.h>


#include <stdlib.h>
#include <stdio.h>
//#include <zephyr/sys/reboot.h>

#include "golioth_app.h"
#include "golioth_ota.h"
#include "main.h"


#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define DEBOUNCE_TIMEOUT_MS 50


int counter = 0;

struct device *voc_sensor;
struct device *imu_sensor;
struct device *weather_sensor;
struct device *distance_sensor;




/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;



// This work function will submit a LightDB Stream output
// It should be called every time the sensor takes a reading
// Now near the top of main.c because the timer and button cb
// both need to know about it. Might move to separate file later.

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
	char* orientation = "unknown";
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
	LOG_INF("Trash level is %f", trash_level);


	// Bucket the Z accelerometer reading
	double z_orientation = sensor_value_to_double(&accel_z);
	LOG_DBG("Z accel is %f and the Threshhold is %f", z_orientation, get_z_threshold());
	if (z_orientation > (double)get_z_threshold() && z_orientation > MIN_Z_THRESHOLD)
	{
		orientation = "upright";	
	}
	else if (z_orientation < (double)get_z_threshold() && z_orientation < MAX_Z_THRESHOLD)
	{
		orientation = "tipped";
	}
	else
	{
		orientation = "error";
		LOG_ERR("Your math or your Z_Threshold limits are wrong. Check settings.");
	}
	LOG_INF("Orientation is %s", orientation);


	snprintk(sbuf, sizeof(sbuf) - 1,
			"{\"imu\":{\"accel_x\":%f,\"accel_y\":%f,\"accel_z\":%f,\"orientation\":\"%s\"},\"weather\":{\"temp\":%f,\"pressure\":%f,\"humidity\":%f},\"gas\":{\"co2\":%f,\"voc\":%f},\"distance\":{\"distance\":%f,\"prox\":%f,\"level\":%f}}",
			sensor_value_to_double(&accel_x),
			sensor_value_to_double(&accel_y),
			sensor_value_to_double(&accel_z),
			orientation,
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





void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	
	// Software timer debounce

	static uint64_t last_time = 0;

	uint64_t now = k_uptime_get();
	// printk("Now is %lld, last time is %lld\n", now, last_time); // debug debounce
	if ((now - last_time) > DEBOUNCE_TIMEOUT_MS)
	{
		k_work_submit(&my_sensorstream_work);
		LOG_INF("Button pressed, initiating reading and restarting");
		restart_timer();
	}
	last_time = now;

}




void button_init(void)

{
	int ret;

	if (!device_is_ready(button.port)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_LEVEL_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

}




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


void my_timer_handler(struct k_timer *dummy) {

	char sbuf[sizeof("4294967295")];
	// int err;

	uint32_t timer_interval = get_sensor_interval();

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
	app_init();
	sensor_init();
	button_init();

	uint32_t timer_interval = get_sensor_interval();
	LOG_INF("Starting timer with interval of %d seconds", timer_interval);
	k_timer_start(&my_timer, K_SECONDS(timer_interval), K_SECONDS(timer_interval));

}
