/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * ownled.c
 *
 *  Created on: 21.10.2018
 *      Author: hoene
 */

#ifndef MAIN_OWNLED_C_
#define MAIN_OWNLED_C_

#include "esp_system.h"
#include <stdint.h>

extern void ownled_init();
extern void ownled_prepare();
extern void ownled_send();
extern esp_err_t ownled_isFinished();
extern void ownled_free();
extern void ownled_setByte(uint8_t c, uint16_t pos, uint8_t sw);
extern void ownled_setPixel(uint8_t c, uint16_t pos, uint8_t r, uint8_t g,
                            uint8_t b);
extern uint8_t ownled_getChannels();
extern void ownled_setSize(uint8_t channel, uint16_t numPixel);

esp_err_t ownled_set_pulses(uint32_t frequency, uint8_t one, uint8_t zero);
uint32_t ownled_get_pulse_frequency();
uint8_t ownled_get_pulse_one();
uint8_t ownled_get_pulse_zero();

void ownled_set_default();

enum OWNLED_COLOR_ORDER {
  OWNLED_RGB = 0,
  OWNLED_RBG,
  OWNLED_GRB,
  OWNLED_GBR,
  OWNLED_BRG,
  OWNLED_BGR,
  OWNLED_RGB_FB,
  OWNLED_RBG_FB,
  OWNLED_GRB_FB,
  OWNLED_GBR_FB,
  OWNLED_BRG_FB,
  OWNLED_BGR_FB,
  OWNLED_BW,
  OWNLED_BW_FB,
  OWNLED_48,
  OWNLED_48_FB
};
extern enum OWNLED_COLOR_ORDER ownled_getColorOrder();
extern esp_err_t ownled_setColorOrder(enum OWNLED_COLOR_ORDER order);

#endif /* MAIN_OWNLED_C_ */
