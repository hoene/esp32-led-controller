/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * config.c
 *
 *  Created on: 29.07.2018
 *      Author: hoene
 */

#include "rtp.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "controller.h"
#include "esp_log.h"
#include "led.h"
#include "mjpeg.h"
#include "status.h"

static const char *TAG = "#rtp";

#define MAX_UNIVERSES (20)

static int counter;
static uint16_t last_seq;
static uint32_t last_ts;

struct __attribute__((packed)) rtp_header {
  uint32_t cc : 4;      /* CSRC count */
  uint32_t x : 1;       /* header extension flag */
  uint32_t p : 1;       /* padding flag */
  uint32_t version : 2; /* protocol version */
  uint32_t pt : 7;      /* payload type */
  uint32_t m : 1;       /* marker bit */
  uint32_t seq : 16;    /* sequence number */
  uint32_t ts;          /* timestamp */
  uint32_t ssrc;        /* synchronization source */
  uint32_t csrc[];      /* optional CSRC list */
};

static int cube_parse(uint8_t *buffer, uint16_t length) {
  float x, y, z;

  buffer[length] = 0;
  status_rtp_last(buffer, length);

  for (int i = 0; i < 32; i++) {
    int res = sscanf((char *)buffer, "(%f,%f,%f)", &x, &y, &z);
    if ((res != 3 && i == 0) || (res > 0 && res < 3)) {
      ESP_LOGE(TAG, "invalid syntax %d", res);
      return -1;
    }
    if (res == 0)
      return 0;

    led_cube2(i, x, y, z);
    ESP_LOGV(TAG, "CUBE%d: %f %f %f", i, x, y, z);

    do {
      buffer++;
    } while (*buffer && *buffer != '(');
  }

  return 0;
}

int16_t artnet_sequence[MAX_UNIVERSES];

static int artnet_parse(uint8_t *buffer, uint16_t length) {
  if (strcmp((char *)buffer, "Art-Net")) {
    ESP_LOGE(TAG, "invalid Art-Net prefix");
    return -1;
  }

  if (buffer[8] == 0x00 && buffer[9] == 0x52) {
    led_trigger();
    return -1;
  }

  if (buffer[8] != 0x00 || buffer[9] != 0x50) {
    ESP_LOGE(TAG, "unknown Art-Net protocol %02X%02X", buffer[8], buffer[9]);
    return -1;
  }

  if (buffer[10] != 0x00 || buffer[11] != 14) {
    ESP_LOGE(TAG, "invalid Art-Net protocol");
    status_artnet_error();
    return -1;
  }

  uint8_t sequence = buffer[12];
  //  uint8_t physical = buffer[13];
  uint16_t universe = buffer[14] + buffer[15] * 256;
  uint16_t dmxlen = buffer[16] * 256 + buffer[17];

  universe -= config_get_artnet_universe_offset(); // wrap around by intention
  if (universe >= MAX_UNIVERSES) {
    ESP_LOGE(TAG, "universe too high");
    status_artnet_error();
    return -1;
  }
  if (artnet_sequence[universe] >= 0) {
    int16_t diff = sequence - artnet_sequence[universe];
    if (diff > 1 && diff < 0xe0)
      status_artnet_error(diff - 1);
  }
  artnet_sequence[universe] = sequence;

  uint16_t width = config_get_artnet_width();
  uint16_t x = universe * 170;
  uint16_t y = x / width;
  x = x % width;
  for (int i = 0; i < dmxlen - 2; i += 3) {
    led_rgb(x, y, buffer[19 + i], buffer[18 + i], buffer[20 + i]);
    x++;
    if (x == width) {
      y++;
      x = 0;
    }
  }

  status_artnet_good();
  status_rtp_last(buffer, length);
  return 0;
}

void rtp_on() {
  counter = 0;
  for (uint8_t i = 0; i < MAX_UNIVERSES; i++)
    artnet_sequence[i] = -1;
}

void rtp_off() {}

int rtp_parse(uint8_t *buffer, int length) {
  if (length > 4 && buffer[0] == '(') {
    return cube_parse(buffer, length);
  }
  if (length > 8 && buffer[0] == 'A') {
    return artnet_parse(buffer, length);
  }

  struct rtp_header *header = (struct rtp_header *)buffer;

  if (length < sizeof(struct rtp_header)) {
    ESP_LOGE(TAG, "short header %d", length);
    return -1;
  }

  header->seq = ntohs(header->seq);
  header->ts = ntohl(header->ts);

  ESP_LOGD(TAG, "rtp header %02X %02X %u %u %u %u %u %u %u %u", buffer[0],
           buffer[1], header->version, header->p, header->x, header->cc,
           header->m, header->pt, header->seq, header->ts);

  int headerLength = sizeof(struct rtp_header) + header->cc * sizeof(uint32_t);
  if (length < headerLength) {
    ESP_LOGE(TAG, "short header %d", length);
    return -2;
  }

  if (header->version != 2) {
    ESP_LOGE(TAG, "wrong version %d", header->version);
    return -3;
  }

  if (header->x == 1) {
    ESP_LOGE(TAG, "header extension not support");
    return -4;
  }

  if (header->pt != 26) { // no mjpeg...
    return -5;
  }

  int8_t restart = 1;
  if (counter != 0) {
    uint16_t diff = header->seq - last_seq;
    if (diff > 1) {
      ESP_LOGE(TAG, "strange sequence %d %d", header->seq, last_seq);
      if (diff < 5)
        status_rtp_loss(diff - 1);
    } else {
      if (header->ts == last_ts)
        restart = 0; // all ok
    }
  }

  last_seq = header->seq;
  last_ts = header->ts;
  counter++;

  ESP_LOGD(TAG, "pt %d seq %d ts %u restart %d end %d len %d", header->pt,
           header->seq, header->ts, restart, header->m, length - headerLength);

  length -= headerLength;
  buffer += headerLength;

  int res = mjpeg_header_parse(buffer, length, restart, header->m);
  if (res)
    last_ts = 0;

  if (header->m)
    led_trigger();

  return res;
}
