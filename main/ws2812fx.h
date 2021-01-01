/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * rtp.h
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#ifndef WS2812FX_H_
#define WS2812FX_H_

#include <stdint.h>

void WS2812FX_init(int line, int n);
void WS2812FX_call(int line, uint16_t (*fct)(int));

uint16_t WS2812FX_mode_bicolor_chase(int line);
uint16_t WS2812FX_mode_blink(int line);
uint16_t WS2812FX_mode_blink_rainbow(int line);
uint16_t WS2812FX_mode_breath(int line);
uint16_t WS2812FX_mode_chase_blackout(int line);
uint16_t WS2812FX_mode_chase_blackout_rainbow(int line);
uint16_t WS2812FX_mode_chase_color(int line);
uint16_t WS2812FX_mode_chase_flash(int line);
uint16_t WS2812FX_mode_chase_flash_random(int line);
uint16_t WS2812FX_mode_chase_rainbow(int line);
uint16_t WS2812FX_mode_chase_rainbow_white(int line);
uint16_t WS2812FX_mode_chase_random(int line);
uint16_t WS2812FX_mode_chase_white(int line);
uint16_t WS2812FX_mode_circus_combustus(int line);
uint16_t WS2812FX_mode_color_sweep_random(int line);
uint16_t WS2812FX_mode_color_wipe(int line);
uint16_t WS2812FX_mode_color_wipe_inv(int line);
uint16_t WS2812FX_mode_color_wipe_random(int line);
uint16_t WS2812FX_mode_color_wipe_rev(int line);
uint16_t WS2812FX_mode_color_wipe_rev_inv(int line);
// uint16_t WS2812FX_mode_comet(int line);
uint16_t WS2812FX_mode_dual_scan(int line);
uint16_t WS2812FX_mode_fade(int line);
uint16_t WS2812FX_mode_fire_flicker(int line);
uint16_t WS2812FX_mode_fire_flicker_intense(int line);
uint16_t WS2812FX_mode_fire_flicker_soft(int line);
// uint16_t WS2812FX_mode_fireworks(int line)
// uint16_t WS2812FX_mode_fireworks_random(void)
uint16_t WS2812FX_mode_flash_sparkle(int line);
uint16_t WS2812FX_mode_halloween(int line);
uint16_t WS2812FX_mode_hyper_sparkle(int line);
uint16_t WS2812FX_mode_icu(int line);
// uint16_t WS2812FX_mode_larson_scanner(int line);
uint16_t WS2812FX_mode_merry_christmas(int line);
uint16_t WS2812FX_mode_multi_dynamic(int line);
uint16_t WS2812FX_mode_multi_strobe(int line);
uint16_t WS2812FX_mode_rainbow(int line);
uint16_t WS2812FX_mode_rainbow_cycle(int line);
uint16_t WS2812FX_mode_random_color(int line);
uint16_t WS2812FX_mode_running_color(int line);
uint16_t WS2812FX_mode_running_lights(int line);
uint16_t WS2812FX_mode_running_random(int line);
uint16_t WS2812FX_mode_running_red_blue(int line);
uint16_t WS2812FX_mode_scan(int line);
uint16_t WS2812FX_mode_single_dynamic(int line);
uint16_t WS2812FX_mode_sparkle(int line);
uint16_t WS2812FX_mode_static(int line);
uint16_t WS2812FX_mode_strobe(int line);
uint16_t WS2812FX_mode_strobe_rainbow(int line);
uint16_t WS2812FX_mode_theater_chase(int line);
uint16_t WS2812FX_mode_theater_chase_rainbow(int line);
uint16_t WS2812FX_mode_tricolor_chase(int line);
uint16_t WS2812FX_mode_twinkle(int line);
// uint16_t WS2812FX_mode_twinkle_fade(int line);
// uint16_t   WS2812FX_mode_twinkle_fade_random(int line);
uint16_t WS2812FX_mode_twinkle_random(int line);

#endif /* WS2812FX */
