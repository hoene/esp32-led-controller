/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * mystring.h
 *
 *  Created on: 25.08.2018
 *      Author: hoene
 */

#ifndef MYSTRING_H_
#define MYSTRING_H_

struct STRING {
  char *buffer;
  int len;
  int allocated;
};

typedef struct STRING *MYSTRING;

MYSTRING mystring_new(const char *);
void mystring_add(MYSTRING, char);
void mystring_append(MYSTRING, const char *);
void mystring_json(MYSTRING, const char *);
void mystring_printf(MYSTRING result, const char *s, ...);
void mystring_free(MYSTRING);

#endif /* MYSTRING_H_ */
