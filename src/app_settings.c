/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/settings.h>
#include "main.h"
#include "app_settings.h"

static int32_t _loop_delay_s = 60;
#define LOOP_DELAY_S_MAX 43200
#define LOOP_DELAY_S_MIN 0

static int32_t _trash_can_height_s = 500;
#define TRASH_CAN_HEIGHT_MIN 100
#define TRASH_CAN_HEIGHT_MAX 1500

int32_t get_loop_delay_s(void)
{
	return _loop_delay_s;
}

int32_t get_trash_can_heigth_s(void)
{
	return _trash_can_height_s;
}

static enum golioth_settings_status on_loop_delay_setting(int32_t new_value, void *arg)
{
	_loop_delay_s = new_value;
	LOG_INF("Set loop delay to %i seconds", new_value);
	wake_system_thread();
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_trash_can_height_setting(int32_t new_value, void *arg)
{
	_trash_can_height_s = new_value;
	LOG_INF("Set trash can height to %i millimeters", new_value);
	wake_system_thread();
	return GOLIOTH_SETTINGS_SUCCESS;
}

int app_settings_register(struct golioth_client *client)
{
	struct golioth_settings *settings = golioth_settings_init(client);

	int err = golioth_settings_register_int_with_range(settings,
							   "LOOP_DELAY_S",
							   LOOP_DELAY_S_MIN,
							   LOOP_DELAY_S_MAX,
							   on_loop_delay_setting,
							   NULL);

	if (err) {
		LOG_ERR("Failed to register settings callback: %d", err);
	}

	err = golioth_settings_register_int_with_range(settings,
						       "TRASH_CAN_HEIGHT_MM",
						       TRASH_CAN_HEIGHT_MIN,
						       TRASH_CAN_HEIGHT_MAX,
						       on_trash_can_height_setting,
						       NULL);

	if (err) {
		LOG_ERR("Failed to register settings callback: %d", err);
	}


	return err;
}
