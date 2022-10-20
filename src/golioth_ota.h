/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GOLIOTH_OTA_H__
#define __GOLIOTH_OTA_H__

#include "flash.h"
#include <net/golioth/fw.h>

//prototype

void ota_init (void);

int golioth_desired_update(struct golioth_req_rsp *rsp);

void ota_img_confirm(void);		

// Hacktime

struct dfu_ctx {
	struct flash_img_context flash;
	char version[65];
};

extern struct dfu_ctx update_ctx;
extern enum golioth_dfu_result dfu_initial_result;


#endif