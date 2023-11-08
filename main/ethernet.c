/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/* ethernet Example

 This example code is in the Public Domain (or CC0 licensed, at your option.)

 Unless required by applicable law or agreed to in writing, this
 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.

 2018 March 26 - this example is based on the one for ESP32-EVB:
 2017 June 15 rudi ;-)
 change log
 edit ETH PHY Example for ESP32-GATEWAY REV B IoT LAN8710 Board with CAN
 - Board use LAN8710 without OSC Power Enable PIN
 - - TLK110 was removed
 - - Kconfig menuconfig TLK110 entry removed cause not need here
 - - we use LAN8720 config for the LAN8710 ( compatible )
 - - Power PIN was not used in shematic and board so not used in code
 - -


 */

#include "ethernet.h"

#include "driver/gpio.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>

#include "status.h"

static const char *TAG = "#ethernet";

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
  char string[64];

  uint8_t mac_addr[6] = {0};
  /* we can get the ethernet driver handle from event data */
  esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

  switch (event_id) {
  case ETHERNET_EVENT_CONNECTED:
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
    ESP_LOGI(TAG, "Ethernet Link Up");
    ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0],
             mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    snprintf(string, sizeof(string), "connected, %s",
             ethernet_status(eth_handle));
    status_ethernet(string);
    break;
  case ETHERNET_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "Ethernet Link Down");
    status_ethernet("disconnected");
    // TODO       ESP_ERROR_CHECK(tcpip_adapter_down(TCPIP_ADAPTER_IF_ETH));
    break;
  case ETHERNET_EVENT_START:
    ESP_LOGI(TAG, "Ethernet Started");
    status_ethernet("start");
    break;
  case ETHERNET_EVENT_STOP:
    ESP_LOGI(TAG, "Ethernet Stopped");
    break;
  default:
    ESP_LOGI(TAG, "unknown Ethernet event %d", event_id);
    break;
  }
}

const char *ethernet_status(esp_eth_handle_t eth_handle) {
  if (eth_handle == NULL)
    return "NA";

  eth_duplex_t duplex;
  eth_speed_t speed;

  ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_G_DUPLEX_MODE, &duplex));
  ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_G_SPEED, &speed));

  if (speed == ETH_SPEED_10M) {
    if (duplex == ETH_DUPLEX_HALF)
      return "10M, half-duplex";
    else
      return "10M, full-duplex";
  } else {
    if (duplex == ETH_DUPLEX_HALF)
      return "100M, half-duplex";
    else
      return "100M, full-duplex";
  }
}

static esp_eth_handle_t eth_handle = NULL;

void ethernet_on() {

#if CONFIG_ETH_ENABLED
  ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                             &eth_event_handler, NULL));
  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = CONFIG_CONTROLLER_ETH_PHY_ADDR;
  phy_config.reset_gpio_num = CONFIG_CONTROLLER_ETH_PHY_RST_GPIO;

  vTaskDelay(pdMS_TO_TICKS(10));

#if CONFIG_CONTROLLER_USE_INTERNAL_ETHERNET
  mac_config.smi_mdc_gpio_num = CONFIG_CONTROLLER_ETH_MDC_GPIO;
  mac_config.smi_mdio_gpio_num = CONFIG_CONTROLLER_ETH_MDIO_GPIO;
  esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
#if CONFIG_CONTROLLER_ETH_PHY_IP101
  esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_CONTROLLER_ETH_PHY_RTL8201
  esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_CONTROLLER_ETH_PHY_LAN8720
  esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
#elif CONFIG_CONTROLLER_ETH_PHY_DP83848
  esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#endif
#endif
  eth_handle = NULL;
  esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);

  ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t *eth_netif = esp_netif_new(&cfg);

  /* attach Ethernet driver to TCP/IP stack */
  ESP_ERROR_CHECK(
      esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

  /* start Ethernet driver state machine */
  ESP_ERROR_CHECK(esp_eth_start(eth_handle));

#endif
}

void ethernet_off() {
  if (eth_handle != NULL) {
    esp_eth_stop(eth_handle);
    eth_handle = NULL;
  }
}
