/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * config.h
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

#include "led.h"
#include <stdint.h>

void config_init();
void config_reset_to_defaults();
void config_write_all();

void config_coloring_reload();
void config_coloring_defaults();
void config_coloring_write();

void config_update_channels();

const char *config_get_wifi_sta_ssid();
const char *config_get_wifi_sta_password();
const char *config_get_wifi_ap_ssid();
const char *config_get_wifi_ap_password();

int config_get_udp_port();
const char *config_get_udp_server();

void config_get_led_line(int line, enum LED_MODE *mode, int *size_x,
		int *size_y, int *offset_x, int *offset_y,
		enum LED_ORIENTATION *orientation, char options[65]);

uint8_t config_get_prefix_leds();
int16_t config_get_refresh_rate();

void config_set_wifi_sta_ssid(const char *);
void config_set_wifi_ap_ssid(const char *);
void config_set_wifi_sta_password(const char *);
void config_set_wifi_ap_password(const char *);
void config_set_udp_port(uint16_t port);
void config_set_udp_server(const char *);

void config_set_led_line(int line, enum LED_MODE mode, int size_x, int size_y,
		int offset_x, int offset_y, enum LED_ORIENTATION orientation,
		const char *string);

void config_set_prefix_leds(uint8_t);
void config_set_refresh_rate(int16_t);

void config_set_artnet_width(uint16_t);
void config_set_artnet_universe_offset(uint16_t);
uint16_t config_get_artnet_width();
uint16_t config_get_artnet_universe_offset();

const char *config_get_compile_date();
const char *config_get_version();

void config_set_partition_name(const char *partitionName);


#endif /* MAIN_CONFIG_H_ */
