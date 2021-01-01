/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* LED Controller for a matrix of smart LEDs */
/* Copyright (C) 2018-2021 Symonics GmbH, Christian Hoene */

/*
 * decoding.c
 *
 *  Created on: 02.09.2018
 *      Author: hoene
 */

#include "decoding.h"

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led.h"
#include "mjpeg.h"
#include "picojpeg.h"

// static const char *TAG = "#decoding";

static TaskHandle_t taskHandle;
static int counter;
static struct MJPEG_FILE *file;

static unsigned char pNeed_bytes_callback(unsigned char *pBuf,
                                          unsigned char buf_size,
                                          unsigned char *pBytes_actually_read,
                                          void *pCallback_data) {
  *pBytes_actually_read =
      buf_size + counter > file->size ? file->size - counter : buf_size;
  memcpy(pBuf, file->buffer + counter, *pBytes_actually_read);
  counter += *pBytes_actually_read;
  return 0;
}

/*
 *
 typedef struct
 {
 // Image resolution
 int m_width;
 int m_height;

 // Number of components (1 or 3)
 int m_comps;

 // Total number of minimum coded units (MCU's) per row/col.
 int m_MCUSPerRow;
 int m_MCUSPerCol;

 // Scan type
 pjpeg_scan_type_t m_scanType;

 // MCU width/height in pixels (each is either 8 or 16 depending on the scan
 type) int m_MCUWidth; int m_MCUHeight;

 // m_pMCUBufR, m_pMCUBufG, and m_pMCUBufB are pointers to internal MCU Y or RGB
 pixel component buffers.
 // Each time pjpegDecodeMCU() is called successfully these buffers will be
 filled with 8x8 pixel blocks of Y or RGB pixels.
 // Each MCU consists of (m_MCUWidth/8)*(m_MCUHeight/8) Y/RGB blocks: 1 for
 greyscale/no subsampling, 2 for H1V2/H2V1, or 4 blocks for H2V2 sampling
 factors.
 // Each block is a contiguous array of 64 (8x8) bytes of a single component:
 either Y for grayscale images, or R, G or B components for color images.
 //
 // The 8x8 pixel blocks are organized in these byte arrays like this:
 //
 // PJPG_GRAYSCALE: Each MCU is decoded to a single block of 8x8 grayscale
 pixels.
 // Only the values in m_pMCUBufR are valid. Each 8 bytes is a row of pixels
 (raster order: left to right, top to bottom) from the 8x8 block.
 //
 // PJPG_H1V1: Each MCU contains is decoded to a single block of 8x8 RGB pixels.
 //
 // PJPG_YH2V1: Each MCU is decoded to 2 blocks, or 16x8 pixels.
 // The 2 RGB blocks are at byte offsets: 0, 64
 //
 // PJPG_YH1V2: Each MCU is decoded to 2 blocks, or 8x16 pixels.
 // The 2 RGB blocks are at byte offsets: 0,
 //                                       128
 //
 // PJPG_YH2V2: Each MCU is decoded to 4 blocks, or 16x16 pixels.
 // The 2x2 block array is organized at byte offsets:   0,  64,
 //                                                   128, 192
 //
 // It is up to the caller to copy or blit these pixels from these buffers into
 the destination bitmap. unsigned char *m_pMCUBufR; unsigned char *m_pMCUBufG;
 unsigned char *m_pMCUBufB;
 } pjpeg_image_info_t;
 */

static void block(int no, pjpeg_image_info_t *block) {
  int x = block->m_MCUWidth * (no % block->m_MCUSPerRow);
  int y = block->m_MCUHeight * (no / block->m_MCUSPerRow);

  /*
          ESP_LOGD(TAG,
                          "block %d image %d %d got %d %d size %d %d st %d c %d
     pos %d %d", no, block->m_width, block->m_height, block->m_MCUWidth,
                          block->m_MCUHeight, block->m_MCUSPerRow,
     block->m_MCUSPerCol, (int )block->m_scanType, block->m_comps, x, y);
  */
  switch (block->m_scanType) {
  case PJPG_GRAYSCALE:
    led_block(x, y, block->m_pMCUBufR, block->m_pMCUBufR, block->m_pMCUBufR);
    break;
  case PJPG_YH1V1:
    led_block(x, y, block->m_pMCUBufR, block->m_pMCUBufG, block->m_pMCUBufB);
    break;
  case PJPG_YH2V1:
    led_block(x, y, block->m_pMCUBufR, block->m_pMCUBufG, block->m_pMCUBufB);
    led_block(x + 8, y, block->m_pMCUBufR + 64, block->m_pMCUBufG + 64,
              block->m_pMCUBufB + 64);
    break;
  case PJPG_YH1V2:
    led_block(x, y, block->m_pMCUBufR, block->m_pMCUBufG, block->m_pMCUBufB);
    led_block(x, y + 8, block->m_pMCUBufR + 128, block->m_pMCUBufG + 128,
              block->m_pMCUBufB + 128);
    break;
  case PJPG_YH2V2:
    led_block(x, y, block->m_pMCUBufR, block->m_pMCUBufG, block->m_pMCUBufB);
    led_block(x + 8, y, block->m_pMCUBufR + 64, block->m_pMCUBufG + 64,
              block->m_pMCUBufB + 64);
    led_block(x, y + 8, block->m_pMCUBufR + 128, block->m_pMCUBufG + 128,
              block->m_pMCUBufB + 128);
    led_block(x + 8, y + 8, block->m_pMCUBufR + 192, block->m_pMCUBufG + 192,
              block->m_pMCUBufB + 192);
    break;
  }
}

static void decodeFile() {
  file = mjpeg_waitfor_frame(taskHandle);
  counter = 0;

  // Initializes the decompressor.
  pjpeg_image_info_t pInfo;
  int res = pjpeg_decode_init(&pInfo, pNeed_bytes_callback, (void *)file, 0);
  ESP_ERROR_CHECK(res == 0 ? ESP_OK : ESP_FAIL);

  int i = 0;
  for (;;) {
    // Decompresses the file's next MCU. Returns 0 on success,
    // PJPG_NO_MORE_BLOCKS if no more blocks are available, or an error code.
    res = pjpeg_decode_mcu();
    if (res == PJPG_NO_MORE_BLOCKS)
      break;
    ESP_ERROR_CHECK(res == 0 ? ESP_OK : ESP_FAIL);

    block(i, &pInfo);
    i++;
  }
  mjpeg_release_frame();
}

static void task(void *args) {
  for (;;) {
    decodeFile();
  }
}

void decoding_on() {
  ESP_ERROR_CHECK(xTaskCreate(task, "decoder", 4096, NULL, tskIDLE_PRIORITY + 9,
                              &taskHandle) == pdPASS
                      ? ESP_OK
                      : ESP_FAIL);
}

void decoding_off() { vTaskDelete(taskHandle); }
