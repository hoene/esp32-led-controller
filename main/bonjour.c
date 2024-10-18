/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * mdns.c
 *
 *  Created on: Feb 7, 2019
 *      Author: hoene
 */

#include "bonjour.h"

#include "config.h"

#include <lwip/apps/mdns.h>
// #include "..mdns.h"
#include "esp_mac.h"

#if 0
void bonjour_on() {
  char name[80];
  uint8_t mac[6];

  // initialize mDNS service
  ESP_ERROR_CHECK(mdns_init());

  ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
  snprintf(name, sizeof(name), "symonics%02x%02x%02x%02x%02x%02x", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);

  // set hostname
  ESP_ERROR_CHECK(mdns_hostname_set(name));
  // set default instance

  snprintf(name, sizeof(name),
           "Symonics Lighting Controller %02X%02X%02X%02X%02X%02X", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);
  ESP_ERROR_CHECK(mdns_instance_name_set(name));

  // add our services
  ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
  ESP_ERROR_CHECK(
      mdns_service_add(NULL, "_rtp", "_udp", config_get_udp_port(), NULL, 0));
  ESP_ERROR_CHECK(mdns_service_add(NULL, "_artnet", "_udp",
                                   config_get_udp_port(), NULL, 0));
}

void bonjour_off() { mdns_free(); }
#endif