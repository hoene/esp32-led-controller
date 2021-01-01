/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * decoder.h
 *
 *  Created on: 02.09.2018
 *      Author: hoene
 */

#ifndef MAIN_JPGFILE_H_
#define MAIN_JPGFILE_H_

#include <stdint.h>

int jpegfile(uint8_t *p, int type, int w, int h, uint8_t *qt);

#endif /* MAIN_JPGFILE_H_ */
