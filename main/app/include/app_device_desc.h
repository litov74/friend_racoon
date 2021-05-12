/**
 * *****************************************************************************
 * @file		app_device_desc.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Module responsible for storing device parameters that are used in
 * 				communication with the server
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef APP_DEVICE_DESC_H__
#define APP_DEVICE_DESC_H__

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdbool.h>
#include <stdint.h>

/* Framework */
#include <esp_err.h>
#include <esp_partition.h>

/* Export constants ----------------------------------------------------------*/

/**
 * @defgroup	app_device_desc_mem_fields Device descriptor key values location addresses
 * @brief		Memory addresses at which the parameter values ​​are located for working with the
 * 				device descriptor
 * @{
 */

#define DEVICE_MAC_ADDRESS_LENGTH	6
#define DEVICE_LOGIN_STR_SIZE		(2 * DEVICE_MAC_ADDRESS_LENGTH)

#define DEVICE_PASSWD_STR_SIZE				8
#define SPI_FLASH_PASSWD_ADDR_OFFSET		0x44
#define SPI_FLASH_PASSWD_FLAG_ADDR_OFFSET	0x4C

#define DEVICE_PASS_HASH_LENGTH					32
#define DEVICE_PASS_HASH_STR_SIZE				(2 * DEVICE_PASS_HASH_LENGTH)
#define SPI_FLASH_PASS_HASH_ADDR_OFFSET			0x00
#define SPI_FLASH_PASS_HASH_FLAG_ADDR_OFFSET	0x40

#define DEVICE_CLIENT_ID_STR_SIZE				36
#define SPI_FLASH_CLIENT_ID_ADDR_OFFSET			0x1000
#define SPI_FLASH_CLIENT_ID_FLAG_ADDR_OFFSET	0x1800
#define SPI_FLASH_CLIENT_ID_SIZE_ADDR_OFFSET	0x1804

#define MAX_DEVICE_USER_TOKEN_STR_SIZE			2048
#define SPI_FLASH_USER_TOKEN_ADDR_OFFSET		0x2000
#define SPI_FLASH_USER_TOKEN_FLAG_ADDR_OFFSET	0x2800
#define SPI_FLASH_USER_TOKEN_SIZE_ADDR_OFFSET	0x2804

#define SPI_FLASH_URLS_ADDR_OFFSET			0x3000
#define SPI_FLASH_URL_UPGRADE_ADDR_OFFSET	0x3000
#define SPI_FLASH_URL_WORK_ADDR_OFFSET		(SPI_FLASH_URL_UPGRADE_ADDR_OFFSET + MAX_FIRMWARE_UPGRADE_URL_LENGTH + 4)

#define MAX_FIRMWARE_UPGRADE_VERSION_LENGTH		32
#define MAX_FIRMWARE_UPGRADE_URL_LENGTH			0xFF
#define MAX_WORK_URL_LENGTH						0xFF

/** @}*/

/* Export typedef ------------------------------------------------------------*/

/** @brief	The structure used to describe a device in the context of a server connection */
typedef struct {
	char login[DEVICE_LOGIN_STR_SIZE + 1];			/*!< Used device login */
	char passwd[DEVICE_PASSWD_STR_SIZE + 1];		/*!< Used device password */
	char pass_hash[DEVICE_PASS_HASH_STR_SIZE + 1];	/*!< Used device password hash */
	char client_id[DEVICE_CLIENT_ID_STR_SIZE + 1];	/*!< Used client ID */
	char server_url[MAX_WORK_URL_LENGTH + 1];		/*!< Used server URL */
} app_devdesc_t;

/* Export functions ----------------------------------------------------------*/

/**
 * @brief		Initialize esp_partition_t structure & device descriptor
 * @param[in]	device_desc_t	Pointer to structure used to describe the device
 * @return
 * 				- ESP_ERR_NOT_FOUND: Requested resource not found
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_init(app_devdesc_t *desc);

/**
 * @brief		Get esp_partition_t structure and verify partition
 * @param[in]	type	Partition type, one of esp_partition_type_t values
 * @param[in]	subtype	Partition subtype, one of esp_partition_subtype_t values.
 * 						To find all partitions of given type, use ESP_PARTITION_SUBTYPE_ANY
 * @param[in]	label	Partition label. Set this value if looking for partition with a specific name
 * @return
 * 				- If partition not found, returns NULL
 * 				- Pointer to esp_partition_t structure. This pointer is valid for the lifetime of the application
 */
esp_partition_t *app_devdesc_partition_init(esp_partition_type_t type,
											esp_partition_subtype_t subtype,
											const char *label);

/**
 * @brief		Fill a buffer with random bytes from hardware RNG
 * @param[out]	dest	Pointer to buffer to fill with random numbers
 * @param[in]	len		Length of buffer in bytes
 * @return
 * 				- None
 */
void app_devdesc_generate_passwd(char *dest, size_t len);

/**
 * @brief		Feed an input buffer into an ongoing message-digest computation
 * 				and write the result to the output buffer
 * @param[out]	output	Pointer to buffer for the generic message-digest checksum result
 * @param[in]	input	Pointer to buffer holding the input data
 * @param[in]	ilen	Length of the input data
 * @return
 * 				- None
 */
void app_devdesc_generate_hash(char *output, const char *input, size_t ilen);

/**
 * @brief		Read device login data
 * @param[out]	dest	Pointer to the buffer where data should be stored
 * @param[in]	len		Result buffer size
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_login_read(char *dest, size_t len);

/**
 * @brief		Read device password saved data
 * @param[out]	dest	Pointer to the buffer where data should be stored
 * @param[in]	len		Result buffer size
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_passwd_read(char *dest, size_t len);

/**
 * @brief		Write device password data to the partition
 * @param[in]	src		Pointer to the source buffer
 * @param[in]	slen	Source string length
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_passwd_write(const char *src, size_t slen);

/**
 * @brief		Read device password hash saved data
 * @param[out]	dest	Pointer to the buffer where data should be stored
 * @param[in]	len		Result buffer size
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_FAIL: Partition not found
 *				- ESP_OK: Success
 */
esp_err_t app_devdesc_hash_read(char *dest, size_t len);

/**
 * @brief		Write device password hash data to the partition
 * @param[in]	src	Pointer to the source buffer
 * @param[in]	len	Source buffer size
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_hash_write(const char *src, size_t len);

/**
 * @brief		Read device client ID saved data
 * @param[out]	dest	Pointer to the buffer where data should be stored
 * @param[in]	len		Required data length
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_client_id_read(char *dest, size_t len);

/**
 * @brief		Write device client ID data to the partition
 * @param[in]	src		Pointer to the source buffer
 * @param[in]	slen	Source string length
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_FAIL: Either partition not found or unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_client_id_write(const char *src, size_t slen);

/**
 * @brief		Read device user token saved data
 * @param[out]	dest	Pointer to the buffer where data should be stored
 * @param[in]	len		Required data length
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_user_token_read(char *dest, size_t len);

/**
 * @brief		Write device user token data to the partition
 * @param[in]	src		Pointer to the source buffer
 * @param[in]	slen	Source string length
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_FAIL: Either partition not found or unexpected error
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_user_token_write(const char *src, size_t slen);

/**
 * @brief		Read device identification data field related length in bytes
 * @param[in]	src_offset	Address of the data to be read, relative to the
 * 							beginning of the partition
 * @return
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_FAIL: Partition not found
 * 				- Data field length
 */
int app_devdesc_id_field_len_read(size_t src_offset);

/**
 * @brief		Write device identification data field related length in bytes
 * @param[in]	dst_offset	Address where the data should be written, relative to the
 * 							beginning of the partition
 * @param[in]	len			Data field length
 * @return
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_id_field_len_write(size_t dst_offset, uint32_t len);

/**
 * @brief		Get device identification field related flag state
 * @param[in]	src_offset	Address of the data to be read, relative to the
 * 							beginning of the partition
 * @return
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_FAIL: Partition not found
 * 				- true or false
 */
int app_devdesc_id_field_flag_get(size_t src_offset);

/**
 * @brief		Set device identification field related flag state
 * @param[in]	dst_offset	Address where the data should be written, relative to the
 * 							beginning of the partition
 * @param[in]	rvalue		true or false
 * @return
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_id_field_flag_set(size_t dst_offset, bool rvalue);

/**
 * @brief		Write URLs to the partition
 * @param[in]	url_work		URL work server
 * @param[in]	url_upgrade		URL get firmware upgrade
 * @return
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_url_write(app_devdesc_t *desc, const char *url_work, const char *url_upgrade);

/**
 * @brief		Read string length from the partition
 * @param[in]	src_offset	Address of the data to be read, relative to the
 * 							beginning of the partition
 * @param[in]	limit		String length limit
 * @return
 * 				- Length of string
 */
size_t app_devdesc_string_get_length(size_t src_offset, size_t limit);

/**
 * @brief		Read string from the partition
 * @param[out]	dest		Pointer to the buffer where data should be stored
 * @param[in]	src_offset	Address of the data to be read, relative to the
 * 							beginning of the partition
 * @param[in]	limit		String length limit
 * @return
 * 				- ESP_FAIL: Partition not found
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_OK: Success
 */
esp_err_t app_devdesc_string_read(char *dest, size_t src_offset, size_t limit);

/**
 * @brief	Delete identification data from device memory
 * @param	None
 * @return
 * 			- ESP_FAIL	Unexpected error
 * 			- ESP_OK	Success
 */
esp_err_t app_devdesc_clear_device_descriptor_data(void);

#endif	/* APP_DEVICE_DESC_H__ */
