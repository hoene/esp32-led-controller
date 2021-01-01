/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */


#ifndef MAIN_WEBSESSION_H_
#define MAIN_WEBSESSION_H_

#include <esp_http_server.h>

int websession_add(httpd_handle_t hd, int sockfd);
void websession_removeAll();
int websession_setWebsocket(httpd_handle_t hd, int sockfd, bool websocket);
void websession_remove(httpd_handle_t hd, int sockfd);
void websession_send_to_all(httpd_ws_frame_t *ws_pkt);

#endif // MAIN_WEBSESSION_H_