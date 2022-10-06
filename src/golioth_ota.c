/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(ota_c, LOG_LEVEL_DBG);

#include <net/golioth/fw.h>
#include <sys/reboot.h>
#include <net/golioth/system_client.h>
#include <logging/log_ctrl.h>			// to include LOG_PANIC

#include "flash.h"
#include "golioth_ota.h"
#include "golioth_app.h"

#define REBOOT_DELAY_SEC	1


extern struct coap_reply coap_replies[4];           // from golioth_app.c, needs to be refactored

enum golioth_dfu_result dfu_initial_result = GOLIOTH_DFU_RESULT_INITIAL;

struct dfu_ctx {
	struct golioth_fw_download_ctx fw_ctx;
	struct flash_img_context flash;
	char version[65];
};

static struct dfu_ctx update_ctx;


static int data_received(struct golioth_blockwise_download_ctx *ctx,
			 const uint8_t *data, size_t offset, size_t len,
			 bool last)
{
	struct dfu_ctx *dfu = CONTAINER_OF(ctx, struct dfu_ctx, fw_ctx);
	int err;

	LOG_DBG("Received %zu bytes at offset %zu%s", len, offset,
		last ? " (last)" : "");

	if (offset == 0) {
		err = flash_img_prepare(&dfu->flash);
		if (err) {
			return err;
		}
	}

	err = flash_img_buffered_write(&dfu->flash, data, len, last);
	if (err) {
		LOG_ERR("Failed to write to flash: %d", err);
		return err;
	}

	if (offset > 0 && last) {
		err = golioth_fw_report_state(client, "main",
					      current_version_str,
					      dfu->version,
					      GOLIOTH_FW_STATE_DOWNLOADED,
					      GOLIOTH_DFU_RESULT_INITIAL);
		if (err) {
			LOG_ERR("Failed to update to '%s' state: %d", "downloaded", err);
		}

		err = golioth_fw_report_state(client, "main",
					      current_version_str,
					      dfu->version,
					      GOLIOTH_FW_STATE_UPDATING,
					      GOLIOTH_DFU_RESULT_INITIAL);
		if (err) {
			LOG_ERR("Failed to update to '%s' state: %d", "updating", err);
		}

		LOG_INF("Requesting upgrade");

		err = boot_request_upgrade(BOOT_UPGRADE_TEST);
		if (err) {
			LOG_ERR("Failed to request upgrade: %d", err);
			return err;
		}

		LOG_INF("Rebooting in %d second(s)", REBOOT_DELAY_SEC);

		/* Synchronize logs */
		LOG_PANIC();

		k_sleep(K_SECONDS(REBOOT_DELAY_SEC));

		sys_reboot(SYS_REBOOT_COLD);
	}

	return 0;
}

static uint8_t *uri_strip_leading_slash(uint8_t *uri, size_t *uri_len)
{
	if (*uri_len > 0 && uri[0] == '/') {
		(*uri_len)--;
		return &uri[1];
	}

	return uri;
}

static int golioth_desired_update(struct golioth_req_rsp *rsp)
{
	struct dfu_ctx *dfu = rsp->user_data;
	size_t version_len = sizeof(dfu->version) - 1;
	uint8_t uri[64];
	uint8_t *uri_p;
	size_t uri_len = sizeof(uri);
	int err;

	if (rsp->err) {
		LOG_ERR("Error while receiving desired FW update: %d", rsp->err);
		return 0;
	}

	LOG_HEXDUMP_DBG(rsp->data, rsp->len, "Desired");

	err = golioth_fw_desired_parse(rsp->data, rsp->len,
				       dfu->version, &version_len,
				       uri, &uri_len);
	switch (err) {
	case 0:
		break;
	case -ENOENT:
		LOG_INF("No release rolled out yet");
		return 0;
	default:
		LOG_ERR("Failed to parse desired version: %d", err);
		return err;
	}

	dfu->version[version_len] = '\0';

	if (version_len == strlen(current_version_str) &&
	    !strncmp(current_version_str, dfu->version, version_len)) {
		LOG_INF("Desired version (%s) matches current firmware version!",
			current_version_str);
		return -EALREADY;
	}

	uri_p = uri_strip_leading_slash(uri, &uri_len);

	err = golioth_fw_report_state(client, "main",
				      current_version_str,
				      dfu->version,
				      GOLIOTH_FW_STATE_DOWNLOADING,
				      GOLIOTH_DFU_RESULT_INITIAL);
	if (err) {
		LOG_ERR("Failed to update to '%s' state: %d", "downloading", err);
	}

	err = golioth_fw_download(client, uri_p, uri_len, data_received, dfu);
	if (err) {
		LOG_ERR("Failed to request firmware: %d", err);
		return err;
	}

	return 0;
}


void ota_init (void) 
{
	
	int err;

    if (!boot_is_img_confirmed()) {
		/*
		 * There is no shared context between previous update request
		 * and current boot, so treat current image 'confirmed' flag as
		 * an indication whether previous update process was successful
		 * or not.
		 */
		dfu_initial_result = GOLIOTH_DFU_RESULT_FIRMWARE_UPDATED_SUCCESSFULLY;

		err = boot_write_img_confirmed();
		if (err) {
			LOG_ERR("Failed to confirm image: %d", err);
		}
	}
}

