/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * config.c
 *
 *  Created on: 29.07.2018
 *      Author: hoene
 */

#include "udp.h"

#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "esp_err.h"
#include "esp_log.h"

#include "config.h"
#include "rtp.h"
#include "status.h"
#include "wifi.h"

static const char *TAG = "#udp";

static int udp_socket = -1;

static uint8_t udp_buffer[1501];

// UDP Listener

void udp_on() {

  // bind to socket
  ESP_LOGD(TAG, "bind_udp_server port:%d", config_get_udp_port());
  udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_socket < 0) {
    ESP_LOGE(TAG, "socket %d %s", udp_socket, strerror(udp_socket));
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(config_get_udp_port());
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    ESP_LOGE(TAG, "socket %d %s", udp_socket, strerror(udp_socket));
    udp_off();
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  ESP_LOGD(TAG, "socket created without errors");
}

void udp_off() {
  if (udp_socket >= 0) {
    close(udp_socket);
    udp_socket = -1;
  }
}

void udp_process(int8_t wait) {

  if (udp_socket < 0)
    return;

  struct sockaddr_in si_other;
  socklen_t si_len = sizeof(si_other);

  int len =
      recvfrom(udp_socket, udp_buffer, sizeof(udp_buffer) - 1,
               wait ? 0 : MSG_DONTWAIT, (struct sockaddr *)&si_other, &si_len);
  if (len < 0) {
    if (errno == EAGAIN)
      return;
    ESP_LOGE(TAG, "recv %d %d %s", len, errno, strerror(errno));
    ESP_ERROR_CHECK(ESP_FAIL);
  }
  if (len == 0) {
    ESP_LOGD(TAG, "recv no packet");
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  udp_buffer[len] = 0;
  ESP_LOGD(TAG, "Received packet from %s:%d", inet_ntoa(si_other.sin_addr),
           ntohs(si_other.sin_port));
  //	ESP_LOGI(TAG, "Data: %d -- %s\n", len, udp_buffer);

  int res = rtp_parse(udp_buffer, len);
  if (res) {
    status_rtp_last(udp_buffer, len);
    status_rtp_error();
  } else
    status_rtp_good();
}
