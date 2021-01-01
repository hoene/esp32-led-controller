/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/**
 * 
 */

#ifndef MAIN_WEBSOCKET_H_
#define MAIN_WEBSOCKET_H_

#include <esp_http_server.h>

void websocket_on(httpd_handle_t web_handle);
void websocket_off(httpd_handle_t web_handle);

#endif
