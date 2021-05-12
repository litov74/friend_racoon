/**
 * *****************************************************************************
 * @file		app_wifi.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Application functionality related to wireless network connection
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef APP_WIFI_H__
#define APP_WIFI_H__

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdint.h>

/* Framework */
#include <esp_err.h>
#include <esp_wifi.h>

/* User files */
#include "app_spiffs.h"

/* Export constants ----------------------------------------------------------*/

extern const char *wifi_ap_recs_path;	/*!< Name of the file containing the data added in the
										 * configuration process for connecting to access points */

#define DEFAULT_WIFI_SSID_LEN		32	/*!< The amount of memory allocated for AP SSID in bytes */
#define DEFAULT_WIFI_PASSWORD_LEN	64	/*!< The amount of memory allocated for AP password in bytes */
#define DEFAULT_SCAN_LIST_SIZE		4	/*!< The amount of memory allocated for access point data when scanning.
										 * Multiple of wifi_ap_record_t structure size */

/**
 * @defgroup	wifi_event_bits WiFi event group related bits
 * @{
 */
#define BIT_STA_CONNECTED			BIT0
#define BIT_STA_DISCONNECTED		BIT1
#define BIT_CONN_TO_INTERNET_OK		BIT2
#define BIT_CONN_TO_INTERNET_FAIL	BIT3
#define BIT_CHECK_PENDING			BIT4
#define BIT_NEW_WIFI_CONF			BIT5
#define BIT_RECONNECT				BIT6
#define BIT_CONN_CORRUPTED			BIT7

/** @}*/

/* Export typedef ------------------------------------------------------------*/

/** @brief	STA configuration settings structure for the ESP32 */
typedef struct {
	char ssid[DEFAULT_WIFI_SSID_LEN + 1];			/*!< Used SSID of AP */
	char password[DEFAULT_WIFI_PASSWORD_LEN + 1];	/*!< Used password of AP */
} wifi_auth_params_t;

/* Export functions ----------------------------------------------------------*/

/**
 * @defgroup	app_wifi_event WiFi event wait functions
 * @brief		Helper functions to wait for a specific event from the WiFi module
 * @{
 */

/**
 * @brief		Wait for signal when the station is connected & ready to make a request
 * @param[in]	event_group	FreeRTOS event group
 * @return
 * 				- None
 */
void app_wifi_wait_sta_connected(EventGroupHandle_t event_group);

/**
 * @brief		Wait for signal when the station is disconnected
 * @param[in]	event_group	FreeRTOS event group
 * @return
 * 				- None
 */
void app_wifi_wait_sta_disconnected(EventGroupHandle_t event_group);

/**
 * @brief		Wait for signal when the station made an attempt to connect to
 * 				access point with the given parameters
 * @param[in]	event_group	FreeRTOS event group
 * @return
 * 				- None
 */
void app_wifi_wait_conn_attempt(EventGroupHandle_t event_group);

/** @}*/

/**
 * @defgroup	app_wifi_init Functions used during WiFi module operation
 * @brief		WiFi/LwIP initialization phase, WiFi configuration phase &
 * 				WiFi start phase
 * @{
 */

/**
 * @brief		Initialize the application WiFi node
 * @param[in]	ctx	User context
 * @param[in]	arg	Launch parameter
 * @return
 * 				- None
 */
void app_wifi_init(void *ctx, int arg);

/**
 * @brief		Join to the specified access point
 * @param[in]	arg		User context
 * @param[in]	mode	WiFi operating mode (WIFI_MODE_STA or WIFI_MODE_APSTA)
 * @param[in]	ssid	SSID of target AP
 * @param[in]	pass	Password of target AP
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_wifi_sta_join(void *arg,
							wifi_mode_t mode,
							const char *ssid,
							const char *pass);

/**
 * @brief		Disconnect from an access point
 * @param[in]	arg	User context
 * @return
 * 				- None
 */
void app_wifi_sta_detach(void *arg);

/**
 * @brief		Set up a soft access point
 * @param[in]	ssid	SSID of own AP
 * @param[in]	pass	Password of own AP
 * @return
 * 				- None
 */
void app_wifi_apsta_set(const char *ssid, const char *pass);

/**
 * @brief	Initialize station+soft-AP mode
 * @param	None
 * @return
 * 			- None
 */
void app_wifi_switch_to_apsta(void);

/**
 * @brief		Scan for available set of APs
 * @param[in]	ap_num		As input param, it stores max AP number ap_records can hold.
 * 							As output param, it receives the actual AP number this API returns
 * @param[out]	ap_list_buf	wifi_ap_record_t array to hold the found APs
 * @return
 * 				- Actual number of APs
 */
int32_t app_wifi_scan(uint16_t *ap_num, wifi_ap_record_t *ap_list_buf);

/**
 * @brief		Compare list of available APs with list of APs that is stored in memory
 * @param[in]	scan_ap_num			Actual found AP number
 * @param[in]	scan_ap_records		Array to hold the found APs
 * @param[in]	spiffs_ap_num		Actual storage AP number
 * @param[in]	spiffs_ap_records	Array to hold the storage APs
 * @return
 * 				- ESP_ERR_NOT_FOUND: Requested resource not found
 * 				- Index of the suitable AP
 */
esp_err_t app_wifi_check_if_ap_exists(	int scan_ap_num,
										wifi_ap_record_t *scan_ap_records,
										int spiffs_ap_num,
										app_spiffs_ap_record_t *spiffs_ap_records);

/** @}*/

/**
 * @brief		WiFi initialization task
 * @param[in]	arg	User context
 * @return
 * 				- None
 */
void app_wifi_init_task(void *arg);

#endif	/* APP_WIFI_H__ */
