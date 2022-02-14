/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * ownled.c
 *
 *  Created on: 21.10.2018
 *      Author: hoene
 */

#include "ownled.h"

#include "controller.h"
#include "driver/rmt.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"

#include "gamma.c"

#define RGB_BYTES_8 (1)
#define RGB_BYTES_24 (3)
#define RGB_BYTES_48 (6)
#define RGB_BITS_8 (RGB_BYTES_8 * 8)
#define RGB_BITS_24 (RGB_BYTES_24 * 8)
#define RGB_BITS_48 (RGB_BYTES_48 * 8)

static const char *TAG = "#ownled";

#define RMT_MEM_BLOCK_BYTE_NUM (4 * RMT_MEM_ITEM_NUM)

/**
 *  define frequency,
 *  length of SLED impulses
 */

/** Frequency of the quartz */
#define FREQ_INPUT (8e7)

/** Frequency of the SLED clock */
#define DEFAULT_FREQ_OUTPUT (8e5)

/** ticks per SLED clock period */
#define DEFAULT_PULSE_LENGTH (FREQ_INPUT / DEFAULT_FREQ_OUTPUT)

/** high impulse of the data bit "one" has the relative length of */
//#define DEFAULT_ONE_PULSE_LENGTH 0.60
#define DEFAULT_ONE_PULSE_LENGTH (0.60)

/** high impulse of the data bit "zero" has the relative length of */
//#define DEFAULT_ZERO_PULSE_LENGTH 0.30
#define DEFAULT_ZERO_PULSE_LENGTH (0.30)

/**
 * config data
 */

static enum OWNLED_COLOR_ORDER color_order = OWNLED_GRB;

/**
 * RMT structure for one (high) and zero (low) impulse initialized by default
 * values
 */
IRAM_ATTR rmt_item32_t cs8812_high = {
	.duration0 = DEFAULT_PULSE_LENGTH * DEFAULT_ONE_PULSE_LENGTH,
	.level0 = 1,
	.duration1 = DEFAULT_PULSE_LENGTH * (1 - DEFAULT_ONE_PULSE_LENGTH),
	.level1 = 0};

IRAM_ATTR rmt_item32_t cs8812_low = {
	.duration0 = DEFAULT_PULSE_LENGTH * DEFAULT_ZERO_PULSE_LENGTH,
	.level0 = 1,
	.duration1 =
		DEFAULT_PULSE_LENGTH * (1 - DEFAULT_ZERO_PULSE_LENGTH),
	.level1 = 0,
};

/**
 * set the pulse length and widths
 */
esp_err_t ownled_set_pulses(uint32_t frequency, uint8_t one, uint8_t zero)
{
	if (one < zero || one < 55 || one > 90 || zero < 10 || zero > 45)
		return ESP_ERR_INVALID_ARG;
	if (frequency * 2 < DEFAULT_FREQ_OUTPUT || frequency * 0.67 > DEFAULT_FREQ_OUTPUT)
		return ESP_ERR_INVALID_ARG;

	double factor = FREQ_INPUT / (frequency * 100.);
	cs8812_high.duration0 = factor * one;
	cs8812_high.duration1 = factor * (100 - one);
	cs8812_low.duration0 = factor * zero;
	cs8812_low.duration1 = factor * (100 - zero);

	return ESP_OK;
}

uint32_t ownled_get_pulse_frequency()
{
	double pulse_length = cs8812_high.duration0 + cs8812_high.duration1;
	return 10000. * round(FREQ_INPUT / (pulse_length * 10000.));
}

uint8_t ownled_get_pulse_one()
{
	double pulse_length = cs8812_high.duration0 + cs8812_high.duration1;
	return round(cs8812_high.duration0 / pulse_length * 20.) * 5;
}

uint8_t ownled_get_pulse_zero()
{
	float pulse_length = cs8812_high.duration0 + cs8812_high.duration1;
	return round(cs8812_low.duration0 / pulse_length * 20.) * 5;
}

/**
 * fastrmt structure
 */

#define MAXIMAL_LINES (8)
#define BYTES_PER_LINE (616 * 3)

extern struct FASTRMI_DATA
{
	uint16_t counter;
	uint16_t length;
	uint32_t baseAddress;
	uint32_t mask;
	uint32_t intmask;
	uint8_t *input;
} fastrmi_para[MAXIMAL_LINES];

extern uint8_t fastrmi_bytes[BYTES_PER_LINE * MAXIMAL_LINES];

extern int32_t _l5_counter, _l5_flags;

static intr_handle_t isr_handle;

/**
 * alloc the structure for RMT handling
 */

static struct
{
	rmt_channel_t rmtChannel;
	uint16_t numBytes;
	uint8_t *frameBuffer;
	uint8_t *rmtBuffer;
} lines[MAXIMAL_LINES];

/**
 * settings of different hardware
 */

static const uint8_t version_gpio[CONFIG_CONTROLLER_LED_LINES] = {CONFIG_CONTROLLER_LED_LINE0
#if CONFIG_CONTROLLER_LED_LINES >= 2
																  ,
																  CONFIG_CONTROLLER_LED_LINE1
#endif
#if CONFIG_CONTROLLER_LED_LINES >= 3
																  ,
																  CONFIG_CONTROLLER_LED_LINE2
#endif
#if CONFIG_CONTROLLER_LED_LINES >= 4
																  ,
																  CONFIG_CONTROLLER_LED_LINE3
#endif
#if CONFIG_CONTROLLER_LED_LINES >= 5
																  ,
																  CONFIG_CONTROLLER_LED_LINE4
#endif
#if CONFIG_CONTROLLER_LED_LINES >= 6
																  ,
																  CONFIG_CONTROLLER_LED_LINE5
#endif
#if CONFIG_CONTROLLER_LED_LINES >= 7
																  ,
																  CONFIG_CONTROLLER_LED_LINE6
#endif
#if CONFIG_CONTROLLER_LED_LINES >= 8
																  ,
																  CONFIG_CONTROLLER_LED_LINE7
#endif

};

/*
 *
 */
uint8_t ownled_getChannels()
{
	return CONFIG_CONTROLLER_LED_LINES;
}

uint8_t ownled_getBlocksize()
{
	switch (CONFIG_CONTROLLER_LED_LINES)
	{
	case 1:
		return 8;
	case 2:
		return 4;
	case 3:
	case 4:
		return 2;
	default:
		return 1;
	}
}

void ownled_init()
{

	if (ownled_getChannels() == 0)
		return;

	//	ownled_set_default();

	for (uint8_t i = 0; i < ownled_getChannels(); i++)
	{

		lines[i].rmtChannel = i * ownled_getBlocksize();
		lines[i].frameBuffer = NULL;
		lines[i].numBytes = 0;

		rmt_config_t config = {.rmt_mode = RMT_MODE_TX, .channel = lines[i].rmtChannel,
							   .clk_div = 1, // 80 MHz (APB CLK typical)
							   .gpio_num = version_gpio[i],
							   .mem_block_num = ownled_getBlocksize(),
							   .tx_config = {
								   .loop_en = false,
								   .carrier_en = false,
								   .carrier_freq_hz = 0,
								   .carrier_duty_percent = 0,
								   .carrier_level = RMT_CARRIER_LEVEL_HIGH,
								   .idle_level = RMT_IDLE_LEVEL_LOW,
								   .idle_output_en = true,
							   }};

		gpio_pad_select_gpio(version_gpio[i]);
		gpio_set_level(version_gpio[i], 0);
		gpio_set_direction(version_gpio[i],
						   GPIO_MODE_OUTPUT);

		ESP_ERROR_CHECK(rmt_config(&config));
		ESP_ERROR_CHECK(
			rmt_set_tx_thr_intr_en(lines[i].rmtChannel, true,
								   ownled_getBlocksize() * RMT_MEM_ITEM_NUM / 2));
		ESP_LOGD(TAG, "init %d config %d %p", i,
				 version_gpio[i],
				 &RMT.tx_lim_ch[i]);
	}
	ESP_ERROR_CHECK(esp_intr_alloc(ETS_RMT_INTR_SOURCE,
								   ESP_INTR_FLAG_LEVEL5 | ESP_INTR_FLAG_IRAM,
								   NULL, NULL, &isr_handle));
}

extern esp_err_t ownled_setColorOrder(enum OWNLED_COLOR_ORDER order)
{
	ESP_LOGI(TAG, "set led order %d->%d", color_order, order);
	switch (order)
	{
	case OWNLED_RGB:
	case OWNLED_RBG:
	case OWNLED_GRB:
	case OWNLED_GBR:
	case OWNLED_BRG:
	case OWNLED_BGR:
	case OWNLED_RGB_FB:
	case OWNLED_RBG_FB:
	case OWNLED_GRB_FB:
	case OWNLED_GBR_FB:
	case OWNLED_BRG_FB:
	case OWNLED_BGR_FB:
	case OWNLED_BW:
	case OWNLED_BW_FB:
	case OWNLED_48:
	case OWNLED_48_FB:
		color_order = order;
		ESP_LOGI(TAG, "set led order %d=%d", color_order, order);
		return ESP_OK;
	}
	return ESP_ERR_INVALID_ARG;
}

enum OWNLED_COLOR_ORDER ownled_getColorOrder()
{
	ESP_LOGI(TAG, "get led order %d", color_order);
	return color_order;
}

void ownled_set_default()
{
	cs8812_high.duration0 = DEFAULT_PULSE_LENGTH * DEFAULT_ONE_PULSE_LENGTH;
	cs8812_high.duration1 = DEFAULT_PULSE_LENGTH * (1 - DEFAULT_ONE_PULSE_LENGTH);
	cs8812_low.duration0 = DEFAULT_PULSE_LENGTH * DEFAULT_ZERO_PULSE_LENGTH;
	cs8812_low.duration1 = DEFAULT_PULSE_LENGTH * (1 - DEFAULT_ZERO_PULSE_LENGTH);
	//	ESP_LOGI(TAG, "led order default %d", color_order);
	//	color_order = OWNLED_GRB;
}

extern void ownled_setBytes(uint8_t c, uint16_t pos, uint8_t sw)
{
	if (c >= MAXIMAL_LINES || pos >= lines[c].numBytes)
		return;
	uint8_t *s = lines[c].frameBuffer;
	if (s == NULL)
	{
		s = lines[c].rmtBuffer;
	}
	s[pos] = sw;
}

extern void ownled_setPixel(uint8_t c, uint16_t pos, uint8_t g, uint8_t b,
							uint8_t r)
{
	if (c >= MAXIMAL_LINES)
		return;

	uint8_t *s = lines[c].frameBuffer;
	if (s == NULL)
	{
		s = lines[c].rmtBuffer;
	}

	if (color_order == OWNLED_BW || color_order == OWNLED_BW_FB)
	{
		if (pos >= lines[c].numBytes)
			return;

		s += pos;

		switch (pos % 12) /* warum? */
		{
		case 6:
		case 9:
			s += 2;
			break;
		case 8:
		case 11:
			s -= 2;
			break;
		}
	}
	else if (color_order == OWNLED_48 || color_order == OWNLED_48_FB)
	{
		if (pos * RGB_BYTES_48 >= lines[c].numBytes)
			return;

		s += pos * RGB_BYTES_48;
	}
	else
	{
		if (pos * RGB_BYTES_24 >= lines[c].numBytes)
			return;

		s += pos * RGB_BYTES_24;
	}

	switch (color_order)
	{
	case OWNLED_BW:
	case OWNLED_BW_FB:
		s[0] = (2126 * r + 7152 * g + 722 * b) / 10000;
		break;
	case OWNLED_48:
	case OWNLED_48_FB:
		s[0] = lookupGamma[r] >> 8;
		s[1] = lookupGamma[r];
		s[2] = lookupGamma[g] >> 8;
		s[3] = lookupGamma[g];
		s[4] = lookupGamma[b] >> 8;
		s[5] = lookupGamma[b];
		break;
	case OWNLED_RGB:
	case OWNLED_RGB_FB:
		s[0] = r;
		s[1] = g;
		s[2] = b;
		break;
	case OWNLED_RBG:
	case OWNLED_RBG_FB:
		s[0] = r;
		s[1] = b;
		s[2] = g;
		break;
	case OWNLED_GRB:
	case OWNLED_GRB_FB:
		s[0] = g;
		s[1] = r;
		s[2] = b;
		break;
	case OWNLED_GBR:
	case OWNLED_GBR_FB:
		s[0] = g;
		s[1] = b;
		s[2] = r;
		break;
	case OWNLED_BRG:
	case OWNLED_BRG_FB:
		s[0] = b;
		s[1] = r;
		s[2] = g;
		break;
	case OWNLED_BGR:
	case OWNLED_BGR_FB:
		s[0] = b;
		s[1] = g;
		s[2] = r;
		break;
	}
}

void ownled_send()
{
	int item_num = ownled_getBlocksize() * RMT_MEM_ITEM_NUM;
	uint8_t bytesPre = item_num >> 3;

	for (uint8_t i = 0; i < ownled_getChannels(); i++)
	{
		if (lines[i].frameBuffer)
		{
			memcpy(lines[i].rmtBuffer, lines[i].frameBuffer,
				   lines[i].numBytes);
		}
	}

	for (uint8_t i = 0; i < ownled_getChannels(); i++)
	{

		/** fill initial bytes int RMT buffer*/
		uint8_t *src = lines[i].rmtBuffer;
		if (src == NULL)
			continue;

		rmt_item32_t *dst = (void *)(0x3ff56800 + RMT_MEM_BLOCK_BYTE_NUM * lines[i].rmtChannel);

		ESP_LOGD(TAG, "%d %d %d %08X %08X %08X %d %d %p %p %p", _l5_counter, i,
				 lines[i].rmtChannel, _l5_flags, fastrmi_para[i].baseAddress,
				 fastrmi_para[i].mask, fastrmi_para[i].counter,
				 fastrmi_para[i].length, src, dst, &RMT.conf_ch[i]);

		for (int8_t j = 0; j < bytesPre; j++)
		{
			uint8_t s = *src++;
			if (j == lines[i].numBytes)
			{
				dst->val = 0;
				break;
			}

			*dst++ = s & 0x80 ? cs8812_high : cs8812_low;
			*dst++ = s & 0x40 ? cs8812_high : cs8812_low;
			*dst++ = s & 0x20 ? cs8812_high : cs8812_low;
			*dst++ = s & 0x10 ? cs8812_high : cs8812_low;
			*dst++ = s & 0x08 ? cs8812_high : cs8812_low;
			*dst++ = s & 0x04 ? cs8812_high : cs8812_low;
			*dst++ = s & 0x02 ? cs8812_high : cs8812_low;
			*dst++ = s & 0x01 ? cs8812_high : cs8812_low;
		}

		/** fill remaining bytes into fastrmi buffer */
		fastrmi_para[i].counter = bytesPre;
		fastrmi_para[i].length = lines[i].numBytes;
		fastrmi_para[i].baseAddress = 0x3ff56800 + RMT_MEM_BLOCK_BYTE_NUM * lines[i].rmtChannel;
		fastrmi_para[i].mask =
			(ownled_getBlocksize() *
			 RMT_MEM_BLOCK_BYTE_NUM) -
			1;
		fastrmi_para[i].intmask = 0x1000000 << lines[i].rmtChannel;
		fastrmi_para[i].input = lines[i].rmtBuffer;

		/** start transmitting */
		ESP_ERROR_CHECK(rmt_tx_start(lines[i].rmtChannel, true));
	}
}

esp_err_t ownled_isFinished()
{
	for (uint8_t i = 0; i < ownled_getChannels(); i++)
	{
		if (fastrmi_para[i].length != 0 && fastrmi_para[i].counter < fastrmi_para[i].length)
			return ESP_ERR_TIMEOUT;
	}
	return ESP_OK;
}

void ownled_setSize(uint8_t channel, uint16_t _numPixels)
{
	uint16_t numBytes = 0;

	if (channel >= ownled_getChannels())
		return;

	free(lines[channel].frameBuffer);
	switch (color_order)
	{
	case OWNLED_RGB:
	case OWNLED_RBG:
	case OWNLED_GRB:
	case OWNLED_GBR:
	case OWNLED_BRG:
	case OWNLED_BGR:
		lines[channel].frameBuffer = NULL;
		numBytes = _numPixels * RGB_BYTES_24;
		break;
	case OWNLED_BW:
		lines[channel].frameBuffer = NULL;
		numBytes = _numPixels * RGB_BYTES_8;
		break;
	case OWNLED_48:
		lines[channel].frameBuffer = NULL;
		numBytes = _numPixels * RGB_BYTES_48;
		break;
	case OWNLED_RGB_FB:
	case OWNLED_RBG_FB:
	case OWNLED_GRB_FB:
	case OWNLED_GBR_FB:
	case OWNLED_BRG_FB:
	case OWNLED_BGR_FB:
		numBytes = _numPixels * RGB_BYTES_24;
		lines[channel].frameBuffer = malloc(numBytes);
		memset(lines[channel].frameBuffer, 0, numBytes);
		break;
	case OWNLED_48_FB:
		numBytes = _numPixels * RGB_BYTES_48;
		lines[channel].frameBuffer = malloc(numBytes);
		memset(lines[channel].frameBuffer, 0, numBytes);
		break;
	case OWNLED_BW_FB:
		numBytes = _numPixels * RGB_BYTES_8;
		lines[channel].frameBuffer = malloc(numBytes);
		memset(lines[channel].frameBuffer, 0, numBytes);
		break;
	}

	if (numBytes > BYTES_PER_LINE)
		numBytes = BYTES_PER_LINE;
	lines[channel].numBytes = numBytes;

	/* TODO Warum der Scheiss hier? */
	int blocksize = ownled_getBlocksize() * BYTES_PER_LINE;
	lines[channel].rmtBuffer = fastrmi_bytes + blocksize * channel;
	ESP_LOGD(TAG, "rmtBuffer %p %p %d", fastrmi_bytes, lines[channel].rmtBuffer,
			 blocksize);
	memset(lines[channel].rmtBuffer, 0, blocksize);
}

void ownled_free()
{
	for (uint8_t i = 0; i < ownled_getChannels(); i++)
	{
		rmt_tx_stop(lines[i].rmtChannel);
		ESP_LOGD(TAG, "free %d %d", i, lines[i].rmtChannel);
		ESP_ERROR_CHECK(rmt_set_tx_thr_intr_en(lines[i].rmtChannel, false, 0));
		ESP_ERROR_CHECK(rmt_set_tx_intr_en(lines[i].rmtChannel, false));
		free(lines[i].frameBuffer);
		lines[i].frameBuffer = NULL;
		lines[i].numBytes = 0;
	}
	rmt_isr_deregister(isr_handle);
}
