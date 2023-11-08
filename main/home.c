/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * config.c
 *
 *  Created on: 29.07.2018
 *      Author: hoene
 */

#include "home.h"

#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "esp_err.h"
#include "esp_log.h"
#include "mbedtls/md.h"

#include "config.h"
#include "rtp.h"
#include "status.h"
#include "wifi.h"

static const char *TAG = "#home";

static TaskHandle_t taskHandle;
int home_socket = -1;

static struct __attribute__((packed)) {
  uint8_t mac[6];
  uint8_t wifi[60];
  uint8_t shaResult[256];
} home_message;

static const char secret[] = "LNBFXVIvbiYLDHvt+R0CC1x/6Zqd6Wm0+YgwvoQ/ftoV";

static void getMessage() {

  ESP_ERROR_CHECK(esp_base_mac_addr_get(home_message.mac));
  for (int i = 0; i < status.ap_size && i < 10; i++) {
    memcpy(home_message.wifi + i * 6, status.ap_records[i].bssid, 6);
  }

  mbedtls_md_context_t ctx;

  mbedtls_md_init(&ctx);
  ESP_ERROR_CHECK(
      mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0));
  ESP_ERROR_CHECK(mbedtls_md_starts(&ctx));
  ESP_ERROR_CHECK(
      mbedtls_md_update(&ctx, (const unsigned char *)secret, strlen(secret)));
  ESP_ERROR_CHECK(
      mbedtls_md_update(&ctx, (const unsigned char *)&home_message, 66));
  ESP_ERROR_CHECK(mbedtls_md_finish(&ctx, home_message.shaResult));
  mbedtls_md_free(&ctx);
}

static int getSocket() {

  struct addrinfo *server_ip;

  if (home_socket >= 0)
    return home_socket;

  /** get ip of server */
  struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
  int result = getaddrinfo("symonics.com", "6454", &hints, &server_ip);

  if ((result != 0) || (server_ip == NULL)) {
    ESP_LOGI(TAG, "DNS lookup failed err=%d res=%p", result, server_ip);
    return -1;
  } else {
    assert(server_ip->ai_family == AF_INET);
    struct sockaddr_in *server = (struct sockaddr_in *)server_ip->ai_addr;
    ESP_LOGI(
        TAG, "%s == %d.%d.%d.%d", server_ip->ai_canonname,
        server->sin_addr.s_addr & 255, (server->sin_addr.s_addr >> 8) & 255,
        (server->sin_addr.s_addr >> 16) & 255, server->sin_addr.s_addr >> 24);
  }

  /** open socket */
  home_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (home_socket < 0) {
    ESP_LOGE(TAG, "socket %d %s", home_socket, strerror(home_socket));
    return -1;
  }

  struct sockaddr_in local_addr;
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(443);
  local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(home_socket, (struct sockaddr *)&local_addr, sizeof(local_addr)) <
      0) {
    ESP_LOGE(TAG, "bind %d %s", home_socket, strerror(home_socket));
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  if (connect(home_socket, (struct sockaddr *)server_ip, sizeof(*server_ip)) <
      0) {
    ESP_LOGE(TAG, "connect %d %s", home_socket, strerror(home_socket));
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  /** free address structure */
  freeaddrinfo(server_ip);

  // create message
  getMessage();

  ESP_LOGD(TAG, "socket created without errors");

  return home_socket;
}

static void send_hello() {
  int res =
      send(home_socket, &home_message, sizeof(home_message), MSG_DONTWAIT);
  if (res != sizeof(home_message)) {
    ESP_LOGW(TAG, "send error %d %d %s", res, errno, strerror(errno));
  } else {
    ESP_LOGD(TAG, "send");
  }
}
static void task(void *args) {

  TickType_t pxPreviousWakeTime;

  vTaskDelay(configTICK_RATE_HZ * 60 +
             rand() % (configTICK_RATE_HZ * 60)); // wait for 60 to 120 seconds

  /** get current time */
  pxPreviousWakeTime = xTaskGetTickCount();

  for (;;) {
    if (getSocket() >= 0) {
      send_hello();
    }
    int incrementTicks =
        xTaskGetTickCount() + configTICK_RATE_HZ * 60 * 12 +
        rand() % (configTICK_RATE_HZ * 60 * 6); // every 12 to 18 minutes
    vTaskDelayUntil(&pxPreviousWakeTime, incrementTicks);
  }
}

void home_on() {

  home_socket = -1;

  ESP_ERROR_CHECK(xTaskCreate(task, "home", 2048, NULL, tskIDLE_PRIORITY,
                              &taskHandle) == pdPASS
                      ? ESP_OK
                      : ESP_FAIL);
}

void home_off() {
  vTaskDelete(taskHandle);

  if (home_socket >= 0) {
    close(home_socket);
    home_socket = -1;
  }
}
