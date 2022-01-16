/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#include "web.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <string.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>

#include "websession.h"
#include "websocket.h"
#include "html.h"
#include "mjpeg.h"

static const char *TAG = strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__;

esp_err_t get_handler_jpeg(httpd_req_t *req)
{
	ESP_LOGI(TAG, "JPEG %d", mjpeg_has_frame());
	if (!mjpeg_has_frame())
		return httpd_resp_send_404(req);

	struct MJPEG_FILE *file = mjpeg_waitfor_frame(xTaskGetCurrentTaskHandle());

	ESP_LOGD(TAG, "send jpg %d %p", file->size, file->buffer);

	httpd_resp_set_type(req, "image/jpeg");
	httpd_resp_set_hdr(req, "Cache-Control", "no-store");
	httpd_resp_set_hdr(req, "Refresh", "2");
	httpd_resp_send(req, (const char *)file->buffer, file->size);
	mjpeg_release_frame();
	return ESP_OK;
}

// ROUTE_CGI_ARG("*",cgiRedirectToHostname,"192.168.13.1"),

static httpd_handle_t web_handle = NULL;
static struct HTML_FILE *web_index_html = NULL;

const char *mimetypes[] = {
	".css", "text/css",
	".js", "text/javascript",
	".svg", "image/svg+xml",
	".ico", "image/x-icon",
	NULL};

/* Our URI handler function to be called during GET /uri request */
esp_err_t get_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "http get file %s", req->uri);

	int len = strlen(req->uri);
	if (len > 1)
	{
		for (struct HTML_FILE *p = html_files; p->data != NULL; p++)
		{
			if (!memcmp((void *)req->uri + 1, (void *)p->name, len - 1) && !memcmp((void *)p->name + len - 1, (void *)".gz", 4))
			{
				if (len > 4)
				{
					const char **p = mimetypes;
					while (*p)
					{
						int lensuffix = strlen(p[0]);
						if (len > lensuffix)
						{
							if (!memcmp(req->uri + len - lensuffix, p[0], lensuffix))
							{
								httpd_resp_set_type(req, p[1]);
								break;
							}
						}
						p += 2;
					}
				}
				httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
				return httpd_resp_send(req, (const char *)p->data, *p->size);
			}
		}
	}
	if (web_index_html == NULL)
	{
		for (struct HTML_FILE *p = html_files; p->data != NULL; p++)
		{
			if (!strcmp(p->name, "index.html.gz"))
			{
				web_index_html = p;
				break;
			}
		}
	}
	if (web_index_html != NULL && !strcmp(req->uri, "/"))
	{
		httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
		return httpd_resp_send(req, (const char *)web_index_html->data, *web_index_html->size);
	}
	return httpd_resp_send_404(req);
}

esp_err_t OTA_reboot_post_handler(httpd_req_t *req)
{
	char ota_buff[128];
	int content_length = req->content_len;
	int recv_len;
	/* Read the data for the request */
	if ((recv_len = httpd_req_recv(req, ota_buff, content_length < sizeof(ota_buff) ? content_length : sizeof(ota_buff))) < 0)
	{
		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
		{
			ESP_LOGE(TAG, "socket timeout");
			httpd_resp_send_408(req);
		}
		else
			ESP_LOGE(TAG, "other error %d", recv_len);
		return ESP_FAIL;
	}

	httpd_resp_send(req, "done", 5);
	esp_restart();
	return ESP_OK;
}

/* Receive .Bin file */
esp_err_t OTA_update_post_handler(httpd_req_t *req)
{
	esp_ota_handle_t ota_handle;

	char ota_buff[128];
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	do
	{
		/* Read the data for the request */
		if ((recv_len = httpd_req_recv(req, ota_buff, content_length < sizeof(ota_buff) ? content_length : sizeof(ota_buff))) < 0)
		{
			if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
			{
				ESP_LOGE(TAG, "socket timeout");
				httpd_resp_send_408(req);
			}
			else
				ESP_LOGE(TAG, "other error %d", recv_len);
			return ESP_FAIL;
		}
		//		printf("OTA RX: %d of %d : %p %s\r\n", content_received, content_length, ota_buff, ota_buff);

		// Is this the first data we are receiving
		// If so, it will have the information in the header we need.
		if (!is_req_body_started)
		{
			is_req_body_started = true;
			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				ESP_LOGE(TAG, "Error With OTA Begin, Cancelling OTA\r\n");
				httpd_resp_send_500(req);
				return ESP_FAIL;
			}
			else
			{
				ESP_LOGD(TAG, "Writing to partition subtype %d at offset 0x%x", update_partition->subtype, update_partition->address);
			}
		}
		// Write OTA data
		esp_ota_write(ota_handle, ota_buff, recv_len);
		content_received += recv_len;
	} while (recv_len > 0 && content_received < content_length);

	if (esp_ota_end(ota_handle) == ESP_OK)
	{
		// Lets update the partition
		if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
		{
			const esp_partition_t *boot_partition = esp_ota_get_boot_partition();

			ESP_LOGI(TAG, "Next boot partition subtype %d at offset 0x%x", boot_partition->subtype, boot_partition->address);
			ESP_LOGI(TAG, "Please restart system");
			httpd_resp_send(req, "done", 5);
		}
		else
		{
			ESP_LOGE(TAG, "OTA Flash Error");
			httpd_resp_send_500(req);
			return ESP_FAIL;
		}
	}
	else
	{
		ESP_LOGI(TAG, "OTA End Error");
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}

	return ESP_OK;
}

/* URI handler structure for GET /uri */
static const httpd_uri_t uri_get = {
	.uri = "/*",
	.method = HTTP_GET,
	.handler = get_handler,
	.user_ctx = NULL};

static const httpd_uri_t uri_get_jpeg = {
	.uri = "/rtp.jpg",
	.method = HTTP_GET,
	.handler = get_handler_jpeg,
	.user_ctx = NULL};

static const httpd_uri_t OTA_update = {
	.uri = "/upload",
	.method = HTTP_POST,
	.handler = OTA_update_post_handler,
	.user_ctx = NULL};

static const httpd_uri_t OTA_reboot = {
	.uri = "/reboot",
	.method = HTTP_POST,
	.handler = OTA_reboot_post_handler,
	.user_ctx = NULL};

/* Function for starting the webserver */
void web_on()
{
	/* Generate default configuration */
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	//	config.max_uri_handlers = 5;
	config.uri_match_fn = httpd_uri_match_wildcard;
	config.open_fn = websession_add;
	config.close_fn = websession_remove;
	config.stack_size = 1024 * 6;
	config.lru_purge_enable = true;

	/* Empty handle to esp_http_server */
	assert(!web_handle);

	/* Start the httpd server */
	if (httpd_start(&web_handle, &config) == ESP_OK)
	{
		websocket_on(web_handle);
		/* Register URI handlers */
		ESP_ERROR_CHECK(httpd_register_uri_handler(web_handle, &uri_get_jpeg));
		ESP_ERROR_CHECK(httpd_register_uri_handler(web_handle, &uri_get));
		ESP_ERROR_CHECK(httpd_register_uri_handler(web_handle, &OTA_update));
		ESP_ERROR_CHECK(httpd_register_uri_handler(web_handle, &OTA_reboot));
	}
	// TODO		espFsInit((void*) (webpages_espfs_start));
	// TODO		captdnsInit();
}

void web_off()
{

	if (web_handle)
	{
		/* Stop the httpd server */
		websocket_off(web_handle);
		httpd_stop(web_handle);
	}
	websession_removeAll();
}
