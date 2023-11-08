/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * playlist.h
 *
 *  Created on: 13.07.2019
 *      Author: hoene
 */

#ifndef MAIN_PLAYLIST_H_
#define MAIN_PLAYLIST_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdint.h>

struct PLAYLIST_ENTRY {
  char filename[16];
  uint32_t duration;
  enum uint8_t {
    PLAYLIST_NONE = 0,
    PLAYLIST_BLANK,
    PLAYLIST_FIXED,
    PLAYLIST_FROMLEFT,
    PLAYLIST_FROMRIGHT,
    PLAYLIST_FROMTOP,
    PLAYLIST_FROMBOTTOM,
    PLAYLIST_LAST
  } mode;
};

void playlist_defaultConfig();
void playlist_readConfig(nvs_handle handle);
void playlist_writeConfig(nvs_handle handle);
void playlist_next(TickType_t now, int8_t line);

uint8_t playlist_getEntries();
void playlist_get(int8_t pos, struct PLAYLIST_ENTRY *entry);
esp_err_t playlist_set(int8_t pos, const struct PLAYLIST_ENTRY *entry);

#endif /* MAIN_PLAYLIST_H_ */
