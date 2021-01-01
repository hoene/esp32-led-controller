/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * status.c
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#include "status.h"

#include <string.h>
#include <time.h>

#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "mystring.h"
#include "web.h"
#include "wifi.h"

static const char *TAG = "#status";

struct STATUS status;

void status_init() {
  status.sta[0] = status.sta[sizeof(status.sta) - 1] = 0;
  status.ap[0] = status.ap[sizeof(status.ap) - 1] = 0;
  status.ethernet[0] = status.ethernet[sizeof(status.ethernet) - 1] = 0;
  status.server[0] = status.server[sizeof(status.server) - 1] = 0;
  status.rtp_last[0] = 0;
  status.ap_size = 0;
  status.ap_records = NULL;
  status.rtp_error = status.rtp_good = status.rtp_loss = 0;
  status.mjpeg_error = status.mjpeg_good = status.mjpeg_loss = 0;
  status.artnet_good = status.artnet_loss = 0;
  status.led_on_time = status.led_bottom_too_slow = status.led_top_too_slow = 0;
}

void status_sta(const char *s) {
  strncpy(status.sta, s, sizeof(status.sta) - 1);
  //	web_status();
}

void status_ap(const char *s) {
  strncpy(status.ap, s, sizeof(status.ap) - 1);
  //	web_status();
}

void status_ethernet(const char *s) {
  ESP_LOGI(TAG, "ethernet %s", s);

  strncpy(status.ethernet, s, sizeof(status.ethernet) - 1);
  //	web_status();
}

void status_ntp(const char *s) {
  strncpy(status.ntp, s, sizeof(status.ntp) - 1);
}

void status_geoip(const char *s) {
  strncpy(status.geoip, s, sizeof(status.geoip) - 1);
}

void status_scan(int size, wifi_ap_record_t *records) {
  if (status.ap_records)
    free(status.ap_records);
  status.ap_size = size;
  status.ap_records = records;
}

void status_server(const char *s) {
  strncpy(status.server, s, sizeof(status.server) - 1);
  //	web_status();
}

void status_rtp_good() { status.rtp_good++; }

void status_rtp_loss(int loss) { status.rtp_loss += loss; }

void status_rtp_error() { status.rtp_error++; }

void status_mjpeg_good() { status.mjpeg_good++; }

void status_mjpeg_loss(int loss) { status.mjpeg_loss += loss; }

void status_mjpeg_error() { status.mjpeg_error++; }

void status_artnet_good() { status.artnet_good++; }

void status_artnet_loss(int loss) { status.artnet_loss += loss; }

void status_artnet_error() { status.artnet_error++; }

void status_rtp_last(uint8_t *buffer, int len) {
  if (len > RTP_LAST_BYTES)
    len = RTP_LAST_BYTES;

  for (int i = 0; i < len; i++) {
    sprintf(status.rtp_last + i * 2, "%02X", buffer[i]);
    if (buffer[i] >= 32 && buffer[i] < 128)
      status.rtp_last[len * 2 + 1 + i] = buffer[i];
    else
      status.rtp_last[len * 2 + 1 + i] = '.';
  }
  status.rtp_last[len * 2] = ' ';
  status.rtp_last[len * 3 + 1] = 0;
}

#define DURATION (2l)

void status_led_update() {
  if (status.led_on_time)
    status.led_on_time--;
  if (status.led_bottom_too_slow)
    status.led_bottom_too_slow--;
  if (status.led_top_too_slow)
    status.led_top_too_slow--;
}

void status_led_on_time() {
  status_led_update();
  status.led_on_time += config_get_refresh_rate() * DURATION;
}

void status_led_bottom_too_slow() {
  status_led_update();
  status.led_bottom_too_slow += config_get_refresh_rate() * DURATION;
}

void status_led_top_too_late() {
  status_led_update();
  status.led_top_too_slow += config_get_refresh_rate() * DURATION;
}
