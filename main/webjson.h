/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#ifndef MAIN_WEBJSON_H_
#define MAIN_WEBJSON_H_

#include <esp_http_server.h>
#include <stddef.h>
#include <stdint.h>

int webjson_parseMessage(httpd_handle_t hd, int sockfd, const uint8_t *payload,
                         size_t len);

#endif