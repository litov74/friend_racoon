/**
 * *****************************************************************************
 * @file		sound_player.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		This module describes the structure of the application's audio player
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <string.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_err.h>
#include <esp_log.h>

/* User files */
#include "sound_player.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "player";

/* Export functions ----------------------------------------------------------*/

/* Initialize a sound player instance */
esp_err_t sound_player_init(sound_player_t *player) {
	if (!player) {
		return ESP_FAIL;
	}
	/* Attempt to create a semaphore */
	if (!player->semphr) {
		if ((player->semphr = xSemaphoreCreateBinary()) != NULL) {
			ESP_LOGD(tag, "The binary semaphore was created successfully");
		} else {
			ESP_LOGD(tag, "The memory required to hold the binary semaphore could not be allocated");
			while (!player->semphr) {
				vTaskDelay(1);
				if ((player->semphr = xSemaphoreCreateBinary()) != NULL) {
					ESP_LOGD(tag, "The binary semaphore was created successfully");
				}
			}
		}
	}
	xSemaphoreGive(player->semphr);
	/* Create a queue capable of containing PLAYER_QUEUE_SIZE blocks of PLAYER_RECV_BUF_SIZE bytes */
	if (!player->queue) {
		if ((player->queue = xQueueCreate(PLAYER_QUEUE_SIZE, PLAYER_RECV_BUF_SIZE)) != NULL) {
			ESP_LOGD(tag, "The queue was created successfully");
		} else {
			ESP_LOGD(tag, "The memory required to hold the queue could not be allocated");
			while (!player->queue) {
				vTaskDelay(1);
				if ((player->queue = xQueueCreate(PLAYER_QUEUE_SIZE, PLAYER_RECV_BUF_SIZE)) != NULL) {
					ESP_LOGD(tag, "The queue was created successfully");
				}
			}
		}
	}
	/* Set the initial player key values */
	player->pend_tr_cnt = 0;
	player->vol = 0;
	player->is_muted = pdFALSE;
	memset(player->pend_tr_id.b, 0, sizeof player->pend_tr_id.b);
	return ESP_OK;
}
