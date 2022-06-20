/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * led.c
 *
 *  Created on: 04.08.2018
 *      Author: hoene
 */
#include "led.h"

#include <math.h>
#include <netdb.h>
#include <string.h>
//#include "defs.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "ownled.h"
#include "status.h"
#include "ws2812fx.h"
#include "mysntp.h"
#include "playlist.h"

static const char *TAG = "#led";

static QueueHandle_t q;
static TaskHandle_t taskHandle;
static float framerate = 20.f;
static struct LED_CONFIG led_config;
static int32_t led_counter = 0;

#define LED_MAX_LINES                                                          \
  (sizeof(led_config.channel) / sizeof(led_config.channel[0]))

struct LED_COLORING led_coloring = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };

static float diagonale(float x, float y) {
	return sqrtf(x * x + y * y);
}

static uint8_t cmap[UINT8_MAX+1];

/**
 * return the number of led lines support by this hardware
 */
int led_get_max_lines() {
	return LED_MAX_LINES;
}

/**

 * set the led configuration. In the next frame, the configuration will be
 * updated with the latest configuration.
 */
void led_set_config(struct LED_CONFIG *config) {
	xQueueOverwrite(q, (void *) config);
	vTaskResume(taskHandle);
}

#define STRANDCNT (2)

static inline float BYTEtoFLOAT(uint8_t c) {
	return c * 0.003921569f; // (1./255.);
}

static inline float f0to1(float a) {
	return fminf(1.f, fmaxf(a, 0.f));
}

static inline uint8_t FLOATtoBYTE(float c) {
	return 255.f * f0to1(c);
}
static inline float fmax3(float a, float b, float c) {
	return fmaxf(a, fmaxf(b, c));
}

static inline float fmin3(float a, float b, float c) {
	return fminf(a, fminf(b, c));
}

static void hsv(float *r, float *g, float *b) {
	/* convert rgb to hsv */

	float max = fmax3(*r, *g, *b);
	float min = fmin3(*r, *g, *b);

	float hue = 0.;
	if (min != max) {
		if (max == *r) {
			hue = (*g - *b) / (max - min);
		} else if (max == *g) {
			hue = (*b - *r) / (max - min) + 2.f;
		} else if (max == *b) {
			hue = (*r - *g) / (max - min) + 4.f;
		}
		if (hue < 0)
			hue += 6.f;
	}

	float s = 0.f;
	if (max > 0.f)
		s = (max - min) / max;

	float v = max;

	/* apply color change */
	hue = fmodf(hue + led_coloring.hue * 3.f + 6, 6.f);
	s = f0to1(s * led_coloring.saturation);

	/* convert hsv to rgb */
	int hi = floor(hue);
	float f = hue - hi;
	switch (hi) {
	case 1:
		*r = v * (1 - s * f);
		*g = v;
		*b = v * (1 - s);
		break;
	case 2:
		*r = v * (1 - s);
		*g = v;
		*b = v * (1 - s * (1 - f));
		break;
	case 3:
		*r = v * (1 - s);
		*g = v * (1 - s * f);
		*b = v;
		break;
	case 4:
		*r = v * (1 - s * (1 - f));
		*g = v * (1 - s);
		*b = v;
		break;
	case 5:
		*r = v;
		*g = v * (1 - s);
		*b = v * (1 - s * f);
		break;
	default:
		*r = v;
		*g = v * (1 - s * (1 - f));
		*b = v * (1 - s);
		break;
	}
}

static void contrasts(float *r, float *g, float *b) {
	*r = *r * led_coloring.contrast * led_coloring.red_contrast
			+ led_coloring.brightness + led_coloring.red_brightness;
	*g = *g * led_coloring.contrast * led_coloring.green_contrast
			+ led_coloring.brightness + led_coloring.green_brightness;
	*b = *b * led_coloring.contrast * led_coloring.blue_contrast
			+ led_coloring.brightness + led_coloring.blue_brightness;
}

void led_set_color(uint8_t c, uint16_t p, uint8_t r, uint8_t g, uint8_t b) {

	float fr = BYTEtoFLOAT(r);
	float fg = BYTEtoFLOAT(g);
	float fb = BYTEtoFLOAT(b);

	hsv(&fr, &fg, &fb);
	contrasts(&fr, &fg, &fb);

	r = FLOATtoBYTE(fr);
	g = FLOATtoBYTE(fg);
	b = FLOATtoBYTE(fb);

	r = cmap[r];
	g = cmap[g];
	b = cmap[b];

	if (p == led_config.channel[c].black[0]
			|| p == led_config.channel[c].black[1]
			|| p == led_config.channel[c].black[2])
		r = b = g = 0;
	ownled_setPixel(c, p + led_config.prefix_leds, g, b, r);
}

static inline void swap(int *a, int *b) {
	int d = *a;
	*a = *b;
	*b = d;
}

static inline void invert(int *a, int sa) {
	*a = sa - *a - 1;
}

static void led_channel_rgb(int c, int x, int y, uint8_t r, uint8_t g,
		uint8_t b) {
	struct LED_CONFIG_CHANNEL *lc = &led_config.channel[c];

	x -= lc->ox;
	y -= lc->oy;
	int sx = lc->sx;
	int sy = lc->sy;

	// pixel out of range
	if (x < 0 || y < 0 || x >= sx || y >= sy)
		return;

	switch (lc->orientation) {
	case LED_ORI0F_ZIGZAG:
	case LED_ORI0F_MEANDER:
		invert(&x, sx);
		break;
	case LED_ORI90_ZIGZAG:
	case LED_ORI90_MEANDER:
		swap(&sx, &sy);
		swap(&x, &y);
		invert(&x, sx);
		break;
	case LED_ORI90F_ZIGZAG:
	case LED_ORI90F_MEANDER:
		swap(&sx, &sy);
		swap(&x, &y);
		invert(&x, sx);
		invert(&y, sy);
		break;
	case LED_ORI180_ZIGZAG:
	case LED_ORI180_MEANDER:
		invert(&x, sx);
		invert(&y, sy);
		break;
	case LED_ORI180F_ZIGZAG:
	case LED_ORI180F_MEANDER:
		invert(&y, sy);
		break;
	case LED_ORI270_ZIGZAG:
	case LED_ORI270_MEANDER:
		swap(&sx, &sy);
		swap(&x, &y);
		invert(&y, sy);
		break;
	case LED_ORI270F_ZIGZAG:
	case LED_ORI270F_MEANDER:
		swap(&sx, &sy);
		swap(&x, &y);
		break;
	default:
		break;
	}

	switch (lc->orientation) {
	case LED_ORI0_MEANDER:
	case LED_ORI0F_MEANDER:
	case LED_ORI90_MEANDER:
	case LED_ORI90F_MEANDER:
	case LED_ORI180_MEANDER:
	case LED_ORI180F_MEANDER:
	case LED_ORI270_MEANDER:
	case LED_ORI270F_MEANDER:
		if ((y & 1) == 1) {
			x = sx - x - 1;
		}
		break;
	default:
		break;
	}

	led_set_color(c, x + y * sx, r, g, b);
}

void led_rgb(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
	for (int c = 0; c < led_get_max_lines(); c++)
		led_channel_rgb(c, x, y, r, g, b);
}

void led_channel_block(int c, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
	for (int iy = 0; iy < 8; iy++)
		for (int ix = 0; ix < 8; ix++)
			led_channel_rgb(c, x + ix, y + iy, *r++, *g++, *b++);
}

void led_block(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b) {
	for (int c = 0; c < led_get_max_lines(); c++) {
		if (led_config.channel[c].mode == LED_MODE_NETWORK) {
			led_channel_block(c, x, y, r, g, b);
		}
	}
}

static void handleNewConfig() {
	for (int i = 0; i < led_get_max_lines(); i++) {
		WS2812FX_init(i, led_config.channel[i].sx * led_config.channel[i].sy);
		ownled_setSize(i,
				led_config.channel[i].sx * led_config.channel[i].sy
						+ led_config.prefix_leds);
	}
	framerate = led_config.refresh_rate;
}

static void fill(int line, uint8_t r, uint8_t g, uint8_t b) {
	int size = led_config.channel[line].sx * led_config.channel[line].sy;
	while (size > 0) {
		size--;
		led_set_color(line, size, r, g, b);
	}
}

static void fillFadeX(int line) {
	for (int x = 0; x < led_config.channel[line].sx; x++) {
		uint8_t c = x * 255 / (led_config.channel[line].sx - 1);
		for (int y = led_config.channel[line].sy - 1; y >= 0; y--) {
			led_channel_rgb(line, x, y, c, c, c);
		}
	}
}

static void fillFadeY(int line) {
	for (int y = 0; y < led_config.channel[line].sy; y++) {
		uint8_t c = y * 255 / (led_config.channel[line].sy - 1);
		for (int x = led_config.channel[line].sx - 1; x >= 0; x--) {
			led_channel_rgb(line, x, y, c, c, c);
		}
	}
}

static void fillFadeXY(int line) {
	float scale = 255.f
			/ diagonale(led_config.channel[line].sx,
					led_config.channel[line].sy);

	for (int y = 0; y < led_config.channel[line].sy; y++) {
		for (int x = led_config.channel[line].sx - 1; x >= 0; x--) {

			uint8_t c = diagonale(x, y) * scale;
			led_channel_rgb(line, x, y, c, c, c);
		}
	}
}

static void fillLinesX(int line) {
	for (int x = 0; x < led_config.channel[line].sx; x++) {
		uint8_t c = (x & 1) * 255;
		for (int y = led_config.channel[line].sy - 1; y >= 0; y--) {
			led_channel_rgb(line, x, y, c, c, c);
		}
	}
}

static void fillLinesXY(int line) {
	for (int y = 0; y < led_config.channel[line].sy; y++) {
		for (int x = led_config.channel[line].sx - 1; x >= 0; x--) {

			uint8_t c = ((x + y) & 1) * 255;
			led_channel_rgb(line, x, y, c, c, c);
		}
	}
}
static void fillLinesY(int line) {
	for (int y = 0; y < led_config.channel[line].sy; y++) {
		uint8_t c = (y & 1) * 255;
		for (int x = led_config.channel[line].sx - 1; x >= 0; x--) {
			led_channel_rgb(line, x, y, c, c, c);
		}
	}
}

static void fillCorners(int line) {
	for (int y = 0; y < led_config.channel[line].sy; y++) {
		for (int x = led_config.channel[line].sx - 1; x >= 0; x--) {
			if (x == 0 && y == 0)
				led_channel_rgb(line, x, y, 255, 255, 255);
			else if (x == led_config.channel[line].sx - 1 && y == 0)
				led_channel_rgb(line, x, y, 0, 0, 255);
			else if (x == 0 && y == led_config.channel[line].sy - 1)
				led_channel_rgb(line, x, y, 255, 0, 0);
			else if (x == led_config.channel[line].sx - 1
					&& y == led_config.channel[line].sy - 1)
				led_channel_rgb(line, x, y, 0, 255, 0);
			else
				led_channel_rgb(line, x, y, 0, 0, 0);
		}
	}
}

static void fillSquare(int line) {
	for (int y = 0; y < led_config.channel[line].sy; y++) {
		for (int x = led_config.channel[line].sx - 1; x >= 0; x--) {
			if (x == 0 && y != led_config.channel[line].sy - 1)
				led_channel_rgb(line, x, y, 255, 255, 255);
			else if (y == led_config.channel[line].sy - 1)
				led_channel_rgb(line, x, y, 255, 0, 0);
			else if (x == led_config.channel[line].sx - 1)
				led_channel_rgb(line, x, y, 0, 255, 0);
			else if (y == 0)
				led_channel_rgb(line, x, y, 0, 0, 255);
			else
				led_channel_rgb(line, x, y, 0, 0, 0);
		}
	}
}

void fillProductionTest(int line) {

	uint16_t size = led_config.channel[line].sx * led_config.channel[line].sy;

	for (uint16_t i = 0; i < size; i++) {
		uint8_t r = 0, g = 0, b = 0;
		uint32_t c = i + led_counter;
		uint16_t mode = (c / 256) % 5;
		c = c & 255;
		if (mode == 0)
			if ((c / 12) & 1)
				r = g = b = 128;
		if (mode == 1)
			r = c;
		if (mode == 2)
			g = c;
		if (mode == 3)
			b = c;
		if (mode == 4)
			b = g = r = c;
		led_set_color(line, i, r, g, b);
	}
}

#define CUBE_MODE_DURATION 128
#define CUBE_MODES 32
#define CUBE_SX 8
#define CUBE_SY 16
#define CUBE_SZ 8
#define MAXCOLOR 128
#define CUBE_POINT_STEP 50

float frand() {
	float r = rand();
	r = powf(r / RAND_MAX, 2.f) * 0.5f;
	if (rand() > RAND_MAX / 2)
		return 1.f - r;
	else
		return r;
}

void fillCube(int line) {

	static float x = 0, y = 0, z = 0;
	static float gx = 1, gy = 1, gz = 1;
	static int count = 0;

	if (line == 0) {
		if (count >= CUBE_POINT_STEP) {
			count = 0;
			x = gx;
			y = gy;
			z = gz;
			do {
				gx = frand();
				gy = frand();
				gz = frand();
			} while (fabs(gx - x) + fabs(gy - y) + fabs(gz - z) < 0.7);
			ESP_LOGI(TAG, "goal %f %f %f", gx, gy, gz);
		} else
			count++;
		led_cube(x + (gx - x) * count / CUBE_POINT_STEP,
				y + (gy - y) * count / CUBE_POINT_STEP,
				z + (gz - z) * count / CUBE_POINT_STEP);
	}
}

static void task(void *args) {

	TickType_t xLastWakeTime = xTaskGetTickCount() + configTICK_RATE_HZ;
	float incrementMs = 0;
	bool missed = true;
	int count = 0;

	for (;;) {

		/**
		 * start sending LED data
		 */
		ownled_send();

		for (;;) {

			/**
			 * wait for 1/framerate s (in the mean)
			 */
			if (framerate > 0) {
				if (++count >= framerate) {
					ESP_LOGV(TAG, "led wait %f %d %f",
							xTaskGetTickCount() / (float )configTICK_RATE_HZ,
							count, framerate);
					count = 0;
				}

				incrementMs += 1000. / framerate;
				int incrementTicks = incrementMs / portTICK_PERIOD_MS;
				incrementMs -= incrementTicks * portTICK_PERIOD_MS;
				if (incrementTicks < 1)
					incrementTicks = 1;
				vTaskDelayUntil(&xLastWakeTime, incrementTicks);
				if (xLastWakeTime + incrementTicks < xTaskGetTickCount()) {
					if (!missed) {
						ESP_LOGE(TAG, "frame(s) missed %d",
								xTaskGetTickCount() - xLastWakeTime);
						missed = true;
					}
					status_led_top_too_late();
					continue;
				} else if (xTaskGetTickCount() - xLastWakeTime > 2
						&& xLastWakeTime < xTaskGetTickCount()) {
					if (!missed)
						ESP_LOGD(TAG, "frame late %d",
								xTaskGetTickCount() - xLastWakeTime);
					status_led_top_too_late();
					continue;
				}
			} else {
				vTaskSuspend(taskHandle);
			}

			/**
			 * wait for sending LED data finish
			 */
			if (ownled_isFinished() != ESP_OK) {
				ESP_LOGD(TAG, "led write out early");
				status_led_bottom_too_slow();
				continue;
			} else
				status_led_on_time();
			missed = false;
			break;
		}

		/**
		 * generate LED data
		 */
		if (xQueueReceive(q, &led_config, 0) == pdTRUE) {
			handleNewConfig();
		} else {
			led_counter++;

			TickType_t now = xTaskGetTickCount();

			for (int i = 0; i < led_get_max_lines(); i++) {

				switch (led_config.channel[i].mode) {
				case LED_MODE_OFF:
					fill(i, 0, 0, 0);
					break;
				case LED_MODE_NETWORK:
					break;
				case LED_MODE_WHITE:
					fill(i, 255, 255, 255);
					break;
				case LED_MODE_GRAY:
					fill(i, 128, 128, 128);
					break;
				case LED_MODE_RED:
					fill(i, 255, 0, 0);
					break;
				case LED_MODE_YELLOW:
					fill(i, 255, 255, 0);
					break;
				case LED_MODE_GREEN:
					fill(i, 0, 255, 0);
					break;
				case LED_MODE_CYAN:
					fill(i, 0, 255, 255);
					break;
				case LED_MODE_BLUE:
					fill(i, 0, 0, 255);
					break;
				case LED_MODE_MAGENTA:
					fill(i, 255, 0, 255);
					break;
				case LED_MODE_FADE_X:
					fillFadeX(i);
					break;
				case LED_MODE_FADE_Y:
					fillFadeY(i);
					break;
				case LED_MODE_FADE_XY:
					fillFadeXY(i);
					break;
				case LED_MODE_LINES_X:
					fillLinesX(i);
					break;
				case LED_MODE_LINES_Y:
					fillLinesY(i);
					break;
				case LED_MODE_LINES_XY:
					fillLinesXY(i);
					break;
				case LED_MODE_CORNERS:
					fillCorners(i);
					break;
				case LED_MODE_SQUARE:
					fillSquare(i);
					break;
				case LED_MODE_PRODUCTION_TEST:
					fillProductionTest(i);
					break;
				case FX_MODE_BLINK:
					WS2812FX_call(i, WS2812FX_mode_blink);
					break;
				case FX_MODE_BREATH:
					WS2812FX_call(i, WS2812FX_mode_breath);
					break;
				case FX_MODE_COLOR_WIPE:
					WS2812FX_call(i, WS2812FX_mode_color_wipe);
					break;
				case FX_MODE_COLOR_WIPE_INV:
					WS2812FX_call(i, WS2812FX_mode_color_wipe_inv);
					break;
				case FX_MODE_COLOR_WIPE_REV:
					WS2812FX_call(i, WS2812FX_mode_color_wipe_rev);
					break;
				case FX_MODE_COLOR_WIPE_REV_INV:
					WS2812FX_call(i, WS2812FX_mode_color_wipe_rev_inv);
					break;
				case FX_MODE_COLOR_WIPE_RANDOM:
					WS2812FX_call(i, WS2812FX_mode_color_wipe_random);
					break;
				case FX_MODE_RANDOM_COLOR:
					WS2812FX_call(i, WS2812FX_mode_random_color);
					break;
				case FX_MODE_SINGLE_DYNAMIC:
					WS2812FX_call(i, WS2812FX_mode_single_dynamic);
					break;
				case FX_MODE_MULTI_DYNAMIC:
					WS2812FX_call(i, WS2812FX_mode_multi_dynamic);
					break;
				case FX_MODE_RAINBOW:
					WS2812FX_call(i, WS2812FX_mode_rainbow);
					break;
				case FX_MODE_RAINBOW_CYCLE:
					WS2812FX_call(i, WS2812FX_mode_rainbow_cycle);
					break;
				case FX_MODE_SCAN:
					WS2812FX_call(i, WS2812FX_mode_scan);
					break;
				case FX_MODE_DUAL_SCAN:
					WS2812FX_call(i, WS2812FX_mode_dual_scan);
					break;
				case FX_MODE_FADE:
					WS2812FX_call(i, WS2812FX_mode_fade);
					break;
				case FX_MODE_THEATER_CHASE:
					WS2812FX_call(i, WS2812FX_mode_theater_chase);
					break;
				case FX_MODE_THEATER_CHASE_RAINBOW:
					WS2812FX_call(i, WS2812FX_mode_theater_chase_rainbow);
					break;
				case FX_MODE_RUNNING_LIGHTS:
					WS2812FX_call(i, WS2812FX_mode_running_lights);
					break;
					/*
					 case FX_MODE_TWINKLE:
					 WS2812FX_call(i, WS2812FX_mode_twinkle);
					 break;
					 case FX_MODE_TWINKLE_RANDOM:
					 WS2812FX_call(i, WS2812FX_mode_twinkle_random);
					 break;
					 case FX_MODE_TWINKLE_FADE:
					 WS2812FX_call(i, WS2812FX_mode_twinkle_fade);
					 break;
					 case FX_MODE_TWINKLE_FADE_RANDOM:
					 WS2812FX_call(i, WS2812FX_mode_twinkle_fade_random);
					 break;
					 */
				case FX_MODE_SPARKLE:
					WS2812FX_call(i, WS2812FX_mode_sparkle);
					break;
				case FX_MODE_FLASH_SPARKLE:
					WS2812FX_call(i, WS2812FX_mode_flash_sparkle);
					break;
				case FX_MODE_HYPER_SPARKLE:
					WS2812FX_call(i, WS2812FX_mode_hyper_sparkle);
					break;
				case FX_MODE_STROBE:
					WS2812FX_call(i, WS2812FX_mode_strobe);
					break;
				case FX_MODE_STROBE_RAINBOW:
					WS2812FX_call(i, WS2812FX_mode_strobe_rainbow);
					break;
				case FX_MODE_MULTI_STROBE:
					WS2812FX_call(i, WS2812FX_mode_multi_strobe);
					break;
				case FX_MODE_BLINK_RAINBOW:
					WS2812FX_call(i, WS2812FX_mode_blink_rainbow);
					break;
				case FX_MODE_CHASE_WHITE:
					WS2812FX_call(i, WS2812FX_mode_chase_white);
					break;
				case FX_MODE_CHASE_COLOR:
					WS2812FX_call(i, WS2812FX_mode_chase_color);
					break;
				case FX_MODE_CHASE_RANDOM:
					WS2812FX_call(i, WS2812FX_mode_chase_random);
					break;

				case FX_MODE_CHASE_RAINBOW:
					WS2812FX_call(i, WS2812FX_mode_chase_rainbow);
					break;
				case FX_MODE_CHASE_FLASH:
					WS2812FX_call(i, WS2812FX_mode_chase_flash);
					break;
				case FX_MODE_CHASE_FLASH_RANDOM:
					WS2812FX_call(i, WS2812FX_mode_chase_flash_random);
					break;
				case FX_MODE_CHASE_RAINBOW_WHITE:
					WS2812FX_call(i, WS2812FX_mode_chase_rainbow);
					break;
				case FX_MODE_CHASE_BLACKOUT_RAINBOW:
					WS2812FX_call(i, WS2812FX_mode_chase_rainbow_white);
					break;
				case FX_MODE_COLOR_SWEEP_RANDOM:
					WS2812FX_call(i, WS2812FX_mode_color_sweep_random);
					break;
				case FX_MODE_RUNNING_COLOR:
					WS2812FX_call(i, WS2812FX_mode_running_color);
					break;
				case FX_MODE_RUNNING_RED_BLUE:
					WS2812FX_call(i, WS2812FX_mode_running_red_blue);
					break;
					/*
					 case FX_MODE_RUNNING_RANDOM:
					 WS2812FX_call(i, WS2812FX_mode_running_random);
					 break;
					 case FX_MODE_LARSON_SCANNER:
					 WS2812FX_call(i, WS2812FX_mode_breath);
					 break;
					 case FX_MODE_COMET:
					 WS2812FX_call(i, WS2812FX_mode_breath);
					 break;
					 case FX_MODE_FIREWORKS:
					 WS2812FX_call(i, WS2812FX_mode_breath);
					 break;
					 case FX_MODE_FIREWORKS_RANDOM:
					 WS2812FX_call(i, WS2812FX_mode_blink);
					 break;
					 */
				case FX_MODE_MERRY_CHRISTMAS:
					WS2812FX_call(i, WS2812FX_mode_merry_christmas);
					break;
				case FX_MODE_FIRE_FLICKER:
					WS2812FX_call(i, WS2812FX_mode_fire_flicker);
					break;
				case FX_MODE_FIRE_FLICKER_SOFT:
					WS2812FX_call(i, WS2812FX_mode_fire_flicker_soft);
					break;
				case FX_MODE_FIRE_FLICKER_INTENSE:
					WS2812FX_call(i, WS2812FX_mode_fire_flicker_intense);
					break;
				case FX_MODE_CIRCUS_COMBUSTUS:
					WS2812FX_call(i, WS2812FX_mode_circus_combustus);
					break;
				case FX_MODE_HALLOWEEN:
					WS2812FX_call(i, WS2812FX_mode_halloween);
					break;
				case FX_MODE_BICOLOR_CHASE:
					WS2812FX_call(i, WS2812FX_mode_breath);
					break;
				case FX_MODE_TRICOLOR_CHASE:
					WS2812FX_call(i, WS2812FX_mode_tricolor_chase);
					break;
				case FX_MODE_ICU:
					WS2812FX_call(i, WS2812FX_mode_icu);
					break;
				case FX_MODE_PLAYLIST:
					playlist_next(now, i);
					break;
				default:
					break;
				}
			}
		}
	}
}

/**
 * init the led strips and starts a thread for handling them
 */
void led_on() {

	int16_t last = 0x200; // something larger than UINT8_MAX
	for(int16_t c=UINT8_MAX;c>=0;c--) {
		uint8_t ones=1;
		uint8_t l=0;
		for(uint8_t b=0x80;b>0 && ones>0;b>>=1) {
			if((c & b) !=0) {
				l|=b;
				ones--;
			}
		}
		if(c - l < last - c) { // rounding to lower value is smaller than to previous larger one
			cmap[c]=l;
			last=l;
		}
		else {
			cmap[c]=last;
		}
	}

	led_counter = 0;
	ownled_init();
	q = xQueueCreate(1, sizeof(struct LED_CONFIG));
	ESP_ERROR_CHECK(q != NULL ? ESP_OK : ESP_FAIL);
	ESP_ERROR_CHECK(
			xTaskCreate(task, "led_task", 4096, NULL, tskIDLE_PRIORITY + 10, &taskHandle) == pdPASS ? ESP_OK : ESP_FAIL);
}

/**
 * stop the led thread and switched all led strips off
 */

void led_off() {
	vTaskDelete(taskHandle);
	vQueueDelete(q);
	ownled_free();
}

/**
 * set a point in the cube
 */
static uint16_t history_pos[32];
static uint8_t history_line[32];
static int count = 0;

#define SPEED_COLOR 50

void led_cube(float x, float y, float z) {

	if (count == 0) {
		memset(history_pos, 0, 64);
		memset(history_line, 0, 32);
	}
	count++;
	int8_t mode = (count / SPEED_COLOR) % 3;
	float up = (count % SPEED_COLOR) / (float) SPEED_COLOR;
	float down = 1.f - up;
	float r = 0, g = 0, b = 0;
	switch (mode) {
	case 0:
		r = up;
		b = down;
		break;
	case 1:
		r = down;
		g = up;
		break;
	case 2:
		g = down;
		b = up;
		break;
	}

	uint16_t line = floorf(f0to1(z) * led_get_max_lines());
	uint8_t pos = floorf(f0to1(x) * led_config.channel[0].sx)
			+ floorf(f0to1(y) * led_config.channel[0].sy)
					* led_config.channel[0].sx + led_config.prefix_leds;

	if (pos != history_pos[31] || line != history_line[31]) {
		led_set_color(history_line[0], history_pos[0], 0, 0, 0);
		memmove(history_pos, history_pos + 1, 62);
		memmove(history_line, history_line + 1, 31);
		history_pos[31] = pos;
		history_line[31] = line;
	}

	//	ESP_LOGI(TAG, "cube %d %d",cube_last_line,cube_last_pos);
	for (int i = 0; i <= 30; i++) {
		led_set_color(history_line[i], history_pos[i], r * (i + 10),
				g * (i + 10), b * (i + 10));
	}
	for (int i = 30; i <= 31; i++) {
		led_set_color(history_line[i], history_pos[i], 255, 255, 255);
	}
}

void led_cube2(int d, float x, float y, float z) {

	if (d < 0 || d > 31)
		return;

	history_line[d] = floorf(f0to1(z) * led_get_max_lines());
	history_pos[d] = floorf(f0to1(x) * led_config.channel[0].sx)
			+ floorf(f0to1(y) * led_config.channel[0].sy)
					* led_config.channel[0].sx + led_config.prefix_leds;

	if (x < 0.f || y < 0.f || z < 0.f)
		led_set_color(history_line[d], history_pos[d], 0, 0, 0);
	else
		led_set_color(history_line[d], history_pos[d], 255, 255, 255);
}

void led_trigger() {
	vTaskResume(taskHandle);
}
