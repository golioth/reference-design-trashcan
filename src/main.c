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


struct device *imu_sensor;

void sensor_init(void)
{
	imu_sensor = (void *)DEVICE_DT_GET_ANY(st_lis2dh12);

    if (imu_sensor == NULL) {
        printk("Could not get lis2dh12 device\n");
        return;
    }

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

}
