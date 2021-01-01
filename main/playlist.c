/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * playlist.c
 *
 *  Created on: 13.07.2019
 *      Author: hoene
 */


#include "esp_err.h"
#include "esp_log.h"
#include <string.h>
#include "driver/gpio.h"
#include "playlist.h"

#define PLAYLIST_ENTRIES 8
static struct PLAYLIST_ENTRY playlist[PLAYLIST_ENTRIES];

static int8_t pos_in_playlist;
static int32_t pos_in_media;
static TickType_t pos_now;

uint8_t playlist_getEntries() {
	return PLAYLIST_ENTRIES;
}

esp_err_t playlist_set(int8_t pos, const struct PLAYLIST_ENTRY *entry) {
	if(pos < 0 || pos >= PLAYLIST_ENTRIES)
		return  ESP_ERR_INVALID_ARG;

	if(entry->duration > 1000000)
		return  ESP_ERR_INVALID_ARG;
	if(entry->mode < 0 || entry->mode >= PLAYLIST_LAST)
		return  ESP_ERR_INVALID_ARG;

	playlist[pos] = *entry;
	playlist[pos].filename[sizeof(playlist[pos].filename)-1] = 0;
	return ESP_OK;
}

void playlist_get(int8_t pos, struct PLAYLIST_ENTRY *entry) {
	ESP_ERROR_CHECK(
			pos>=0 && pos < PLAYLIST_ENTRIES ? ESP_OK : ESP_ERR_INVALID_ARG);
	*entry = playlist[pos];
}

void playlist_defaultConfig() {
	memset(playlist, 0, sizeof(playlist));
}

void playlist_readConfig(nvs_handle my_handle) {
	for (int8_t i = 0; i < PLAYLIST_ENTRIES; i++) {
		char varname[32];
		snprintf(varname, sizeof(varname), "playlist%dfile", i);
		size_t size = sizeof(playlist[i].filename);
		nvs_get_blob(my_handle, varname, playlist[i].filename, &size);
		snprintf(varname, sizeof(varname), "playlist%dduration", i);
		nvs_get_u32(my_handle, varname, &playlist[i].duration);
		snprintf(varname, sizeof(varname), "playlist%dmode", i);
		nvs_get_u8(my_handle, varname, (uint8_t*) &playlist[i].mode);
	}
}

void playlist_writeConfig(nvs_handle my_handle) {
	for (int8_t i = 0; i < PLAYLIST_ENTRIES; i++) {
		char varname[32];
		snprintf(varname, sizeof(varname), "playlist%dfile", i);
		nvs_set_blob(my_handle, varname, playlist[i].filename,
				sizeof(playlist[i].filename));
		snprintf(varname, sizeof(varname), "playlist%dduration", i);
		nvs_set_u32(my_handle, varname, playlist[i].duration);
		snprintf(varname, sizeof(varname), "playlist%dmode", i);
		nvs_set_u8(my_handle, varname, (uint8_t) playlist[i].mode);
	}
}

void playlist_on() {
	playlist_defaultConfig();
	pos_in_playlist = -1;
}

static void getNextFile() {
}
static void setNextImage() {
}

void playlist_next(TickType_t now, int8_t line) {

	if(pos_in_playlist < 0) {
		/* playlist is called for the first time */
		pos_in_playlist = 0;
		pos_in_media = 0;
		getNextFile(pos_in_playlist);
	}
	else if(now != pos_now) {
		/* playlist is called with a time increment */
		pos_in_media += (now - pos_now) * 1000.f / configTICK_RATE_HZ;

		if(pos_in_media >= playlist[pos_in_playlist].duration) {
			/* media is finished, search for next */
			do {
				pos_in_playlist++;
				if (pos_in_playlist == PLAYLIST_ENTRIES) {
					pos_in_playlist = 0;
					break;
				}
			} while (playlist[pos_in_playlist].mode == PLAYLIST_NONE);
			pos_in_media = 0;
			getNextFile(pos_in_playlist);
		}
	}

	pos_now = now;
	setNextImage(line, pos_in_media);
}

void playlist_off() {

}

