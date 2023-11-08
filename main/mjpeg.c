/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * mjpeg.c
 *
 *  Created on: 02.09.2018
 *      Author: hoene
 */

#include "mjpeg.h"

#include <arpa/inet.h>
#include <string.h>

#include "decoding.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "jpgfile.h"
#include "status.h"

static const char *TAG = "#mjpeg";

static TaskHandle_t remote_task = NULL;

void mjpeg_frame_wait_for_new(void) {
  remote_task = xTaskGetCurrentTaskHandle();
  ulTaskNotifyTake(pdTRUE, 0);
  remote_task = NULL;
}

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | Type-specific |              Fragment Offset                  |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |      Type     |       Q       |     Width     |     Height    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */
struct __attribute__((packed)) rtp_mjpeg {
  uint32_t fragmentOffset : 24;
  uint32_t typeSpecific : 8;
  uint8_t type;
  uint8_t q;
  uint8_t width;
  uint8_t height;
};

struct __attribute__((packed)) quantizationTable {
  uint8_t tbd;
  uint8_t precision;
  uint16_t len;
  uint8_t data[0];
};

static struct MJPEG_FILE current;
static struct MJPEG_FILE last;
static xSemaphoreHandle xSemaphore;
static int offset_counter;

void mjpeg_frame_release() { xSemaphoreGive(xSemaphore); }

struct MJPEG_FILE *mjpeg_frame_access(TickType_t xTicksToWait) {
  if (!xSemaphoreTake(xSemaphore, xTicksToWait))
    return NULL;

  if (last.size > 0)
    return &last;

  mjpeg_frame_release();
  return NULL;
}

static void finish_current() {

  if (!xSemaphoreTake(xSemaphore, 0)) {
    status_mjpeg_loss(1);
    ESP_LOGE(TAG, "dropping jpeg image of length %d", last.size);
  } else {
    last.decoded = false;
    memcpy(&last, &current, sizeof(current));
    xSemaphoreGive(xSemaphore);
    if (remote_task)
      xTaskNotifyGive(remote_task);
    status_mjpeg_good();
  }
}

void mjpeg_on() {
  current.size = 0;
  last.size = 0;
  current.decoded = false;
  offset_counter = -1;
  vSemaphoreCreateBinary(xSemaphore);
}

void mjpeg_off() { xSemaphoreGive(xSemaphore); }

int mjpeg_header_parse(uint8_t *buffer, int length, uint8_t start,
                       uint8_t end) {
  if (length < sizeof(struct rtp_mjpeg)) {
    ESP_LOGE(TAG, "short mjpeg header %d", length);
    return -20;
  }

  struct rtp_mjpeg *mjpegHeader = (struct rtp_mjpeg *)buffer;
  *(uint32_t *)buffer = ntohl(*(uint32_t *)buffer);

#if 0
	ESP_LOGI(TAG, "%08XL %d ts %u offset %u type %u q %u w %u h %u",
			*(uint32_t* )buffer, length, mjpegHeader->typeSpecific,
			mjpegHeader->fragmentOffset, mjpegHeader->type, mjpegHeader->q,
			mjpegHeader->width * 8, mjpegHeader->height * 8);
#endif

  if (start && mjpegHeader->fragmentOffset != 0) {
    ESP_LOGE(TAG, "Missing header, drop entire jpeg image");
    offset_counter = -1;
    return -21;
  }

  buffer += sizeof(struct rtp_mjpeg);
  length -= sizeof(struct rtp_mjpeg);

  /* Parse the quantization table header. */

  if (mjpegHeader->fragmentOffset != 0 &&
      mjpegHeader->fragmentOffset != offset_counter) {
    ESP_LOGE(TAG, "Lost segment, drop entire jpeg image %d %d",
             mjpegHeader->fragmentOffset, offset_counter);
    offset_counter = -1;
    status_mjpeg_loss(1);
    return -23;
  }

  /* Start of JPEG data packet. */
  if (mjpegHeader->fragmentOffset == 0) {

    /* check for valid jpeg */
    if (mjpegHeader->q != 255) {
      ESP_LOGI(TAG, "Q value of %d not implemented", mjpegHeader->q);
      status_mjpeg_error();
      return -22;
    }

    if (length < sizeof(struct quantizationTable)) {
      ESP_LOGE(TAG, "short quantization table header %d", length);
      status_mjpeg_error();
      return -30;
    }

    struct quantizationTable *qTableHeader = (struct quantizationTable *)buffer;
    qTableHeader->len = ntohs(qTableHeader->len);

    ESP_LOGV(TAG, "%08XL tbd %u prec %u len %u", *(uint32_t *)buffer,
             qTableHeader->tbd, qTableHeader->precision, qTableHeader->len);

    if (qTableHeader->precision != 0 ||
        qTableHeader->len > length - sizeof(*qTableHeader) ||
        qTableHeader->len != 64) {
      ESP_LOGE(TAG, "quantization table header invalid");
      status_mjpeg_error();
      return -31;
    }

    /* generated jpeg file header */
    current.size = jpegfile(current.buffer, mjpegHeader->type,
                            mjpegHeader->width, mjpegHeader->height,
                            buffer + sizeof(struct quantizationTable));
    assert(current.size <= sizeof(current.buffer));

    buffer += qTableHeader->len + sizeof(*qTableHeader);
    length -= qTableHeader->len + sizeof(*qTableHeader);
    offset_counter = 0;
  }

  /* append data */
  if (current.size + length > sizeof(current.buffer)) {
    ESP_LOGE(TAG, "dropping mjpeg frame, too large");
    status_mjpeg_error();
    return -33;
  }
  memcpy(current.buffer + current.size, buffer, length);
  current.size += length;

  /* test for end */
  offset_counter += length;
  if (offset_counter > 0 && end) {
    finish_current();
  }
  return 0;
}
