/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#include "webjson.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "ownled.h"
#include "playlist.h"
#include "status.h"
#include "websession.h"
#include "wifi.h"
#include "esp_mac.h"

static const char *TAG =
    strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__;

static cJSON *addScan() {
  char line[80];
  cJSON *json = cJSON_CreateArray();
  if (json == NULL) {
    return NULL;
  }

  for (int i = 0; i < status.ap_size; i++) {
    snprintf(line, sizeof(line), "%02X%02X%02X%02X%02X%02X %d %s %s",
             status.ap_records[i].bssid[0], status.ap_records[i].bssid[1],
             status.ap_records[i].bssid[2], status.ap_records[i].bssid[3],
             status.ap_records[i].bssid[4], status.ap_records[i].bssid[5],
             status.ap_records[i].rssi,
             wifi_auth_names[status.ap_records[i].authmode],
             status.ap_records[i].ssid);
    cJSON_AddItemToArray(json, cJSON_CreateString(line));
  }
  return json;
}

static cJSON *status_to_json() {

  uint8_t mac[6];
  time_t now = 0;
  char line[32];

  cJSON *json = cJSON_CreateObject();
  if (json == NULL)
    return NULL;

  cJSON_AddItemToObject(json, "id", cJSON_CreateStringReference("status"));

  /* git */
  cJSON_AddItemToObject(json, "compile_date",
                        cJSON_CreateStringReference(config_get_compile_date()));
  cJSON_AddItemToObject(json, "compile_git",
                        cJSON_CreateStringReference(config_get_version()));

  /* server */
  cJSON_AddItemToObject(json, "server_state",
                        cJSON_CreateStringReference(status.server));

  /* rtp */
  cJSON_AddItemToObject(json, "rtp_last", cJSON_CreateString(status.rtp_last));
  cJSON_AddItemToObject(json, "rtp_good", cJSON_CreateNumber(status.rtp_good));
  cJSON_AddItemToObject(json, "rtp_error",
                        cJSON_CreateNumber(status.rtp_error));
  cJSON_AddItemToObject(json, "rtp_loss", cJSON_CreateNumber(status.rtp_loss));

  /* mjpeg */
  cJSON_AddItemToObject(json, "mjpeg_good",
                        cJSON_CreateNumber(status.mjpeg_good));
  cJSON_AddItemToObject(json, "mjpeg_error",
                        cJSON_CreateNumber(status.mjpeg_error));
  cJSON_AddItemToObject(json, "mjpeg_loss",
                        cJSON_CreateNumber(status.mjpeg_loss));

  /* artnet */
  cJSON_AddItemToObject(json, "artnet_good",
                        cJSON_CreateNumber(status.artnet_good));
  cJSON_AddItemToObject(json, "artnet_error",
                        cJSON_CreateNumber(status.artnet_error));
  cJSON_AddItemToObject(json, "artnet_loss",
                        cJSON_CreateNumber(status.artnet_loss));

  /* led timing */
  cJSON_AddItemToObject(json, "led_on_time",
                        cJSON_CreateNumber(status.led_on_time));
  cJSON_AddItemToObject(json, "led_bottom_too_slow",
                        cJSON_CreateNumber(status.led_bottom_too_slow));
  cJSON_AddItemToObject(json, "led_top_too_slow",
                        cJSON_CreateNumber(status.led_top_too_slow));

  /* MAC */
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(line, sizeof(line), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  cJSON_AddItemToObject(json, "MAC", cJSON_CreateString(line));

  /* free */
  snprintf(line, sizeof(line), "%d B",
           heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
  cJSON_AddItemToObject(json, "free", cJSON_CreateString(line));

  /* tasks */
  char *pmem = malloc(2048);
  vTaskGetRunTimeStats(pmem);
  cJSON_AddItemToObject(json, "tasks", cJSON_CreateString(pmem));
  free(pmem);

  /* partition */
  cJSON_AddItemToObject(
      json, "boot_partition",
      cJSON_CreateStringReference(esp_ota_get_running_partition()->label));

  /* uptime */
  time(&now);
  snprintf(line, sizeof(line), "%lld", now);
  cJSON_AddItemToObject(json, "time", cJSON_CreateString(line));

  /* networking */
  cJSON_AddItemToObject(json, "net_sta",
                        cJSON_CreateStringReference(status.sta));
  cJSON_AddItemToObject(json, "net_ap", cJSON_CreateStringReference(status.ap));
  cJSON_AddItemToObject(json, "net_ethernet",
                        cJSON_CreateStringReference(status.ethernet));
  cJSON_AddItemToObject(json, "net_scan", addScan());

  /* filesystem */
  /** TODO **/

  /* time */
  cJSON_AddItemToObject(json, "net_ntp",
                        cJSON_CreateStringReference(status.ntp));
  cJSON_AddItemToObject(json, "net_geoip",
                        cJSON_CreateStringReference(status.geoip));
  return json;
}

static cJSON *config_to_json() {
  cJSON *json = cJSON_CreateObject();
  if (json == NULL)
    return NULL;

  cJSON_AddItemToObject(json, "id", cJSON_CreateStringReference("config"));

  /* wifi */
  cJSON_AddItemToObject(
      json, "wifi_sta_ssid",
      cJSON_CreateStringReference(config_get_wifi_sta_ssid()));
  cJSON_AddItemToObject(
      json, "wifi_sta_password",
      cJSON_CreateStringReference(config_get_wifi_sta_password()));
  cJSON_AddItemToObject(json, "wifi_ap_ssid",
                        cJSON_CreateStringReference(config_get_wifi_ap_ssid()));
  cJSON_AddItemToObject(
      json, "wifi_ap_password",
      cJSON_CreateStringReference(config_get_wifi_ap_password()));

  /* rtp */
  cJSON_AddItemToObject(json, "rtp_port",
                        cJSON_CreateNumber(config_get_udp_port()));
  cJSON_AddItemToObject(json, "registration_server",
                        cJSON_CreateStringReference(config_get_udp_server()));

  /* all leds */
  cJSON_AddItemToObject(json, "prefix_leds",
                        cJSON_CreateNumber(config_get_prefix_leds()));
  cJSON_AddItemToObject(json, "channels",
                        cJSON_CreateNumber(ownled_getChannels()));
  cJSON_AddItemToObject(json, "refresh_rate",
                        cJSON_CreateNumber(config_get_refresh_rate()));
  cJSON_AddItemToObject(json, "artnet_width",
                        cJSON_CreateNumber(config_get_artnet_width()));
  cJSON_AddItemToObject(
      json, "artnet_universe_offset",
      cJSON_CreateNumber(config_get_artnet_universe_offset()));

  /* leds */
  cJSON *array = cJSON_CreateArray();
  for (int i = 0; i < ownled_getChannels(); i++) {
    cJSON *n = cJSON_CreateObject();
    int sx, sy, ox, oy;
    enum LED_ORIENTATION ori;
    enum LED_MODE mode;
    char op[65];
    config_get_led_line(i, &mode, &sx, &sy, &ox, &oy, &ori, op);
    cJSON_AddItemToObject(n, "s", cJSON_CreateNumber((int)mode));
    cJSON_AddItemToObject(n, "sx", cJSON_CreateNumber(sx));
    cJSON_AddItemToObject(n, "sy", cJSON_CreateNumber(sy));
    cJSON_AddItemToObject(n, "ox", cJSON_CreateNumber(ox));
    cJSON_AddItemToObject(n, "oy", cJSON_CreateNumber(oy));
    cJSON_AddItemToObject(n, "r", cJSON_CreateNumber((int)ori));
    cJSON_AddItemToObject(n, "op", cJSON_CreateString(op));
    cJSON_AddItemToArray(array, n);
  }
  cJSON_AddItemToObject(json, "leds", array);

  /* playlist */
  array = cJSON_CreateArray();

  for (int i = 0; i < playlist_getEntries(); i++) {
    cJSON *n = cJSON_CreateObject();
    struct PLAYLIST_ENTRY entry;
    playlist_get(i, &entry);
    cJSON_AddItemToObject(n, "filename", cJSON_CreateString(entry.filename));
    cJSON_AddItemToObject(n, "duration", cJSON_CreateNumber(entry.duration));
    cJSON_AddItemToObject(n, "mode", cJSON_CreateNumber(entry.mode));
    cJSON_AddItemToArray(array, n);
  }
  cJSON_AddItemToObject(json, "playlist", array);

  /* color configs */
  cJSON_AddItemToObject(
      json, "contrast",
      cJSON_CreateNumber((int)(led_coloring.contrast * 100 - 100)));
  cJSON_AddItemToObject(
      json, "brightness",
      cJSON_CreateNumber((int)(led_coloring.brightness * 100)));
  cJSON_AddItemToObject(
      json, "red_contrast",
      cJSON_CreateNumber((int)(led_coloring.red_contrast * 100 - 100)));
  cJSON_AddItemToObject(
      json, "red_brightness",
      cJSON_CreateNumber((int)(led_coloring.red_brightness * 100)));
  cJSON_AddItemToObject(
      json, "green_contrast",
      cJSON_CreateNumber((int)(led_coloring.green_contrast * 100 - 100)));
  cJSON_AddItemToObject(
      json, "green_brightness",
      cJSON_CreateNumber((int)(led_coloring.green_brightness * 100)));
  cJSON_AddItemToObject(
      json, "blue_contrast",
      cJSON_CreateNumber((int)(led_coloring.blue_contrast * 100 - 100)));
  cJSON_AddItemToObject(
      json, "blue_brightness",
      cJSON_CreateNumber((int)(led_coloring.blue_brightness * 100)));
  cJSON_AddItemToObject(
      json, "saturation",
      cJSON_CreateNumber((int)(led_coloring.saturation * 100 - 100)));
  cJSON_AddItemToObject(json, "hue",
                        cJSON_CreateNumber((int)(led_coloring.hue * 100)));

  /* LED config */
  cJSON_AddItemToObject(json, "led_frequency",
                        cJSON_CreateNumber(ownled_get_pulse_frequency()));
  cJSON_AddItemToObject(json, "led_one",
                        cJSON_CreateNumber(ownled_get_pulse_one()));
  cJSON_AddItemToObject(json, "led_zero",
                        cJSON_CreateNumber(ownled_get_pulse_zero()));
  cJSON_AddItemToObject(json, "led_order",
                        cJSON_CreateNumber(ownled_getColorOrder()));

  return json;
}

/**
 * send json to given websocket listener
 */
static void send_json(httpd_handle_t hd, int sockfd, cJSON *json) {
  httpd_ws_frame_t ws_pkt = {.final = 0,
                             .fragmented = 0,
                             .len = 0,
                             .payload = (uint8_t *)cJSON_PrintUnformatted(json),
                             .type = HTTPD_WS_TYPE_TEXT};
  cJSON_Delete(json);
  ws_pkt.len = strlen((char *)ws_pkt.payload);
  httpd_ws_send_frame_async(hd, sockfd, &ws_pkt);
}

static cJSON *error_to_json(const char *error) {
  cJSON *json = cJSON_CreateObject();
  if (json == NULL)
    return NULL;

  cJSON_AddItemToObject(json, "id", cJSON_CreateStringReference("error"));
  cJSON_AddItemToObject(json, "cause", cJSON_CreateStringReference(error));
  ESP_LOGE(TAG, "error %s", error);
  return json;
}

static cJSON *ok_to_json() {
  cJSON *json = cJSON_CreateObject();
  if (json == NULL)
    return NULL;

  cJSON_AddItemToObject(json, "id", cJSON_CreateStringReference("ok"));
  return json;
}

void config_to_all() {
  ESP_LOGD(TAG, "send web config to all");
  cJSON *json = config_to_json();
  httpd_ws_frame_t ws_pkt = {.final = 0,
                             .fragmented = 0,
                             .len = 0,
                             .payload = (uint8_t *)cJSON_PrintUnformatted(json),
                             .type = HTTPD_WS_TYPE_TEXT};
  cJSON_Delete(json);
  websession_send_to_all(&ws_pkt);
}

static const char *parseConfigNetwork(cJSON *root) {
  cJSON *n;
  ESP_LOGD(TAG, "network config received");

  n = cJSON_GetObjectItem(root, "udp_port");
  if (!n || n->type != cJSON_Number || n->valueint < 0 || n->valueint > 0xffff)
    return "rtp port invalid";
  config_set_udp_port(n->valueint);

  n = cJSON_GetObjectItem(root, "udp_server");
  if (!n || n->type != cJSON_String || strlen(n->valuestring) > 31)
    return "registration server invalid";
  config_set_udp_server(n->valuestring);

  n = cJSON_GetObjectItem(root, "wifi_sta_ssid");
  if (!n || n->type != cJSON_String || strlen(n->valuestring) > 31)
    return "wifi sta ssid invalid";
  config_set_wifi_sta_ssid(n->valuestring);

  n = cJSON_GetObjectItem(root, "wifi_sta_password");
  if (!n || n->type != cJSON_String || strlen(n->valuestring) < 8 ||
      strlen(n->valuestring) > 63)
    return "wifi sta password invalid";
  config_set_wifi_sta_password(n->valuestring);

  n = cJSON_GetObjectItem(root, "wifi_ap_ssid");
  if (!n || n->type != cJSON_String || strlen(n->valuestring) > 31)
    return "wifi ap ssid invalid";
  config_set_wifi_ap_ssid(n->valuestring);

  n = cJSON_GetObjectItem(root, "wifi_ap_password");
  if (!n || n->type != cJSON_String || strlen(n->valuestring) < 8 ||
      strlen(n->valuestring) > 63)
    return "wifi ap password invalid";
  config_set_wifi_ap_password(n->valuestring);

  config_write_all();
  return NULL;
}

/*
 blue_brightness: "0"
 blue_contrast: "0"
 brightness: "-91"
 contrast: "0"
 green_brightness: "0"
 green_contrast: "0"
 hue: "0"
 id: "config.color"
 red_brightness: "0"
 red_contrast: "0"
 saturation: "0"
 */
static const char *parseConfigColor(cJSON *root) {
  cJSON *n;
  ESP_LOGD(TAG, "color config received");

  n = cJSON_GetObjectItem(root, "brightness");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.brightness = n->valueint / 100.f;

  n = cJSON_GetObjectItem(root, "contrast");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.contrast = (n->valueint + 100) / 100.f;

  n = cJSON_GetObjectItem(root, "red_brightness");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.red_brightness = n->valueint / 100.f;

  n = cJSON_GetObjectItem(root, "red_contrast");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.red_contrast = (n->valueint + 100) / 100.f;

  n = cJSON_GetObjectItem(root, "green_brightness");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.green_brightness = n->valueint / 100.f;

  n = cJSON_GetObjectItem(root, "green_contrast");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.green_contrast = (n->valueint + 100) / 100.f;

  n = cJSON_GetObjectItem(root, "blue_brightness");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.blue_brightness = n->valueint / 100.f;

  n = cJSON_GetObjectItem(root, "blue_contrast");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.blue_contrast = (n->valueint + 100) / 100.f;

  n = cJSON_GetObjectItem(root, "hue");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.hue = n->valueint / 100.f;

  n = cJSON_GetObjectItem(root, "saturation");
  if (!n || n->type != cJSON_Number || n->valueint < -100 || n->valueint > 100)
    return "invalid";
  led_coloring.saturation = (n->valueint + 100) / 100.f;

  return NULL;
}

static const char *parseConfigPlaylist(cJSON *root) {
  cJSON *n;

  root = cJSON_GetObjectItem(root, "playlist");
  if (!root || root->type != cJSON_Array ||
      cJSON_GetArraySize(root) != playlist_getEntries())
    return "invalid";

  for (int i = 0; i < playlist_getEntries(); i++) {
    struct PLAYLIST_ENTRY entry;
    cJSON *subitem = cJSON_GetArrayItem(root, i);
    if (subitem == NULL)
      return "invalid";
    n = cJSON_GetObjectItem(subitem, "filename");
    if (!n || n->type != cJSON_String)
      return "invalid";
    strncpy(entry.filename, n->valuestring, strlen(entry.filename));
    n = cJSON_GetObjectItem(subitem, "duration");
    if (!n || n->type != cJSON_Number || n->valueint < 0 ||
        n->valueint > 1000000)
      return "invalid duration";
    entry.duration = n->valueint;
    n = cJSON_GetObjectItem(subitem, "mode");
    if (!n || n->type != cJSON_Number || n->valueint < 0 ||
        n->valueint >= PLAYLIST_LAST)
      return "invalid mode";
    entry.mode = n->valueint;
    if (playlist_set(i, &entry) != ESP_OK)
      return "invalid settings";
  }
  return NULL;
}

static const char *parseConfigLed(cJSON *root) {
  ESP_LOGD(TAG, "led config received");

  cJSON *channels = cJSON_GetObjectItem(root, "channels");
  if (!channels || channels->type != cJSON_Number)
    return "no channels";

  cJSON *prefix_leds = cJSON_GetObjectItem(root, "prefix_leds");
  if (!prefix_leds || prefix_leds->type != cJSON_Number)
    return "no prefix_leds";

  cJSON *refresh_rate = cJSON_GetObjectItem(root, "refresh_rate");
  if (!refresh_rate || refresh_rate->type != cJSON_Number)
    return "no refresh_rate";

  cJSON *artnet_width = cJSON_GetObjectItem(root, "artnet_width");
  if (!artnet_width || artnet_width->type != cJSON_Number)
    return "no artnet_width";

  cJSON *artnet_universe_offset =
      cJSON_GetObjectItem(root, "artnet_universe_offset");
  if (!artnet_universe_offset || artnet_universe_offset->type != cJSON_Number)
    return "no artnet_universe_offset";

  cJSON *leds = cJSON_GetObjectItem(root, "leds");
  if (!leds || leds->type != cJSON_Array)
    return "no leds array";

  if (channels->valueint != ownled_getChannels()) {
    return "invalid number of channels";
  }
  config_set_prefix_leds(prefix_leds->valueint);
  config_set_refresh_rate(refresh_rate->valueint);
  config_set_artnet_width(artnet_width->valueint);
  config_set_artnet_universe_offset(artnet_universe_offset->valueint);

  int size = cJSON_GetArraySize(leds);
  if (size > ownled_getChannels())
    size = ownled_getChannels();
  for (int i = 0; i < size; i++) {
    cJSON *n = cJSON_GetArrayItem(leds, i);
    if (!n || n->type != cJSON_Object)
      return "no leds item";

    cJSON *s = cJSON_GetObjectItem(n, "s");
    if (!s || s->type != cJSON_Number || s->valueint < 0 ||
        s->valueint >= LED_MODE_MAXVALUE)
      return "no leds s";

    cJSON *sx = cJSON_GetObjectItem(n, "sx");
    if (!sx || sx->type != cJSON_Number || sx->valueint < 0)
      return "no leds sx";

    cJSON *sy = cJSON_GetObjectItem(n, "sy");
    if (!sy || sy->type != cJSON_Number || sy->valueint < 0)
      return "no leds sy";

    cJSON *ox = cJSON_GetObjectItem(n, "ox");
    if (!ox || ox->type != cJSON_Number || ox->valueint < 0)
      return "no leds ox";

    cJSON *oy = cJSON_GetObjectItem(n, "oy");
    if (!oy || oy->type != cJSON_Number || oy->valueint < 0)
      return "no leds oy";

    cJSON *r = cJSON_GetObjectItem(n, "r");
    if (!r || r->type != cJSON_Number || r->valueint < 0 ||
        r->valueint >= LED_ORI_MAXVALUE)
      return "no leds r";

    cJSON *op = cJSON_GetObjectItem(n, "op");
    if (!op || op->type != cJSON_String || strlen(op->valuestring) > 64)
      return "no led options";

    config_set_led_line(i, s->valueint, sx->valueint, sy->valueint,
                        ox->valueint, oy->valueint, r->valueint,
                        op->valuestring);
  }

  cJSON *frequency = cJSON_GetObjectItem(root, "led_frequency");
  if (!frequency || frequency->type != cJSON_Number)
    return "no frequency";
  cJSON *one = cJSON_GetObjectItem(root, "led_one");
  if (!one || one->type != cJSON_Number)
    return "no one pulse";
  cJSON *zero = cJSON_GetObjectItem(root, "led_zero");
  if (!zero || zero->type != cJSON_Number)
    return "no zero pulse length";
  if (ownled_set_pulses(frequency->valuedouble, one->valuedouble,
                        zero->valuedouble) != ESP_OK)
    return "wrong frequency, one, or zero value";

  cJSON *order = cJSON_GetObjectItem(root, "led_order");
  if (!zero || zero->type != cJSON_Number)
    return "no led order";
  if (ownled_setColorOrder(order->valueint) != ESP_OK)
    return "wrong led order";

  config_write_all();
  config_update_channels();
  return NULL;
}

int webjson_parseMessage(httpd_handle_t hd, int sockfd, const uint8_t *payload,
                         size_t len) {
  cJSON *root = NULL;
  cJSON *id = NULL;

  const uint8_t *end = payload + len;
  root = cJSON_ParseWithOpts((const char *)payload, (const char **)&end, false);
  if (!root || root->type != cJSON_Object || end > payload + len)
    goto error;

  id = cJSON_GetObjectItem(root, "id");
  if (!id || id->type != cJSON_String)
    goto error;

  if (!strcmp(id->valuestring, "hello")) {
    ESP_LOGI(TAG, "hello system");
    send_json(hd, sockfd, config_to_json());
    send_json(hd, sockfd, status_to_json());
  } else if (!strcmp(id->valuestring, "reboot")) {
    ESP_LOGI(TAG, "reboot system");
    esp_restart();
  } else if (!strcmp(id->valuestring, "defaults")) {
    ESP_LOGI(TAG, "reset to defaults");
    config_reset_to_defaults();
    esp_restart();
  } else if (!strcmp(id->valuestring, "config.network")) {
    const char *result = parseConfigNetwork(root);
    ESP_LOGD(TAG, "config.network set result %s", result ? result : "OK");
    if (result) {
      send_json(hd, sockfd, error_to_json(result));
    }
    esp_restart();
  } else if (!strcmp(id->valuestring, "config.led")) {
    const char *result = parseConfigLed(root);
    ESP_LOGD(TAG, "config.led set result %s", result ? result : "OK");
    if (result) {
      send_json(hd, sockfd, error_to_json(result));
    } else
      send_json(hd, sockfd, config_to_json());
  } else if (!strcmp(id->valuestring, "config.color")) {
    const char *result = parseConfigColor(root);
    ESP_LOGD(TAG, "config.color set result %s", result ? result : "OK");
    if (result) {
      send_json(hd, sockfd, error_to_json(result));
    } else
      send_json(hd, sockfd, ok_to_json());
  } else if (!strcmp(id->valuestring, "config.playlist")) {
    const char *result = parseConfigPlaylist(root);
    ESP_LOGD(TAG, "config.playlist set result %s", result ? result : "OK");
    if (result) {
      send_json(hd, sockfd, error_to_json(result));
    } else
      send_json(hd, sockfd, ok_to_json());
  } else if (!strcmp(id->valuestring, "config.color.discard")) {
    ESP_LOGD(TAG, "config.color.discard");
    config_coloring_reload();
    config_to_all();
  } else if (!strcmp(id->valuestring, "config.color.defaults")) {
    ESP_LOGD(TAG, "config.color.defaults");
    config_coloring_defaults();
    config_coloring_write();
    config_to_all();
  } else if (!strcmp(id->valuestring, "config.color.write")) {
    ESP_LOGD(TAG, "config.color.write");
    config_coloring_write();
    config_to_all();
  }
  cJSON_Delete(root);
  return ESP_OK;

error:
  cJSON_Delete(root);
  ESP_LOGE(TAG, "WS Error");
  return ESP_FAIL;
}