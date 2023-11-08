/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#include "websocket.h"

#include <esp_log.h>
#include <string.h>

#include "webjson.h"
#include "websession.h"

static const char *TAG =
    strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__;

#if 0
/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    static const char * data = "Async data";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t echo_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "echo handler");
    uint8_t buf[128] = {0};
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 128);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char *)ws_pkt.payload, "Trigger async") == 0)
    {
        return trigger_async_send(req->handle, req);
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    }
    return ret;
}
#endif

static esp_err_t echo_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "echo handler");
  uint8_t buf[1600] = {0};
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = buf;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
    return ret;
  }
  ESP_LOGI(TAG, "Got packet with message: %s %d %d", ws_pkt.payload, ws_pkt.len,
           ws_pkt.fragmented);
  ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
  if (ws_pkt.fragmented) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed with fragmented");
    return ESP_FAIL;
  }

  websession_setWebsocket(req->handle, httpd_req_to_sockfd(req), true);

  return webjson_parseMessage(req->handle, httpd_req_to_sockfd(req),
                              ws_pkt.payload, ws_pkt.len);
}

static const httpd_uri_t ws = {.uri = "/ws",
                               .method = HTTP_GET,
                               .handler = echo_handler,
                               .user_ctx = NULL,
                               .is_websocket = true};

void websocket_on(httpd_handle_t web_handle) {
  httpd_register_uri_handler(web_handle, &ws);
}

void websocket_off(httpd_handle_t web_handle) {
  httpd_unregister_uri_handler(web_handle, ws.uri, ws.method);
}