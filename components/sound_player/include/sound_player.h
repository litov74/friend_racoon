/**
 * *****************************************************************************
 * @file		sound_player.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		This module describes the structure of the application's audio player
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef SOUND_PLAYER_H__
#define SOUND_PLAYER_H__

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_http_client.h>

/* User files */
#include "uuid.h"

/* Export constants ----------------------------------------------------------*/

#define PLAYER_RECV_BUF_SIZE	DEFAULT_HTTP_BUF_SIZE
#define PLAYER_QUEUE_SIZE		100U
#define PLAYER_BUF_CAP_MSG		(PLAYER_QUEUE_SIZE / 2)

/* Export typedef ------------------------------------------------------------*/

/** @brief	HTTP sound getter state space enumeration */
typedef enum {
	GETTER_IDLE = 0,
	GETTER_STARTING,
	GETTER_BUFFERING,
	GETTER_ACTIVE,
	GETTER_PAUSE,
	GETTER_STOP_AT_THE_END,
	GETTER_HALT,
} http_sound_getter_state_e;

/** @brief	A sound player related structure */
typedef struct {
	double pend_tr_cnt;								/*!< The current number of tracks in the queue */
	uuid_t pend_tr_id;								/*!< Unique identifier of the track being played */
	double vol;										/*!< Current sound level value from 0 to 100 */
	BaseType_t is_muted;							/*!< Audio output has been disabled flag */
	/* Buffers */
	char http_buf[PLAYER_RECV_BUF_SIZE + 1];		/*!< Buffer used to store data received from server */
	uint8_t codec_buf[PLAYER_RECV_BUF_SIZE];		/*!< Buffer used to store the next chunk of the
													 * audio file to be played */
	/* Variable used to store current player state value */
	http_sound_getter_state_e state;				/*<! Current HTTP sound getter related state machine state */
	/* HTTP client handles */
	esp_http_client_handle_t http_cleaner_client;	/*!< HTTP sound commands cleaner network connection instance */
	esp_http_client_handle_t http_getter_client;	/*!< HTTP sound getter network connection instance */
	/* FreeRTOS mechanics */
	QueueHandle_t queue;							/*!< Queue for storing chunks of the audio file being played */
	SemaphoreHandle_t semphr;						/*!< Binary semaphore used to lock resources associated with
													 * sound player */
	TaskHandle_t decoder_hdl;						/*!< Reference of the audio data getter task */
	TaskHandle_t getter_hdl;						/*!< Reference of the audio data decoder task */
} sound_player_t;

/* Export functions prototypes -----------------------------------------------*/

/**
 * @brief		Initialize a sound player instance
 * @param[in]	player	A pointer to sound player instance
 * @return
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t sound_player_init(sound_player_t *player);

#endif	/* SOUND_PLAYER_H__ */
