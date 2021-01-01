/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * mjpeg.h
 *
 *  Created on: 02.09.2018
 *      Author: hoene
 */

#ifndef MAIN_MJPEG_H_
#define MAIN_MJPEG_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

#define MJPEG_MAX_SIZE (40000)

struct MJPEG_FILE {
  uint8_t buffer[MJPEG_MAX_SIZE];
  int size;
};

void mjpeg_on();
void mjpeg_off();

int mjpeg_header_parse(uint8_t *buffer, int length, uint8_t start, uint8_t end);

int mjpeg_has_frame();
struct MJPEG_FILE *mjpeg_waitfor_frame(TaskHandle_t task);
void mjpeg_release_frame();

#endif /* MAIN_MJPEG_H_ */
