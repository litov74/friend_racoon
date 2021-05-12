/**
 * *****************************************************************************
 * @file		app_spiffs.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Module for working with files
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef APP_SPIFFS_H__
#define APP_SPIFFS_H__

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdint.h>

/* Framework */
#include "esp_err.h"

/* Export constants ----------------------------------------------------------*/

#define SPIFFS_WIFI_SSID_LENGTH		32
#define SPIFFS_WIFI_PASSWORD_LENGTH	64

/* Export typedef ------------------------------------------------------------*/

/** @brief	Storage STA configuration settings for the ESP32 */
typedef struct {
	uint8_t ssid[SPIFFS_WIFI_SSID_LENGTH];			/*!< Saved SSID of target AP */
	uint8_t password[SPIFFS_WIFI_PASSWORD_LENGTH];	/*!< Saved password of target AP */
} app_spiffs_ap_record_t;

/* Export functions ----------------------------------------------------------*/

/**
 * @brief	Initialize SPIFFS
 * @param	None
 * @return
 * 			- ESP_FAIL: Parameter error
 *     		- ESP_OK: Success
 */
esp_err_t app_spiffs_init(void);

/**
 * @brief		Detach SPIFFS
 * @param[in]	partition_label	Same label as passed to esp_vfs_spiffs_register
 * @return
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_spiffs_deinit(const char *partition_label);

/**
 * @brief		Create a file
 * @param[in]	filename	Specified filename
 * @return
 * 				- ESP_ERR_INVALID_ARG: File already exists
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_spiffs_create_file(const char *filename);

/**
 * @brief		Clear the contents of a file
 * @param[in]	filename	Specified filename
 * @return
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_spiffs_erase_file(const char *filename);

/**
 * @brief		Count the number of lines in a document
 * @param[in]	filename	Specified filename
 * @return
 * 			- ESP_FAIL: Unexpected error
 * 			- ESP_OK: File read
 * 			- Number of lines
 */
int app_spiffs_get_lines_num(const char *filename);

/**
 * @brief		Read existing records
 * @param[in]	filename	Specified filename
 * @param[in]	ap_num		As input param, it stores max AP number ap_records can hold.
 * 							As output param, it receives the actual AP number this API returns
 * @param[out]	ap_records	spiffs_ap_record_t array to hold the saved APs
 * @return
 * 			- ESP_ERR_NOT_FOUND: File not found
 * 			- ESP_FAIL: Unexpected error
 * 			- ESP_OK: Success
 */
esp_err_t app_spiffs_read_records(const char *filename, int *ap_num, app_spiffs_ap_record_t *ap_records);

/**
 * @brief		Insert new record
 * @param[in]	filename	Specified filename
 * @param[in]	ap_num		As input param, it stores max AP number
 * @param[in]	ssid		SSID of target AP
 * @param[in]	pass		Password of target AP
 * @return
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_ERR_NOT_FOUND: File not found
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_spiffs_insert_record(	const char *filename,
									int *ap_num,
									const char *ssid,
									const char *pass);

/**
 * @brief		Get a password by the specified line index
 * @param[in]	filename	Specified filename
 * @param[in]	idx			Line index
 * @param[out]	ap_records	spiffs_ap_record_t array to hold the saved APs
 * @return
 * 				- ESP_ERR_NOT_FOUND: File not found
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_spiffs_get_password(const char *filename, uint16_t *idx, app_spiffs_ap_record_t *ap_records);

#endif	/* APP_SPIFFS_H__ */
