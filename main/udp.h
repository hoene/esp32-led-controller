/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * udp.h
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#ifndef MAIN_UDP_H_
#define MAIN_UDP_H_

#include <stdint.h>

void udp_on();
void udp_off();
void udp_process(int8_t wait);

#endif /* MAIN_UDP_H_ */
