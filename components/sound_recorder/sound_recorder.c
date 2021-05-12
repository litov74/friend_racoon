/**
 * *****************************************************************************
 * @file		sound_recorder.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		This module describes the structure of the application's sound recorder
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
#include "sound_recorder.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "recorder";

/* Export functions ----------------------------------------------------------*/

/* Initialize a voice recorder instance */
esp_err_t sound_recorder_init(sound_recorder_t *recorder) {
	if (!recorder) {
		return ESP_FAIL;
	}
	/* Attempt to create a semaphore */
	if (!recorder->semphr) {
		if ((recorder->semphr = xSemaphoreCreateBinary()) != NULL) {
			ESP_LOGD(tag, "The binary semaphore was created successfully");
		} else {
			ESP_LOGD(tag, "The memory required to hold the binary semaphore could not be allocated");
			while (!recorder->semphr) {
				vTaskDelay(1);
				if ((recorder->semphr = xSemaphoreCreateBinary()) != NULL) {
					ESP_LOGD(tag, "The binary semaphore was created successfully");
				}
			}
		}
	}
	xSemaphoreGive(recorder->semphr);
	/* Create a queue capable of containing RECORDER_QUEUE_SIZE blocks of RECORDER_TRANS_BUF_SIZE bytes */
	if (!recorder->queue) {
		if ((recorder->queue = xQueueCreate(RECORDER_QUEUE_SIZE, RECORDER_TRANS_BUF_SIZE)) != NULL) {
			ESP_LOGI(tag, "The queue was created successfully");
		} else {
			ESP_LOGI(tag, "The memory required to hold the queue could not be allocated");
			while (!recorder->queue) {
				vTaskDelay(1);
				if ((recorder->queue = xQueueCreate(RECORDER_QUEUE_SIZE, RECORDER_TRANS_BUF_SIZE)) != NULL) {
					ESP_LOGI(tag, "The queue was created successfully");
				}
			}
		}
	}
	/* Fill in WAV file header */
	strcpy(recorder->wav_hdr.ChunkID, "RIFF");
	strncpy(recorder->wav_hdr.Format, "WAVE", strlen("WAVE"));
	strncpy(recorder->wav_hdr.Subchunk1ID, "fmt ", strlen("fmt "));
	recorder->wav_hdr.Subchunk1Size = 16;
	recorder->wav_hdr.AudioFormat = 1;
	recorder->wav_hdr.NumChannels = 1;
	recorder->wav_hdr.SampleRate = 16000;
	recorder->wav_hdr.BitsPerSample = 16;
	recorder->wav_hdr.ByteRate =	recorder->wav_hdr.NumChannels *
									recorder->wav_hdr.SampleRate *
									recorder->wav_hdr.BitsPerSample /
									8;
	recorder->wav_hdr.BlockAlign =	recorder->wav_hdr.NumChannels *
									recorder->wav_hdr.BitsPerSample /
									8;
	strncpy(recorder->wav_hdr.Subchunk2ID, "data", strlen("data"));
	return ESP_OK;
}
