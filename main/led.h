/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * led.h
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#ifndef MAIN_LED_H_
#define MAIN_LED_H_

#include <stdint.h>

enum LED_ORIENTATION {
  LED_ORI0_ZIGZAG,
  LED_ORI0F_ZIGZAG,
  LED_ORI90_ZIGZAG,
  LED_ORI90F_ZIGZAG,
  LED_ORI180_ZIGZAG,
  LED_ORI180F_ZIGZAG,
  LED_ORI270_ZIGZAG,
  LED_ORI270F_ZIGZAG,
  LED_ORI0_MEANDER,
  LED_ORI0F_MEANDER,
  LED_ORI90_MEANDER,
  LED_ORI90F_MEANDER,
  LED_ORI180_MEANDER,
  LED_ORI180F_MEANDER,
  LED_ORI270_MEANDER,
  LED_ORI270F_MEANDER,
  LED_ORI_MAXVALUE
};
enum LED_MODE {
  LED_MODE_OFF,
  LED_MODE_NETWORK,
  LED_MODE_WHITE,
  LED_MODE_GRAY,
  LED_MODE_RED,
  LED_MODE_YELLOW,
  LED_MODE_GREEN,
  LED_MODE_CYAN,
  LED_MODE_BLUE,
  LED_MODE_MAGENTA,
  LED_MODE_FADE_X,
  LED_MODE_FADE_Y,
  LED_MODE_FADE_XY,
  LED_MODE_LINES_X,
  LED_MODE_LINES_Y,
  LED_MODE_LINES_XY,
  LED_MODE_CORNERS,
  LED_MODE_SQUARE,
  LED_MODE_PRODUCTION_TEST,
  FX_MODE_BLINK,
  FX_MODE_BREATH,
  // FX_MODE_STATIC,
  FX_MODE_COLOR_WIPE,
  FX_MODE_COLOR_WIPE_INV,
  FX_MODE_COLOR_WIPE_REV,
  FX_MODE_COLOR_WIPE_REV_INV,
  FX_MODE_COLOR_WIPE_RANDOM,
  FX_MODE_RANDOM_COLOR,
  FX_MODE_SINGLE_DYNAMIC,
  FX_MODE_MULTI_DYNAMIC,
  FX_MODE_RAINBOW,
  FX_MODE_RAINBOW_CYCLE,
  FX_MODE_SCAN,
  FX_MODE_DUAL_SCAN,
  FX_MODE_FADE,
  FX_MODE_THEATER_CHASE,
  FX_MODE_THEATER_CHASE_RAINBOW,
  FX_MODE_RUNNING_LIGHTS,
  //	FX_MODE_TWINKLE,
  //	FX_MODE_TWINKLE_RANDOM,
  // FX_MODE_TWINKLE_FADE,
  // FX_MODE_TWINKLE_FADE_RANDOM,
  FX_MODE_SPARKLE,
  FX_MODE_FLASH_SPARKLE,
  FX_MODE_HYPER_SPARKLE,
  FX_MODE_STROBE,
  FX_MODE_STROBE_RAINBOW,
  FX_MODE_MULTI_STROBE,
  FX_MODE_BLINK_RAINBOW,
  FX_MODE_CHASE_WHITE,
  FX_MODE_CHASE_COLOR,
  FX_MODE_CHASE_RANDOM,
  FX_MODE_CHASE_RAINBOW,
  FX_MODE_CHASE_FLASH,
  FX_MODE_CHASE_FLASH_RANDOM,
  FX_MODE_CHASE_RAINBOW_WHITE,
  FX_MODE_CHASE_BLACKOUT,
  FX_MODE_CHASE_BLACKOUT_RAINBOW,
  FX_MODE_COLOR_SWEEP_RANDOM,
  FX_MODE_RUNNING_COLOR,
  FX_MODE_RUNNING_RED_BLUE,
  //	FX_MODE_RUNNING_RANDOM,
  //	FX_MODE_LARSON_SCANNER,
  //	FX_MODE_COMET,
  //	FX_MODE_FIREWORKS,
  //	FX_MODE_FIREWORKS_RANDOM,
  FX_MODE_MERRY_CHRISTMAS,
  FX_MODE_FIRE_FLICKER,
  FX_MODE_FIRE_FLICKER_SOFT,
  FX_MODE_FIRE_FLICKER_INTENSE,
  FX_MODE_CIRCUS_COMBUSTUS,
  FX_MODE_HALLOWEEN,
  FX_MODE_BICOLOR_CHASE,
  FX_MODE_TRICOLOR_CHASE,
  FX_MODE_ICU,
  FX_MODE_PLAYLIST,
  LED_MODE_MAXVALUE
};

struct LED_COLORING {
  float contrast;
  float brightness;
  float red_contrast;
  float red_brightness;
  float green_contrast;
  float green_brightness;
  float blue_contrast;
  float blue_brightness;
  float saturation;
  float hue;
};
extern struct LED_COLORING led_coloring;

struct LED_CONFIG_CHANNEL {
  enum LED_MODE mode;
  enum LED_ORIENTATION orientation;
  int16_t sx;
  int16_t sy;
  int16_t ox;
  int16_t oy;
  int16_t black[3];
};

struct LED_CONFIG {
  int16_t refresh_rate;
  uint8_t channels;
  uint8_t prefix_leds;
  struct LED_CONFIG_CHANNEL channel[8];
  uint16_t artnet_width;
  uint16_t artnet_universe_offset;
};

void led_on();
void led_off();

int led_get_max_lines();

void led_set_config(struct LED_CONFIG *);
void led_set_framerate(float);

void led_cube(float x, float y, float z);
void led_cube2(int d, float x, float y, float z);
void led_block(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b);
void led_set_color(uint8_t c, uint16_t p, uint8_t r, uint8_t g, uint8_t b);
void led_rgb_rtp(int x, int y, uint8_t r, uint8_t g, uint8_t b);
void led_trigger();

#endif /* MAIN_LED_H_ */
