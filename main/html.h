/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#ifndef MAIN_HTML_H_
#define MAIN_HTML_H_

#include <stdint.h>

struct HTML_FILE {
    const char *name;
    const uint16_t *size;
    const uint8_t *data;
};

extern struct HTML_FILE html_files[];

#endif