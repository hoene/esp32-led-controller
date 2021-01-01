/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#include "web.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <string.h>

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

// ROUTE_CGI_ARG("/upload", cgiUploadFirmware, &uploadParams),
// ROUTE_CGI("/rebootfirmware", cgiRebootFirmware),
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

#if 0
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Our URI handler function to be called during POST /uri request */
esp_err_t post_handler(httpd_req_t *req)
{
	/* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
	char content[100];

	/* Truncate if content length larger than the buffer */
	size_t recv_size = MIN(req->content_len, sizeof(content));

	int ret = httpd_req_recv(req, content, recv_size);
	if (ret <= 0)
	{ /* 0 return value indicates connection closed */
		/* Check if timeout occurred */
		if (ret == HTTPD_SOCK_ERR_TIMEOUT)
		{
			/* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
			httpd_resp_send_408(req);
		}
		/* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
		return ESP_FAIL;
	}

	/* Send a simple response */
	const char resp[] = "URI POST Response";
	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}
#endif

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

#if 0
/* URI handler structure for POST /uri */
static const httpd_uri_t uri_post = {
	.uri = "/uri",
	.method = HTTP_POST,
	.handler = post_handler,
	.user_ctx = NULL};
#endif

/* Function for starting the webserver */
void web_on()
{
	/* Generate default configuration */
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.max_uri_handlers = 4;
	config.uri_match_fn = httpd_uri_match_wildcard;
	config.open_fn = websession_add;
	config.close_fn = websession_remove;

	/* Empty handle to esp_http_server */
	assert(!web_handle);

	/* Start the httpd server */
	if (httpd_start(&web_handle, &config) == ESP_OK)
	{
		websocket_on(web_handle);
		/* Register URI handlers */
		httpd_register_uri_handler(web_handle, &uri_get_jpeg);
		httpd_register_uri_handler(web_handle, &uri_get);
		//		httpd_register_uri_handler(web_handle, &uri_post);
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
