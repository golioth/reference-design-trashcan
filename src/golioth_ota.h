/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GOLIOTH_OTA_H__
#define __GOLIOTH_OTA_H__

//prototype

void ota_init (void);
int golioth_desired_update(const struct coap_packet *update,
				  struct coap_reply *reply,
				  const struct sockaddr *from); // dis bad

#endif