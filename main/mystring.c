/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * status.c
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#include "mystring.h"

#include <stdarg.h>
#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

// static const char *TAG = "#mystring.c";

#define RESIZE_INCREMENT (64)

MYSTRING mystring_new(const char *in) {
  MYSTRING result = malloc(sizeof(struct STRING));
  ESP_ERROR_CHECK(result ? ESP_OK : ESP_ERR_NO_MEM);

  result->allocated = RESIZE_INCREMENT;
  result->len = 0;
  result->buffer = malloc(result->allocated);
  ESP_ERROR_CHECK(result->buffer ? ESP_OK : ESP_ERR_NO_MEM);

  result->buffer[0] = 0;

  mystring_append(result, in);
  return result;
}

void mystring_add(MYSTRING result, char c) {
  if (result->len + 1 >= result->allocated) {
    result->allocated += RESIZE_INCREMENT;
    result->buffer = realloc(result->buffer, result->allocated);
    ESP_ERROR_CHECK(result->buffer ? ESP_OK : ESP_ERR_NO_MEM);
  }
  result->buffer[result->len] = c;
  result->len++;
  result->buffer[result->len] = 0;
}

void mystring_append(MYSTRING result, const char *in) {
  if (in == NULL)
    return;

  while (*in) {
    mystring_add(result, *in);
    in++;
  }
}

void mystring_printf(MYSTRING result, const char *s, ...) {
  char tmp[256];
  va_list valist;

  va_start(valist, s);

  int res = vsnprintf(tmp, sizeof(tmp), s, valist);
  assert(res < sizeof(tmp) && res >= 0);
  mystring_append(result, tmp);
}

void mystring_json(MYSTRING result, const char *s) {
  if (s == NULL)
    return;

  char tmp[7];

  while (*s) {
    switch (*s) {
    case '"':
      mystring_append(result, "\\\"");
      break;
    case '\\':
      mystring_append(result, "\\\\");
      break;
    case '\b':
      mystring_append(result, "\\b");
      break;
    case '\f':
      mystring_append(result, "\\f");
      break;
    case '\n':
      mystring_append(result, "\\n");
      break;
    case '\r':
      mystring_append(result, "\\r");
      break;
    case '\t':
      mystring_append(result, "\\t");
      break;
    default:
      if (*s <= '\x1f') {
        sprintf(tmp, "\\u%04x", *s);
        mystring_append(result, tmp);
      } else {
        mystring_add(result, *s);
      }
    }
    s++;
  }
}

void mystring_free(MYSTRING result) {
  free(result->buffer);
  free(result);
}
