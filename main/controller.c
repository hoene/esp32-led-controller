/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#include "controller.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_flash_encrypt.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include "bonjour.h"
#include "config.h"
#include "decoding.h"
#include "ethernet.h"
#include "filesystem.h"
#include "home.h"
#include "led.h"
#include "mjpeg.h"
#include "mysntp.h"
#include "rtp.h"
#include "status.h"
#include "udp.h"
#include "web.h"
#include "wifi.h"

static const char *TAG = "#controller";

/** Print chip information */
static void printChipInfo() {
  ESP_LOGI(TAG, "Symonics Matrix LED Controller");
  ESP_LOGI(TAG, "IDF version %s", esp_get_idf_version());
  ESP_LOGI(TAG, "project/date/time %s", config_get_compile_date());
  ESP_LOGI(TAG, "git %s", config_get_version());
  ESP_LOGI(TAG, "flash encryption %d", esp_flash_encryption_enabled());

  const esp_partition_t *last = esp_ota_get_last_invalid_partition();
  if (last)
    ESP_LOGI(TAG, "last invalid partition %08lX:%s", last->address, last->label);

  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  esp_base_mac_addr_set(
      mac); // get rid of "Base MAC address is not set, read default base MAC
            // address from BLK0 of EFUSE" message
  ESP_LOGI(TAG, "MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);

  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ESP_LOGD(TAG, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
           chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  ESP_LOGD(TAG, "silicon revision %d, ", chip_info.revision);

    uint32_t size_flash_chip;
    esp_flash_get_size(NULL, &size_flash_chip);

  ESP_LOGD(TAG, "%luMB %s flash", size_flash_chip / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded"
                                                         : "external");

  const esp_partition_t *configured = esp_ota_get_boot_partition();
  if (configured == NULL) {
    ESP_LOGE(TAG, "No boot partition");
  }
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (configured == NULL) {
    ESP_LOGE(TAG, "No running partition");
  }

  if (running != NULL && configured != NULL) {
    if (configured != running) {
      ESP_LOGI(TAG,
               "Configured OTA boot partition at offset 0x%08lx, but running "
               "from offset 0x%08lx",
               configured->address, running->address);
      ESP_LOGI(TAG, "(This can happen if either the OTA boot data or preferred "
                    "boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08lx)",
             running->type, running->subtype, running->address);
  }
}

void networking_on() {
  // Initialize TCP/IP network interface (should be called only once in
  // application)
  ESP_ERROR_CHECK(esp_netif_init());
  // Create default event loop that running in background
  ESP_ERROR_CHECK(esp_event_loop_create_default());
}

void networking_off() {}

void app_main() {

  printChipInfo();

  // disable WLAN and other log messages
  esp_log_level_set("wifi", ESP_LOG_WARN);
  esp_log_level_set("httpd-freertos", ESP_LOG_WARN);

  // booting
  status_init();
  filesystem_on();
  mjpeg_on();
  decoding_on();
  rtp_on();
  config_init();
  networking_on();
  wifi_on();
  ethernet_on();
  led_on(); // require config
  config_update_channels();
  udp_on();
  bonjour_on();
  home_on();
  mysntp_on();

  for (;;) {
    udp_process(1);
  }

  mysntp_off();
  home_off();
  bonjour_off();
  led_off();
  ethernet_off();
  wifi_off();
  rtp_off();
  networking_off();
  decoding_off();
  mjpeg_off();
  filesystem_off();

  ESP_LOGI(TAG, "Restarting now.\n");
  fflush(stdout);
  esp_restart();
}
