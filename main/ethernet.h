/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * ethernet.h
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#ifndef MAIN_ETHERNET_H_
#define MAIN_ETHERNET_H_

void ethernet_on();
const char *ethernet_status();
void ethernet_off();

#endif /* MAIN_ETHERNET_H_ */
