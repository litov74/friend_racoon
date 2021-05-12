/**
 * *****************************************************************************
 * @file		sound_recorder.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		This module describes the structure of the application's sound recorder
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef SOUND_RECORDER_H__
#define SOUND_RECORDER_H__

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_http_client.h>
#include <esp_spi_flash.h>

/* Export constants ----------------------------------------------------------*/

#define RECORDER_TRANS_BUF_SIZE		1024
#define RECORDER_QUEUE_SIZE			200U

/* Export typedef ------------------------------------------------------------*/

/** @brief	The header of a WAV (RIFF) file is 44 bytes long and has the following format */
typedef struct {
	char ChunkID[4];
	int ChunkSize;
	char Format[4];
	char Subchunk1ID[4];
	int Subchunk1Size;
	short AudioFormat;
	short NumChannels;
	int SampleRate;
	int ByteRate;
	short BlockAlign;
	short BitsPerSample;
	char Subchunk2ID[4];
	int Subchunk2Size;
} wav_header_t;

/** @brief	I2S sampler state space enumeration */
typedef enum {
	SAMPLER_IDLE = 0,
	SAMPLER_STARTING,
	SAMPLER_ACTIVE,
	SAMPLER_HALT,
} i2s_sampler_state_e;

/** @brief	A sound recorder related structure */
typedef struct {
	int16_t rec_buf[RECORDER_TRANS_BUF_SIZE / 2];	/*!< Buffer used to store data sampled from microphone */
	char http_buf[RECORDER_TRANS_BUF_SIZE];			/*!< Buffer used to store the next chunk of the
													 * audio record to be sent */
	wav_header_t wav_hdr;							/*!< The header of a WAV (RIFF) file to be sent */
	i2s_sampler_state_e state;						/*<! Current sound recorder related state machine state */
	esp_http_client_handle_t http_client;			/*!< HTTP sound sender network connection instance */
	QueueHandle_t queue;							/*!< Queue for storing chunks of the audio file being sent */
	SemaphoreHandle_t semphr;						/*!< Binary semaphore used to lock resources associated with
													 * sound recorder */
	TaskHandle_t sampler_hdl;						/*!< Reference of the audio data recorder task */
	TaskHandle_t sender_hdl;						/*!< Reference of the audio data sender task */
} sound_recorder_t;

/* Export functions ----------------------------------------------------------*/

/**
 * @brief		Initialize a voice recorder instance
 * @param[in]	voice_recorder	A pointer to voice recorder instance
 * @return
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t sound_recorder_init(sound_recorder_t *recorder);

#endif	/* SOUND_RECORDER_H__ */
