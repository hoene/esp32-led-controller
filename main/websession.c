/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

#include "websession.h"

#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>
#include <sys/socket.h>

static const char *TAG = strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__;

/**
 * handling a list of all open websockets
 */
struct WebsessionNode
{
	httpd_handle_t hd;
	int sockfd;
	bool websocket;
	struct WebsessionNode *next;
};

static struct WebsessionNode *websessions = NULL;

void websession_send_to_all(httpd_ws_frame_t *ws_pkt)
{
	struct WebsessionNode *n = websessions;
	while (n != NULL)
	{
		if (n->websocket)
		{
			httpd_ws_send_frame_async(n->hd, n->sockfd, ws_pkt);
		}
		n = n->next;
	}
}

int websession_add(httpd_handle_t hd, int sockfd)
{
	ESP_LOGI(TAG, "session add %p %d", hd, sockfd);

	struct WebsessionNode *n = malloc(sizeof(struct WebsessionNode));
	if (n == NULL)
		return ESP_ERR_NO_MEM;

	n->hd = hd;
	n->sockfd = sockfd;
	n->websocket = false;
	n->next = websessions;
	websessions = n;

	//	esp_ota_mark_app_valid_cancel_rollback();

	return ESP_OK;
}

int websession_setWebsocket(httpd_handle_t hd, int sockfd, bool websocket)
{
	struct WebsessionNode *n = websessions;

	while (n != NULL)
	{
		if (n->hd == hd && n->sockfd == sockfd)
		{
			n->websocket = websocket;
			return ESP_OK;
		}
		n = n->next;
	}
	return ESP_FAIL;
}

void websession_remove(httpd_handle_t hd, int sockfd)
{

	ESP_LOGI(TAG, "session rm %p %d", hd, sockfd);

	close(sockfd);

	struct WebsessionNode *previous = NULL;
	struct WebsessionNode *n = websessions;

	while (n != NULL)
	{
		if (n->sockfd == sockfd && n->hd == hd)
		{
			struct WebsessionNode *next = n->next;
			free(n);
			if (previous)
				previous->next = next;
			else
				websessions = next;
			n = next;
		}
		else
		{
			previous = n;
			n = n->next;
		}
	}
}

void websession_removeAll()
{
	while (websessions != NULL)
	{
		struct WebsessionNode *node = websessions;
		websessions = websessions->next;
		free(node);
	}
}
