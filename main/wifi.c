/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#include "wifi.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <mdns.h>
#include <string.h>
#include "config.h"
#include "ethernet.h"
#include "status.h"
#include "udp.h"
#include "web.h"

static const char *TAG = strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__;

#define EXAMPLE_MAX_STA_CONN 3

static void wifi_scan();
static void wifi_scan_done(esp_netif_t *netif_ap);

static int ap_connected;
static int client_connecting = 0;
static esp_netif_t *netif_ap = NULL;

static void updateApConnected()
{
  char line[64];
  snprintf(line, sizeof(line), "connected: %d", ap_connected);
  status_ap(line);
}

/** Event handler for Ethernet events */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  char string[64];
  system_event_info_t *info = (system_event_info_t *)event_data;

  ESP_LOGD(TAG, "NETWORK WIFI_EVENT %s %d", event_base, event_id);

  switch (event_id)
  {

  case SYSTEM_EVENT_STA_START:
    status_sta("start");
    esp_wifi_connect();
    break;

  case SYSTEM_EVENT_STA_CONNECTED:
    status_sta("connected");
    break;

  case SYSTEM_EVENT_STA_GOT_IP:
    esp_ip4addr_ntoa(&info->got_ip.ip_info.ip, string, sizeof(string));
    status_sta(string);
    break;

  case SYSTEM_EVENT_STA_DISCONNECTED:
    status_sta("disconnected");
    ESP_LOGD(TAG, "STA disconnect %d", info->disconnected.reason);
    client_connecting++;
    if (client_connecting < 5)
      esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_STOP:
    status_sta("stopped");
    break;

  case SYSTEM_EVENT_AP_START:
    status_ap("start");
    break;

  case SYSTEM_EVENT_AP_STACONNECTED:
    ESP_LOGD(TAG, "station:" MACSTR " join, AID=%d",
             MAC2STR(info->sta_connected.mac),
             info->sta_connected.aid);
    ap_connected++;
    updateApConnected();
    break;
  case SYSTEM_EVENT_AP_STAIPASSIGNED:
    ESP_LOGI(TAG, "IP assigned to client");
    break;

  case SYSTEM_EVENT_AP_STADISCONNECTED:
    ESP_LOGD(TAG, "station:" MACSTR "leave, AID=%d",
             MAC2STR(info->sta_disconnected.mac),
             info->sta_disconnected.aid);
    --ap_connected;
    updateApConnected();
    break;

  case SYSTEM_EVENT_AP_STOP:
    status_ap("off");
    break;

  case SYSTEM_EVENT_SCAN_DONE:
    ESP_LOGD(TAG, "SCAN %d %d %04X", info->scan_done.number,
             info->scan_done.scan_id,
             info->scan_done.status);
    if (info->scan_done.status == 1) /** try again */
      wifi_scan();
    else
    {
      wifi_scan_done(netif_ap);
      web_on();
    }
    break;

  case SYSTEM_EVENT_STA_WPS_ER_PIN:
  case SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP:
    // WPS is not supported, yes
    break;

  default:
    ESP_LOGE(TAG, "SYSTEM_EVENT unknown %d", event_id);
    break;
  }

  //  mdns_handle_system_event(ctx, event);
}

static void wifi_init_softap(esp_netif_t *netif_ap)
{
#if 0
   esp_netif_ip_info_t ip_info;
   esp_netif_set_ip4_addr(&ip_info.ip, 192, 168, 14, 1);
   esp_netif_set_ip4_addr(&ip_info.netmask, 255, 255, 255, 0);
   esp_netif_set_ip4_addr(&ip_info.gw, 255, 255, 14, 1);
   esp_netif_set_ip_info(netif_ap, &ip_info);
#endif

  wifi_config_t wifi_config = {
      .ap = {.ssid_len = strlen(config_get_wifi_ap_ssid()),
             .max_connection = 3,
             .authmode = WIFI_AUTH_WPA_WPA2_PSK},
  };
  strncpy((char *)wifi_config.ap.ssid, config_get_wifi_ap_ssid(),
          sizeof(wifi_config.ap.ssid) - 1);
  strncpy((char *)wifi_config.ap.password, config_get_wifi_ap_password(),
          sizeof(wifi_config.ap.password) - 1);
  if (strlen(config_get_wifi_ap_password()) == 0)
  {
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

  ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s", config_get_wifi_ap_ssid(), config_get_wifi_ap_password());
}

static void wifi_init_sta()
{
  wifi_config_t wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config));
  strncpy((char *)wifi_config.sta.ssid, config_get_wifi_sta_ssid(),
          sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char *)wifi_config.sta.password, config_get_wifi_sta_password(),
          sizeof(wifi_config.sta.password) - 1);

  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

  ESP_LOGI(TAG, "wifi_init_sta finished.");
  //	ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
  //			config_get_wifi_sta_ssid(),
  //config_get_wifi_sta_password());
}

const char *wifi_auth_names[6] = {"open", "WEP", "WPA",
                                  "WPA2", "WPA+2", "WPA2E"};

static void wifi_scan_done(esp_netif_t *netif_ap)
{
  uint16_t number;
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&number));
  ESP_LOGI(TAG, "AP NUM %04X", number);

  wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * number);
  if (number > 0)
  {
    ESP_ERROR_CHECK(ap_records ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_records));

    for (int i = 0; i < number; i++)
    {
      ESP_LOGI(TAG, "AP %d: %02X%02X%02X%02X%02X%02X %d %s %s	", // %d %s %s %s,",
               i, ap_records[i].bssid[0], ap_records[i].bssid[1],
               ap_records[i].bssid[2], ap_records[i].bssid[3],
               ap_records[i].bssid[4], ap_records[i].bssid[5], ap_records[i].rssi,
               wifi_auth_names[ap_records[i].authmode], ap_records[i].ssid);
    }
  }

  status_scan(number, ap_records);

  esp_wifi_stop();
  ESP_LOGI(TAG, "ESP_WIFI_MODE_APSTA");
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  wifi_init_sta();
  wifi_init_softap(netif_ap);
  ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_scan(system_event_sta_scan_done_t *done)
{
  wifi_scan_config_t config = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 100,
      .show_hidden = true,
      .scan_type = WIFI_SCAN_TYPE_ACTIVE,
      .scan_time.active.min = 100,
      .scan_time.active.max = 500,
  };

  ESP_ERROR_CHECK(esp_wifi_scan_start(&config, false));
}

void wifi_on()
{
  ap_connected = 0;

  esp_netif_create_default_wifi_ap();

  esp_netif_create_default_wifi_sta();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /* store config in ram */
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  wifi_scan(NULL);
}

void wifi_off() { esp_wifi_deinit(); }
