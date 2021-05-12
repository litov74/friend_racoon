/**
 * *****************************************************************************
 * @file		app.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Application's common module
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef APP_SPECIFICS_H__
#define APP_SPECIFICS_H__

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdint.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_timer.h>

/* User files */
#include "sound_player.h"
#include "sound_recorder.h"
#include "app_client.h"
#include "app_device_desc.h"
#include "app_server.h"
#include "app_wifi.h"

/* Export constants ----------------------------------------------------------*/

#define MAX_HTTP_RECV_BUF		DEFAULT_HTTP_BUF_SIZE
#define MAX_HTTP_TRANS_BUF		(4 * DEFAULT_HTTP_BUF_SIZE)
#define COMMON_RING_BUF_SIZE	(10 * DEFAULT_HTTP_BUF_SIZE)	/*!< Size of the general purpose ring buffer in bytes */

#define FIRMWARE_VERSION_PREFIX		"Racoon.D1."

//#define DEVELOP_VERSION

/* Export typedef ------------------------------------------------------------*/

/** @brief	Set of URLs used during device operation */

typedef struct {
	char regdev[DEFAULT_HTTP_BUF_SIZE];
	char login[DEFAULT_HTTP_BUF_SIZE];
	char player[DEFAULT_HTTP_BUF_SIZE];
	char profile[DEFAULT_HTTP_BUF_SIZE];
	char sampler[DEFAULT_HTTP_BUF_SIZE];
} app_uri_set_t;

/** @brief	Main application structure */
typedef struct {
	wifi_auth_params_t wifi_config;		/*!< STA configuration settings structure for the ESP32 */
	app_devdesc_t device;				/*!< An instance of a device descriptor containing parameters for
										 * authorization on the server */
	app_uri_set_t uri;					/*!< URLs set structure instance */
	app_client_func_t client;			/*!< Instance of the structure responsible for the
										 * functionality of the client module */
	esp_timer_handle_t tim;				/*!< General purpose timer instance */
	httpd_handle_t web_server;			/*!< Application's web server instance */
	EventGroupHandle_t event_group;		/*!< An instance of the group of application events that occur during the
										 * operation of the WiFi module */
	RingbufHandle_t rbuf_hdl;			/*!< General purpose ring buffer instance */
	SemaphoreHandle_t spi_flash_mtx;	/*!< MUTEX for locking a shared resource used when accessing SPI flash */
} app_network_conn_t;

/** @brief	Structure containing parameters used when initializing the WiFi module */
typedef struct {
	int init_state;
	app_network_conn_t *app_ptr;
} app_wifi_initializer_t;

/* External variables --------------------------------------------------------*/

/** @brief	The main application structure instance */
extern app_network_conn_t app_instance;

/* Export functions ----------------------------------------------------------*/

/**
 * @brief		Access the shared resource
 * @param[in]	semphr		A handle to the semaphore being taken
 * @param[in]	block_time	The time in ticks to wait for the semaphore to become
 * 							available
 * @return
 * 				- ESP_FAIL: xTicksToWait expired without the semaphore becoming available
 * 				- ESP_OK: The semaphore was obtained
 */
esp_err_t app_semaphore_take(SemaphoreHandle_t semphr, TickType_t block_time);

/**
 * @brief		Free the semaphore
 * @param[in]	semphr	A handle to the semaphore being released
 * @return
 * 				- None
 */
void app_semaphore_give(SemaphoreHandle_t semphr);

/**
 * @brief		Application initialization phase
 * @param[in]	arg	Pointer to the application structure
 * @return
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_init(app_network_conn_t *arg);

/**
 * @brief	Get total amount and free amount of memory under control of HIMEM API
 * @param	None
 * @return
 * 			- None
 */
void app_himem_get_size_info(void);

/**
 * @brief	Restarts the device
 * @param	None
 * @return
 * 			- None
 */
void app_restart_device(void);

/**
 * @brief	Delete connection settings from device memory
 * @param	None
 * @return
 * 			- None
 */
void app_clear_device_connection_data(void);

void app_uri_init(app_network_conn_t *arg);

#endif	/* APP_SPECIFICS_H__ */
