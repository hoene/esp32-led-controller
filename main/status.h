/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * status.h
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#ifndef MAIN_STATUS_H_
#define MAIN_STATUS_H_

#include "esp_wifi_types.h"

#define RTP_LAST_BYTES (32)

struct STATUS {
  char sta[64];
  char ap[64];
  char ethernet[64];
  int ap_size;
  wifi_ap_record_t *ap_records;
  char server[32];
  int rtp_error, rtp_good, rtp_loss;
  int mjpeg_error, mjpeg_good, mjpeg_loss;
  int artnet_error, artnet_good, artnet_loss;
  char rtp_last[RTP_LAST_BYTES * 3 + 2];
  uint32_t led_on_time, led_bottom_too_slow, led_top_too_slow;
  char ntp[32];
  char geoip[64];
};

extern struct STATUS status;

void status_init();

void status_sta(const char *);
void status_ap(const char *);
void status_ethernet(const char *);

void status_scan(int, wifi_ap_record_t *);
void status_server(const char *);

void status_rtp_good();
void status_rtp_loss(int);
void status_rtp_error();

void status_rtp_last(uint8_t *, int);

void status_mjpeg_good();
void status_mjpeg_loss(int);
void status_mjpeg_error();

void status_artnet_error();
void status_artnet_loss(int);
void status_artnet_good();

void status_led_on_time();
void status_led_bottom_too_slow();
void status_led_top_too_late();

void status_ntp(const char *);
void status_geoip(const char *);

#endif /* MAIN_STATUS_H_ */
