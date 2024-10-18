/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * config.c
 *
 *  Created on: 29.07.2018
 *      Author: hoene
 */

#include "config.h"

#include <string.h>

#include "bonjour.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_mac.h"

#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "nvs_flash.h"
#include "ownled.h"
#include "playlist.h"

static const char *TAG = "#config";

static char wifi_sta_ssid[32];
static char wifi_sta_password[64];
static char wifi_ap_ssid[32];
static char wifi_ap_password[64];
static uint16_t udp_port;
static char udp_server[32];
static struct LED_CONFIG led_config;
static char partition_name[17] = NVS_DEFAULT_PART_NAME;

struct LED_CONFIG_CHANNEL_V1_1 {
  enum LED_MODE mode;
  enum LED_ORIENTATION orientation;
  int16_t sx;
  int16_t sy;
  int16_t ox;
  int16_t oy;
  int16_t black[3];
};

struct LED_CONFIG_V1_1 {
  int16_t refresh_rate;
  uint8_t channels;
  uint8_t prefix_leds;
  struct LED_CONFIG_CHANNEL_V1_1 channel[8];
  uint16_t artnet_width;
  uint16_t artnet_universe_offset;
};

struct LED_CONFIG_CHANNEL_V1_0 {
  enum LED_MODE mode;
  enum LED_ORIENTATION orientation;
  int sx;
  int sy;
  int ox;
  int oy;
};

struct LED_CONFIG_V1_0 {
  uint8_t refresh_rate;
  uint8_t channels;
  uint8_t prefix_leds;
  struct LED_CONFIG_CHANNEL_V1_0 channel[8];
  uint16_t artnet_width;
  uint16_t artnet_universe_offset;
};

#if 0
static const char secret[] = "LNBFXVIvbiYLDHvt+R0CC1x/6Zqd6Wm0+YgwvoQ/ftoV";

static void getPassword(const char *in, char output[32]) {
	mbedtls_md_context_t ctx;
	uint8_t shaResult[MBEDTLS_MD_MAX_SIZE];

	mbedtls_md_init(&ctx);
	ESP_ERROR_CHECK(
			mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_MD5),
					0));
	ESP_ERROR_CHECK(mbedtls_md_starts(&ctx));
	ESP_ERROR_CHECK(
			mbedtls_md_update(&ctx, (const unsigned char* ) secret,
					strlen(secret)));
	ESP_ERROR_CHECK(
			mbedtls_md_update(&ctx, (const unsigned char* ) in, strlen(in)));
	ESP_ERROR_CHECK(mbedtls_md_finish(&ctx, shaResult));
	mbedtls_md_free(&ctx);

	size_t size;
	ESP_ERROR_CHECK(
			mbedtls_base64_encode((unsigned char* ) output, 32, &size,
					shaResult, 128 / 8));
	output[size - 2] = 0;
}
#endif

static uint8_t defaultModes[8] = {
    LED_MODE_WHITE, LED_MODE_GRAY, LED_MODE_RED,  LED_MODE_YELLOW,
    LED_MODE_GREEN, LED_MODE_CYAN, LED_MODE_BLUE, LED_MODE_MAGENTA};

// set values to default
static void setDefaults() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  snprintf(wifi_ap_ssid, sizeof(wifi_ap_ssid),
           "Symonics%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
  strcpy(wifi_ap_password, "controller");
  //	getPassword(wifi_ap_ssid, wifi_ap_password);
  ESP_LOGI(TAG, "AP:%s:%s", wifi_ap_ssid, wifi_ap_password);

  strcpy(wifi_sta_ssid, "SymonicsController");
  strcpy(wifi_sta_password, "controller");
  //	strncpy(wifi_sta_ssid, "Internet", sizeof(wifi_sta_ssid));
  //	strncpy(wifi_sta_password, wifi_ap_password, sizeof(wifi_sta_password));

  strncpy(udp_server, "www.symonics.com", sizeof(udp_server));
  udp_port = 6454;

  led_config.channels = 2;
  led_config.prefix_leds = 0;
  led_config.refresh_rate = 10;
  for (int i = 0; i < led_get_max_lines(); i++) {
    led_config.channel[i].mode = defaultModes[i];
    led_config.channel[i].sx = led_config.channel[i].sy = 8;
    led_config.channel[i].ox = led_config.channel[i].oy = 0;
    led_config.channel[i].orientation = LED_ORI0_ZIGZAG;
    led_config.channel[i].black[0] = led_config.channel[i].black[1] =
        led_config.channel[i].black[2] = -1;
  }

  config_coloring_defaults();

  led_config.artnet_universe_offset = 0;
  led_config.artnet_width = 512 / 3;

  ownled_set_default();

  playlist_defaultConfig();
}

/*
 * test the NVS storage
 */

static void testConfig() {

  nvs_handle my_handle;
  esp_err_t res;
  uint16_t value1, value2;

  //> tries to open the "config" storage
  if ((res = nvs_open_from_partition(partition_name, "config", NVS_READONLY,
                                     &my_handle)) != ESP_OK) {
    ESP_LOGI(TAG, "cannot open config %d", res);
    //> ok, no config found. Now write it first
    ESP_ERROR_CHECK(nvs_open_from_partition(partition_name, "config",
                                            NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u16(my_handle, "test123", 0x1234));
    nvs_close(my_handle);
    ESP_ERROR_CHECK(nvs_open_from_partition(partition_name, "config",
                                            NVS_READONLY, &my_handle));
  }

  // read a value
  ESP_ERROR_CHECK(nvs_get_u16(my_handle, "test123", &value1));
  ESP_LOGI(TAG, "read %d", value1);
  nvs_close(my_handle);

  // now write a change value
  ESP_ERROR_CHECK(nvs_open_from_partition(partition_name, "config",
                                          NVS_READWRITE, &my_handle));
  value1++;
  ESP_ERROR_CHECK(nvs_set_u16(my_handle, "test123", value1));
  ESP_ERROR_CHECK(nvs_commit(my_handle));
  nvs_close(my_handle);

  // read it again and compare it with the changed value
  ESP_ERROR_CHECK(nvs_open_from_partition(partition_name, "config",
                                          NVS_READONLY, &my_handle));
  ESP_ERROR_CHECK(nvs_get_u16(my_handle, "test123", &value2));
  nvs_close(my_handle);
  if (value1 != value2) {
    ESP_LOGE(TAG, "config storage broken! %d <> %d", value1, value2);
  }
}

// reads configuration (if any)
static void readConfig() {
  nvs_handle my_handle;
  int res;
  if ((res = nvs_open_from_partition(partition_name, "config", NVS_READONLY,
                                     &my_handle))) {
    ESP_LOGI(TAG, "cannot open config %d", res);
    return;
  }

  size_t size = sizeof(wifi_sta_ssid);
  nvs_get_blob(my_handle, "wifi_sta_ssid", wifi_sta_ssid, &size);
  size = sizeof(wifi_ap_ssid);
  nvs_get_blob(my_handle, "wifi_ap_ssid", wifi_ap_ssid, &size);
  size = sizeof(wifi_sta_password);
  nvs_get_blob(my_handle, "wifi_sta_pw", wifi_sta_password, &size);
  size = sizeof(wifi_ap_password);
  nvs_get_blob(my_handle, "wifi_ap_pw", wifi_ap_password, &size);

  size = sizeof(udp_server);
  nvs_get_blob(my_handle, "udp_server", udp_server, &size);

  nvs_get_u16(my_handle, "udp_port", &udp_port);

  size = sizeof(struct LED_CONFIG_V1_1); // is larger than sizeof(struct
                                         // LED_CONFIG_V1_0));
  struct LED_CONFIG_V1_1 tmp;
  if (nvs_get_blob(my_handle, "leds", &tmp, &size) == ESP_OK) {
    if (size == sizeof(struct LED_CONFIG_V1_1)) {
      ESP_LOGI(TAG, "led bin config size %d v1.1", size);
      for (int i = 0; i < 8; i++) {
        led_config.channel[i].mode = tmp.channel[i].mode;
        led_config.channel[i].orientation = tmp.channel[i].orientation;
        led_config.channel[i].sx = tmp.channel[i].sx;
        led_config.channel[i].sy = tmp.channel[i].sy;
        led_config.channel[i].ox = tmp.channel[i].ox;
        led_config.channel[i].oy = tmp.channel[i].oy;
        led_config.channel[i].black[0] = tmp.channel[i].black[0];
        led_config.channel[i].black[1] = tmp.channel[i].black[1];
        led_config.channel[i].black[2] = tmp.channel[i].black[2];
      }
    } else if (size == sizeof(struct LED_CONFIG_V1_0)) {
      ESP_LOGI(TAG, "led bin config size %d v1.0", size);
      struct LED_CONFIG_V1_0 *ptmp = (struct LED_CONFIG_V1_0 *)&tmp;
      for (int i = 0; i < 8; i++) {
        led_config.channel[i].mode = ptmp->channel[i].mode;
        led_config.channel[i].orientation = ptmp->channel[i].orientation;
        led_config.channel[i].sx = ptmp->channel[i].sx;
        led_config.channel[i].sy = ptmp->channel[i].sy;
        led_config.channel[i].ox = ptmp->channel[i].ox;
        led_config.channel[i].oy = ptmp->channel[i].oy;
      }
    }
  }

  for (int i = 0; i < 8; i++) {
    char varname[32];

    uint8_t mode = led_config.channel[i].mode;
    sprintf(varname, "leds%dmode", i);
    nvs_get_u8(my_handle, varname, &mode);
    led_config.channel[i].mode = mode;

    uint8_t orientation = led_config.channel[i].orientation;
    sprintf(varname, "leds%dori", i);
    nvs_get_u8(my_handle, varname, &orientation);
    led_config.channel[i].orientation = orientation;

    sprintf(varname, "leds%dsx", i);
    nvs_get_i16(my_handle, varname, &led_config.channel[i].sx);
    sprintf(varname, "leds%dsy", i);
    nvs_get_i16(my_handle, varname, &led_config.channel[i].sy);
    sprintf(varname, "leds%dox", i);
    nvs_get_i16(my_handle, varname, &led_config.channel[i].ox);
    sprintf(varname, "leds%doy", i);
    nvs_get_i16(my_handle, varname, &led_config.channel[i].oy);
    sprintf(varname, "leds%db0", i);
    nvs_get_i16(my_handle, varname, &led_config.channel[i].black[0]);
    sprintf(varname, "leds%db1", i);
    nvs_get_i16(my_handle, varname, &led_config.channel[i].black[1]);
    sprintf(varname, "leds%db2", i);
    nvs_get_i16(my_handle, varname, &led_config.channel[i].black[2]);
  }

  size = sizeof(led_coloring);
  nvs_get_blob(my_handle, "coloring", &led_coloring, &size);

  nvs_get_u8(my_handle, "channels", &led_config.channels);
  nvs_get_u8(my_handle, "prefix_leds", &led_config.prefix_leds);
  nvs_get_i16(my_handle, "refresh_rate", &led_config.refresh_rate);

  nvs_get_u16(my_handle, "artnet_width", &led_config.artnet_width);
  nvs_get_u16(my_handle, "artnet_u_offset", &led_config.artnet_universe_offset);

  uint32_t frequency;
  uint8_t one, zero, order;
  nvs_get_u32(my_handle, "led_frequency", &frequency);
  nvs_get_u8(my_handle, "led_one", &one);
  nvs_get_u8(my_handle, "led_zero", &zero);
  nvs_get_u8(my_handle, "led_order", &order);
  ownled_set_pulses(frequency, one, zero);
  ownled_setColorOrder(order);

  playlist_readConfig(my_handle);

  nvs_close(my_handle);
}

// write configuration
void config_write_all() {
  nvs_handle my_handle;
  ESP_ERROR_CHECK(nvs_open_from_partition(partition_name, "config",
                                          NVS_READWRITE, &my_handle));

  ESP_ERROR_CHECK(nvs_set_blob(my_handle, "wifi_sta_ssid", wifi_sta_ssid,
                               sizeof(wifi_sta_ssid)));
  ESP_ERROR_CHECK(nvs_set_blob(my_handle, "wifi_ap_ssid", wifi_ap_ssid,
                               sizeof(wifi_ap_ssid)));
  ESP_ERROR_CHECK(nvs_set_blob(my_handle, "wifi_sta_pw", wifi_sta_password,
                               sizeof(wifi_sta_password)));
  ESP_ERROR_CHECK(nvs_set_blob(my_handle, "wifi_ap_pw", wifi_ap_password,
                               sizeof(wifi_ap_password)));
  ESP_ERROR_CHECK(
      nvs_set_blob(my_handle, "udp_server", udp_server, sizeof(udp_server)));
  ESP_ERROR_CHECK(nvs_set_u16(my_handle, "udp_port", udp_port));

  nvs_erase_key(my_handle, "leds"); // remove config from version 1.0

  for (int i = 0; i < 8; i++) {
    char varname[32];

    uint8_t mode = led_config.channel[i].mode;
    sprintf(varname, "leds%dmode", i);
    nvs_set_u8(my_handle, varname, mode);

    uint8_t orientation = led_config.channel[i].orientation;
    sprintf(varname, "leds%dori", i);
    nvs_set_u8(my_handle, varname, orientation);

    sprintf(varname, "leds%dsx", i);
    nvs_set_i16(my_handle, varname, led_config.channel[i].sx);
    sprintf(varname, "leds%dsy", i);
    nvs_set_i16(my_handle, varname, led_config.channel[i].sy);
    sprintf(varname, "leds%dox", i);
    nvs_set_i16(my_handle, varname, led_config.channel[i].ox);
    sprintf(varname, "leds%doy", i);
    nvs_set_i16(my_handle, varname, led_config.channel[i].oy);
    sprintf(varname, "leds%db0", i);
    nvs_set_i16(my_handle, varname, led_config.channel[i].black[0]);
    sprintf(varname, "leds%db1", i);
    nvs_set_i16(my_handle, varname, led_config.channel[i].black[1]);
    sprintf(varname, "leds%db2", i);
    nvs_set_i16(my_handle, varname, led_config.channel[i].black[2]);
  }

  ESP_ERROR_CHECK(nvs_set_u8(my_handle, "channels", led_config.channels));
  ESP_ERROR_CHECK(nvs_set_u8(my_handle, "prefix_leds", led_config.prefix_leds));
  ESP_ERROR_CHECK(
      nvs_set_u8(my_handle, "refresh_rate", led_config.refresh_rate));

  ESP_ERROR_CHECK(
      nvs_set_u16(my_handle, "artnet_width", led_config.artnet_width));
  ESP_ERROR_CHECK(nvs_set_u16(my_handle, "artnet_u_offset",
                              led_config.artnet_universe_offset));

  ESP_ERROR_CHECK(
      nvs_set_u32(my_handle, "led_frequency", ownled_get_pulse_frequency()));
  ESP_ERROR_CHECK(nvs_set_u8(my_handle, "led_one", ownled_get_pulse_one()));
  ESP_ERROR_CHECK(nvs_set_u8(my_handle, "led_zero", ownled_get_pulse_zero()));
  ESP_ERROR_CHECK(nvs_set_u8(my_handle, "led_order", ownled_getColorOrder()));

  playlist_writeConfig(my_handle);

  nvs_close(my_handle);
}

void config_update_channels() { led_set_config(&led_config); }

void config_init() {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init_partition(partition_name);
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_LOGE(TAG, "ESP_ERR_NVS_NO_FREE_PAGES -> erase NVS");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init_partition(partition_name);
  }
  ESP_ERROR_CHECK(ret);

  testConfig();
  setDefaults();
  readConfig();
}

const char *config_get_compile_date() {
  static char date[67] = "";
  if (date[0] == 0) {
    const esp_app_desc_t *description = esp_ota_get_app_description();
    snprintf(date, sizeof(date), "%s %s %s", description->project_name,
             description->date, description->time);
  }
  return date;
}

const char *config_get_version() {
  const esp_app_desc_t *description = esp_ota_get_app_description();
  return description->version;
}

//***********************************************************************************************
//                                B A C K T O F A C T O R Y *
//***********************************************************************************************
// Return to factory version. * This will set the otadata to boot from the
// factory image, ignoring previous OTA updates.     *
//***********************************************************************************************
static void back2factory() {
  esp_partition_iterator_t pi;    // Iterator for find
  const esp_partition_t *factory; // Factory partition

  pi =
      esp_partition_find(ESP_PARTITION_TYPE_APP, // Get partition iterator for
                         ESP_PARTITION_SUBTYPE_APP_FACTORY, // factory partition
                         "factory");

  // Check result
  if (pi == NULL) {
    ESP_LOGE(TAG, "Failed to find factory partition");
    ESP_ERROR_CHECK(ESP_ERR_INVALID_ARG);
  }

  factory = esp_partition_get(pi);    // Get partition struct
  esp_partition_iterator_release(pi); // Release the iterator
  ESP_ERROR_CHECK(
      esp_ota_set_boot_partition(factory)); // Set partition for boot
}

void config_reset_to_defaults() {
  ESP_ERROR_CHECK(nvs_flash_erase());
  setDefaults();
  back2factory();
}

const char *config_get_wifi_sta_ssid() {
  // at most 32
  return wifi_sta_ssid;
}

const char *config_get_wifi_sta_password() {
  // at most 64
  return wifi_sta_password;
}

const char *config_get_wifi_ap_ssid() {
  // at most 32
  return wifi_ap_ssid;
}

const char *config_get_wifi_ap_password() {
  // at most 64
  return wifi_ap_password;
}

int config_get_udp_port() { return udp_port; }

const char *config_get_udp_server() { return udp_server; }

static void scpy(char *dst, const char *src, int len) {
  strncpy(dst, src, len - 1);
  dst[len - 1] = 0;
}

void config_set_wifi_sta_ssid(const char *ssid) {
  scpy(wifi_sta_ssid, ssid, sizeof(wifi_sta_ssid));
}
void config_set_wifi_sta_password(const char *password) {
  scpy(wifi_sta_password, password, sizeof(wifi_sta_password));
}

void config_set_wifi_ap_ssid(const char *ssid) {
  scpy(wifi_ap_ssid, ssid, sizeof(wifi_ap_ssid));
}

void config_set_wifi_ap_password(const char *password) {
  scpy(wifi_ap_password, password, sizeof(wifi_ap_password));
}

void config_set_udp_port(uint16_t port) {
  if (port == udp_port)
    return;
  udp_port = port;
  bonjour_off();
  bonjour_on();
}

void config_set_udp_server(const char *server) {
  scpy(udp_server, server, sizeof(udp_server));
}

static int compare_short(const void *p1, const void *p2) {
  int a = *(const short *)p1;
  int b = *(const short *)p2;
  return a - b;
}

void config_set_led_line(int line, enum LED_MODE mode, int size_x, int size_y,
                         int offset_x, int offset_y,
                         enum LED_ORIENTATION orientation,
                         const char *options) {
  if (line < 0 || line >= led_get_max_lines())
    return;

  led_config.channel[line].mode = mode;
  led_config.channel[line].sx = size_x;
  led_config.channel[line].sy = size_y;
  led_config.channel[line].ox = offset_x;
  led_config.channel[line].oy = offset_y;
  led_config.channel[line].orientation = orientation;
  led_config.channel[line].black[0] = -1;
  led_config.channel[line].black[1] = -1;
  led_config.channel[line].black[2] = -1;

  if (0 == strncmp(options, "black=", 6)) {
    int16_t *black = led_config.channel[line].black;
    int res =
        sscanf(options + 6, "%hd,%hd,%hd", &black[0], &black[1], &black[2]);
    ESP_LOGI(TAG, "option res %d", res);

    for (int i = 0; i < res; i++) {
      if (black[i] < 0 || black[i] >= size_x * size_y) {
        black[i] = black[res - 1];
        res--;
        i--;
      }
    }
    qsort(black, res, sizeof(int16_t), compare_short);
    for (int i = res; i < 3; i++)
      black[i] = -1;

    ESP_LOGI(TAG, "options %d %d %d %d", res, black[0], black[1], black[2]);
  }
}

void config_get_led_line(int line, enum LED_MODE *mode, int *size_x,
                         int *size_y, int *offset_x, int *offset_y,
                         enum LED_ORIENTATION *orientation, char options[65]) {
  if (line < 0 || line >= led_get_max_lines())
    return;

  *size_x = led_config.channel[line].sx;
  *size_y = led_config.channel[line].sy;
  *offset_x = led_config.channel[line].ox;
  *offset_y = led_config.channel[line].oy;
  *orientation = led_config.channel[line].orientation;
  *mode = led_config.channel[line].mode;

  if (led_config.channel[line].black[0] == -1)
    options[0] = 0;
  else if (led_config.channel[line].black[1] == -1)
    sprintf(options, "black=%d", led_config.channel[line].black[0]);
  else if (led_config.channel[line].black[2] == -1)
    sprintf(options, "black=%d,%d", led_config.channel[line].black[0],
            led_config.channel[line].black[1]);
  else
    sprintf(options, "black=%d,%d,%d", led_config.channel[line].black[0],
            led_config.channel[line].black[1],
            led_config.channel[line].black[2]);
}

/**
 * handle the coloring information
 */

void config_coloring_reload() {
  nvs_handle my_handle;
  if (nvs_open_from_partition(partition_name, "config", NVS_READONLY,
                              &my_handle))
    return;

  size_t size = sizeof(led_coloring);
  nvs_get_blob(my_handle, "coloring", &led_coloring, &size);

  nvs_close(my_handle);
}

static struct LED_COLORING defaults_led_coloring = {1, 0, 1, 0, 1,
                                                    0, 1, 0, 1, 0};

void config_coloring_defaults() { led_coloring = defaults_led_coloring; }

void config_coloring_write() {
  nvs_handle my_handle;
  ESP_ERROR_CHECK(nvs_open_from_partition(partition_name, "config",
                                          NVS_READWRITE, &my_handle));

  ESP_ERROR_CHECK(
      nvs_set_blob(my_handle, "coloring", &led_coloring, sizeof(led_coloring)));

  nvs_close(my_handle);
}

uint8_t config_get_channels() { return led_config.channels; }

void config_set_channels(uint8_t value) {
  if (value < 1 || value > led_get_max_lines())
    return;
  led_config.channels = value;
}

uint8_t config_get_prefix_leds() { return led_config.prefix_leds; }

void config_set_prefix_leds(uint8_t value) {
  if (value > 1)
    return;
  led_config.prefix_leds = value;
}

int16_t config_get_refresh_rate() { return led_config.refresh_rate; }

void config_set_refresh_rate(int16_t value) {
  if ((value >= 2 && value <= 250) || value == -1)
    led_config.refresh_rate = value;
}

void config_set_artnet_width(uint16_t val) { led_config.artnet_width = val; }

void config_set_artnet_universe_offset(uint16_t val) {
  led_config.artnet_universe_offset = val;
}

uint16_t config_get_artnet_width() { return led_config.artnet_width; }

uint16_t config_get_artnet_universe_offset() {
  return led_config.artnet_universe_offset;
}

void config_set_partition_name(const char *pn) {
  strcpy(partition_name, pn);
  ESP_LOGI(TAG, "NVS partition name is %s", partition_name);
}
