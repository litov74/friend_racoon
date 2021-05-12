/**
 * *****************************************************************************
 * @file		app_client.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Application functionality related to performing media tasks
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef APP_CLIENT_H__
#define APP_CLIENT_H__

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_http_client.h>

/* User files */
#include "app_device_desc.h"
#include "sound_recorder.h"
#include "uuid.h"

/* Export constants ----------------------------------------------------------*/

/**
 * @brief	Size of wav files sent to the server when the voice recording function is enabled
 * *****************************************************************************
 * @note	The functionality of recording and sending audio files is implemented using a queue.
 * 			The number of queue elements and the size of one of its elements are set in the
 * 			"sound recorder" component.
 * *****************************************************************************
 */
#define QUEUE_MESSAGES_WAITING_THRESHOLD	(RECORDER_QUEUE_SIZE / 4)

/* Some commonly used status codes */
#define HTTP_200	200	/*!< OK */
#define HTTP_204	204	/*!< No Content */
#define HTTP_207	207	/*!< Multi-Status */
#define HTTP_400	400	/*!< Bad Request */
#define HTTP_401	401	/*!< Unauthorized */
#define HTTP_404	404	/*!< Not Found */
#define HTTP_406	406	/*!< Not Acceptable */
#define HTTP_408	408	/*!< Request Timeout */
#define HTTP_500	500	/*!< Internal Server Error */

/* Export typedef ------------------------------------------------------------*/

/** @brief	Structure used to describe the device profile */
typedef struct {
	BaseType_t is_muted;					/*!< Audio output has been disabled flag */
	BaseType_t is_player;					/*!< Sound player active flag */
	BaseType_t is_recorder;					/*!< Sound recorder active flag */
	char id[DEVICE_CLIENT_ID_STR_SIZE + 1];	/*!< String for storing device ID */
	char name[33];							/*!< String for storing device name */
	double vol;								/*!< Current sound level value from 0 to 100 */
	double track_cnt;						/*!< The current number of tracks in the queue */
	uuid_t track_id;						/*!< Unique identifier of the track being played */
} app_client_profile_t;

/** @brief	Application web client node related structure */
typedef struct {
	BaseType_t led_tracker;					/*!< Flag used to store the state of the user LED */
	sound_player_t player;					/*!< An instance of the application sound player structure*/
	sound_recorder_t sampler;				/*!< An instance of the application sound recorder structure*/
	esp_http_client_handle_t http_client;	/*!< The esp_http_client handle used to make profile requests */
	SemaphoreHandle_t semphr;				/*!< Binary semaphore used to lock resources associated with profile requests */
	TaskHandle_t hdl;						/*!< Reference of the main task of the application's client module */
} app_client_func_t;

/* Export functions ----------------------------------------------------------*/

/**
 * @brief		Execute the end-of-reproduction request
 * @param[in]	player	A pointer to the application sound player instance
 * @return
 * 				- None
 */
void app_client_delete_track(sound_player_t *player);

/**
 * @defgroup	app_client_rtos_tasks FreeRTOS tasks of the client module of the application
 * @brief		A group that combines tasks related to requesting a profile, receiving audio data for
 * 				playback and sending recorded audio data
 * @{
 */

/**
 * @brief		Requests the current state of the profile from the server and
 * 				performs actions in accordance with the values ​​of its parameters
 * @param[in]	arg	A pointer to the application HTTP client instance
 * @return
 * 				- None
 */
void http_profile_getter_task(void *arg);

/**
 * @brief		Get an audio data from an HTTP server
 * @param[in]	arg	A pointer to the application sound player instance
 * @return
 * 				- None
 */
void http_sound_getter_task(void *arg);

/**
 * @brief		Feed an audio data to the VS1053b decoder chip
 * @param[in]	arg	A pointer to the application sound player instance
 * @return
 * 				- None
 */
void sound_decoder_task(void *arg);

/**
 * @brief		Send audio recordings to the server
 * @param[in]	arg	A pointer to the application sound recorder instance
 * @return
 * 				- None
 */
void http_sound_sender_task(void *arg);

/**
 * @brief		Get an output data from the PDM microphone
 * @param[in]	arg	A pointer to the application sound recorder instance
 * @return
 * 				- None
 */
void sound_recorder_task(void *arg);

/** @}*/

/**
 * @defgroup	app_client_utils Client module helper functions
 * @brief		Auxiliary functions that are used to perform certain actions based on
 * 				the values ​​of the device profile parameters
 * @{
 */

/**
 * @brief		Get JSON string that contains current device profile keys values
 * @param[in]	cli_hdl	esp_http_client_handle_t context
 * @param[out]	profile	A pointer to app_device_profile_t structure to fill
 * @return
 * 				- ESP_ERR_NOT_FOUND: Critical parameter was not found
 * 				- ESP_ERR_INVALID_STATE: Device is not authorized
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_client_get_device_profile(esp_http_client_handle_t cli_hdl, app_client_profile_t *profile);

/**
 * @brief		Parse JSON string that contains current device profile keys values
 * @param[out]	profile	A pointer to app_device_profile_t structure to fill
 * @param[in]	content	JSON string
 * @return
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_client_parse_profile(app_client_profile_t *profile, const char * const content);

/**
 * @brief		Change the state of the player in accordance with the current
 * 				values ​​of the profile keys
 * @param[out]	player	A pointer to the application player structure instance
 * @param[in]	profile	A pointer to app_device_profile_t intermediate buffer structure
 * @return
 * 				- None
 */
void app_client_set_player_state(sound_player_t *player, app_client_profile_t *profile);

/**
 * @brief		Change the state of the recorder in accordance with the current
 * 				values ​​of the profile keys
 * @param[out]	sampler	A pointer to the application recorder structure instance
 * @return
 * 				- None
 */
void app_client_set_sampler_state(sound_recorder_t *sampler, app_client_profile_t *profile);

/**
 * @brief		Suspend execution of all current media tasks
 * @param[in]	arg		Application web client context
 * @return
 * 				- None
 */
void app_client_halt_media_tasks(void *arg);

/** @}*/

#endif	/* APP_CLIENT_H__ */
