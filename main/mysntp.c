/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * smtp.c
 *
 *  Created on: 26.05.2019
 *      Author: hoene
 */

#include "mysntp.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "../esp-idf/components/lwip/lwip/src/include/lwip/apps/sntp.h"
#include "nvs_flash.h"

static const char *TAG = "#mysntp";

void mysntp_on()
{
	char ntp_server[30] = "pool.ntp.org";
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, ntp_server);
    sntp_init();
}

void mysntp_retry()
{
	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);
	if(tv_now.tv_sec < 365*24*60*60) {
		mysntp_off();
		mysntp_on();
	}
}

void mysntp_off()
{
	sntp_stop();
}

