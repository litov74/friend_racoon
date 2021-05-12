/**
 * *****************************************************************************
 * @file		app_device_desc.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Module responsible for storing device parameters that are used in
 * 				communication with the server
 *
 * *****************************************************************************
 */

//#define LOG_LOCAL_LEVEL		ESP_LOG_DEBUG

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdbool.h>
#include <string.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <mbedtls/md.h>

/* User files */
#include "app_device_desc.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_devdesc";

static const esp_partition_type_t partition_type = ESP_PARTITION_TYPE_DATA;
static const esp_partition_subtype_t partition_subtype = ESP_PARTITION_SUBTYPE_DATA_NVS;
static const char *partition_label = "desc";

/* Private structures --------------------------------------------------------*/

static esp_partition_t *devdesc_part = NULL;

/* Export functions ----------------------------------------------------------*/

/* Initialize esp_partition_t structure & device descriptor */
esp_err_t app_devdesc_init(app_devdesc_t *desc) {
	devdesc_part = app_devdesc_partition_init(partition_type, partition_subtype, partition_label);
	if (!devdesc_part) {
		ESP_LOGE(tag, "Failed to find partition!");
		return ESP_FAIL;
	}
	/* Device login */
	if (app_devdesc_login_read(desc->login, sizeof desc->login) != ESP_OK) {
		ESP_LOGE(tag, "Failed to read login");
		return ESP_FAIL;
	} else {
		ESP_LOGI(tag, "Login \t\t%s, length = %d", desc->login, strlen(desc->login));
	}
	/* Password & password hash */
	if (	app_devdesc_id_field_flag_get(SPI_FLASH_PASSWD_FLAG_ADDR_OFFSET) == true &&
			app_devdesc_id_field_flag_get(SPI_FLASH_PASS_HASH_FLAG_ADDR_OFFSET) == true) {
		ESP_ERROR_CHECK( app_devdesc_passwd_read(desc->passwd, DEVICE_PASSWD_STR_SIZE) );
		ESP_ERROR_CHECK( app_devdesc_hash_read(desc->pass_hash, sizeof desc->pass_hash) );
	} else {
		ESP_LOGW(tag, "No password exists");
		char *source = (char *)malloc(DEVICE_PASSWD_STR_SIZE);
		if (!source) {
			ESP_LOGE(tag, "Memory allocation failure");
			return ESP_FAIL;
		}
		app_devdesc_generate_passwd(source, DEVICE_PASSWD_STR_SIZE);
		ESP_ERROR_CHECK( app_devdesc_passwd_write(source, DEVICE_PASSWD_STR_SIZE * sizeof(char)) );
		free(source);
		ESP_ERROR_CHECK( app_devdesc_passwd_read(desc->passwd, DEVICE_PASSWD_STR_SIZE) );
		char *result = (char *)calloc(DEVICE_PASS_HASH_LENGTH, sizeof(char));
		if (!result) {
			ESP_LOGE(tag, "Memory allocation failure");
			return ESP_FAIL;
		}
		app_devdesc_generate_hash(result, desc->passwd, DEVICE_PASSWD_STR_SIZE);
		ESP_ERROR_CHECK( app_devdesc_hash_write(result, DEVICE_PASS_HASH_LENGTH * sizeof(char)) );
		free(result);
		ESP_ERROR_CHECK( app_devdesc_hash_read(desc->pass_hash, sizeof desc->pass_hash) );
	}
	ESP_LOGI(tag, "Password \t\t%s, length = %d", desc->passwd, strlen(desc->passwd));
	ESP_LOGD(tag, "Password hash \t%s, length = %d", desc->pass_hash, strlen(desc->pass_hash));
	/* Client ID */
	uint16_t len;
	if (app_devdesc_id_field_flag_get(SPI_FLASH_CLIENT_ID_FLAG_ADDR_OFFSET) == true) {
		len = (uint16_t)app_devdesc_id_field_len_read(SPI_FLASH_CLIENT_ID_SIZE_ADDR_OFFSET);
		ESP_ERROR_CHECK( app_devdesc_client_id_read(desc->client_id, len) );
		ESP_LOGI(tag, "Client ID \t%s, length = %u", desc->client_id, len);
	} else {
		ESP_LOGW(tag, "No client ID exists");
		return ESP_ERR_NOT_FOUND;
	}
	app_devdesc_string_read(desc->server_url, SPI_FLASH_URL_WORK_ADDR_OFFSET, MAX_WORK_URL_LENGTH);
	ESP_LOGI(tag, "Server URL \t%s", desc->server_url[0] == 0 ? "[empty]" : desc->server_url);
	return ESP_OK;
}

/* Get esp_partition_t structure and verify partition */
esp_partition_t *app_devdesc_partition_init(esp_partition_type_t type,
											esp_partition_subtype_t subtype,
											const char *label) {
	const esp_partition_t *ptr = esp_partition_find_first(type, subtype, label);
	if (!ptr) {
		return NULL;
	}
	const esp_partition_t *ptr_verified = esp_partition_verify(ptr);
	if (!ptr_verified) {
		return NULL;
	}
	return (esp_partition_t *)ptr_verified;
}

/* Fill a buffer with random bytes from hardware RNG */
void app_devdesc_generate_passwd(char *dest, size_t len) {
	const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	if (len) {
		for (size_t i = 0; i < len; ++i) {
			uint32_t seed = esp_random() % (uint32_t)(sizeof(charset) - 1);
			dest[i] = charset[seed];
		}
	}
}

/* Feed an input buffer into an ongoing message-digest computation
 * and write the result to the output buffer */
void app_devdesc_generate_hash(char *output, const char *input, size_t ilen) {
	mbedtls_md_context_t ctx;
	mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
	mbedtls_md_init(&ctx);
	mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
	mbedtls_md_starts(&ctx);
	mbedtls_md_update(&ctx, (const unsigned char *)input, ilen);
	mbedtls_md_finish(&ctx, (unsigned char *)output);
	mbedtls_md_free(&ctx);
}

/* Read device login data */
esp_err_t app_devdesc_login_read(char *dest, size_t len) {
	if (len != (size_t)(DEVICE_LOGIN_STR_SIZE + 1)) {
		return ESP_ERR_INVALID_ARG;
	}
	uint8_t *buf = (uint8_t *)calloc(DEVICE_MAC_ADDRESS_LENGTH, sizeof(uint8_t));
	if (!buf) {
		return ESP_ERR_NO_MEM;
	}
	ESP_ERROR_CHECK( esp_efuse_mac_get_default(buf) );
	for (int i = 0; i < DEVICE_MAC_ADDRESS_LENGTH; ++i) {
		char *tmp = (char *)calloc(3, sizeof(char));
		snprintf(tmp, 3, "%02x", buf[i]);
		strncat(dest, tmp, 2);
		free(tmp);
	}
	free(buf);
	return ESP_OK;
}

/* Read device password saved data */
esp_err_t app_devdesc_passwd_read(char *dest, size_t len) {
	if (len > (size_t)DEVICE_PASSWD_STR_SIZE) {
		return ESP_ERR_INVALID_ARG;
	}
	if (esp_partition_read(	devdesc_part,
							SPI_FLASH_PASSWD_ADDR_OFFSET,
							dest,
							len) != ESP_OK) {
		return ESP_FAIL;
	}
	dest[len] = '\0';
	return ESP_OK;
}

/* Write device password data to the partition */
esp_err_t app_devdesc_passwd_write(const char *src, size_t slen) {
	if (slen > (size_t)DEVICE_PASSWD_STR_SIZE) {
		return ESP_ERR_INVALID_ARG;
	}
	if (esp_partition_erase_range(	devdesc_part,
									SPI_FLASH_PASS_HASH_ADDR_OFFSET,
									0x1000) != ESP_OK) {
		return ESP_FAIL;
	}
	vTaskDelay(1);
	if (esp_partition_write(devdesc_part,
							SPI_FLASH_PASSWD_ADDR_OFFSET,
							src,
							slen) != ESP_OK) {
		return ESP_FAIL;
	}
	if (app_devdesc_id_field_flag_set(SPI_FLASH_PASSWD_FLAG_ADDR_OFFSET, true) != ESP_OK) {
		return ESP_FAIL;
	}
	ESP_LOGI(tag, "The password has been saved");
	return ESP_OK;
}

/* Read device password hash saved data */
esp_err_t app_devdesc_hash_read(char *dest, size_t len) {
	if (len != (size_t)(DEVICE_PASS_HASH_STR_SIZE + 1)) {
		return ESP_ERR_INVALID_ARG;
	}
	uint32_t *buf = (uint32_t *)calloc(DEVICE_PASS_HASH_STR_SIZE, sizeof(char));
	if (!buf) {
		return ESP_ERR_NO_MEM;
	}
	if (esp_partition_read(	devdesc_part,
							SPI_FLASH_PASS_HASH_ADDR_OFFSET,
							buf,
							DEVICE_PASS_HASH_STR_SIZE) != ESP_OK) {
		return ESP_FAIL;
	}
	for (int i = 0; i < DEVICE_PASS_HASH_STR_SIZE / 4; ++i) {
		uint8_t count = 0;
		for (int j = 4 * i; j < (4 * i) + 4; ++j) {
			dest[j] = (char)(buf[i] >> (24 - (8 * (count++))));
		}
	}
	dest[len - 1] = '\0';
	free(buf);
	return ESP_OK;
}

/* Write device password hash data to the partition */
esp_err_t app_devdesc_hash_write(const char *src, size_t len) {
	if (len != (size_t)DEVICE_PASS_HASH_LENGTH) {
		return ESP_ERR_INVALID_ARG;
	}
	char *ch_buf = (char *)calloc(DEVICE_PASS_HASH_STR_SIZE + 1, sizeof(char));
	if (!ch_buf) {
		return ESP_ERR_NO_MEM;
	}
	for (int i = 0; i < DEVICE_PASS_HASH_LENGTH; ++i) {
		char *tmp = (char *)calloc(3, sizeof(char));
		snprintf(tmp, 3, "%02x", src[i]);
		strncat(ch_buf, tmp, 2);
		free(tmp);
	}
	uint32_t *word_buf = (uint32_t *)malloc(DEVICE_PASS_HASH_STR_SIZE);
	if (!word_buf) {
		return ESP_ERR_NO_MEM;
	}
	for (int i = (DEVICE_PASS_HASH_STR_SIZE / 4) - 1; i >= 0; --i) {
		uint8_t tmp[4], count = 0;
		for (int j = 4 * i; j < (4 * i) + 4; ++j) {
			tmp[count++] = (uint8_t)ch_buf[j];
		}
		word_buf[i] = (uint32_t)((tmp[0] << 24) | (tmp[1] << 16) | (tmp[2] << 8) | tmp[3]);
	}
	free(ch_buf);
	vTaskDelay(1);
	if (esp_partition_write(devdesc_part,
							SPI_FLASH_PASS_HASH_ADDR_OFFSET,
							word_buf,
							DEVICE_PASS_HASH_STR_SIZE) != ESP_OK) {
		return ESP_FAIL;
	}
	free(word_buf);
	if (app_devdesc_id_field_flag_set(SPI_FLASH_PASS_HASH_FLAG_ADDR_OFFSET, true) != ESP_OK) {
		return ESP_FAIL;
	}
	ESP_LOGI(tag, "The hash has been saved");
	return ESP_OK;
}

/* Read device client ID saved data */
esp_err_t app_devdesc_client_id_read(char *dest, size_t len) {
	if (len > (size_t)DEVICE_CLIENT_ID_STR_SIZE) {
		return ESP_ERR_INVALID_ARG;
	}
	if (esp_partition_read(	devdesc_part,
							SPI_FLASH_CLIENT_ID_ADDR_OFFSET,
							dest,
							len) != ESP_OK) {
		return ESP_FAIL;
	}
	dest[len] = '\0';
	return ESP_OK;
}

/* Write device client ID data to the partition */
esp_err_t app_devdesc_client_id_write(const char *src, size_t slen) {
	if (slen > (size_t)DEVICE_CLIENT_ID_STR_SIZE) {
		return ESP_ERR_INVALID_ARG;
	}
	if (esp_partition_erase_range(	devdesc_part,
									SPI_FLASH_CLIENT_ID_ADDR_OFFSET,
									0x1000) != ESP_OK) {
		return ESP_FAIL;
	}
	vTaskDelay(1);
	if (esp_partition_write(devdesc_part,
							SPI_FLASH_CLIENT_ID_ADDR_OFFSET,
							src,
							slen) != ESP_OK) {
		return ESP_FAIL;
	}
	if (app_devdesc_id_field_flag_set(SPI_FLASH_CLIENT_ID_FLAG_ADDR_OFFSET, true) != ESP_OK) {
		return ESP_FAIL;
	}
	if (app_devdesc_id_field_len_write(SPI_FLASH_CLIENT_ID_SIZE_ADDR_OFFSET, (uint32_t)slen) != ESP_OK) {
		return ESP_FAIL;
	}
	ESP_LOGI(tag, "The client ID has been saved");
	return ESP_OK;
}

/* Read device user token saved data */
int app_devdesc_user_token_read(char *dest, size_t len) {
	if (len > (size_t)MAX_DEVICE_USER_TOKEN_STR_SIZE) {
		return ESP_ERR_INVALID_ARG;
	}
	if (esp_partition_read(	devdesc_part,
							SPI_FLASH_USER_TOKEN_ADDR_OFFSET,
							dest,
							len) != ESP_OK) {
		return ESP_FAIL;
	}
	dest[len] = '\0';
	return ESP_OK;
}

/* Write device user token data to the partition */
esp_err_t app_devdesc_user_token_write(const char *src, size_t slen) {
	if (slen > (size_t)MAX_DEVICE_USER_TOKEN_STR_SIZE) {
		return ESP_ERR_INVALID_ARG;
	}
	if (esp_partition_erase_range(	devdesc_part,
									SPI_FLASH_USER_TOKEN_ADDR_OFFSET,
									0x1000) != ESP_OK) {
		return ESP_FAIL;
	}
	vTaskDelay(1);
	if (esp_partition_write(devdesc_part,
							SPI_FLASH_USER_TOKEN_ADDR_OFFSET,
							src,
							slen) != ESP_OK) {
		return ESP_FAIL;
	}
	if (app_devdesc_id_field_flag_set(SPI_FLASH_USER_TOKEN_FLAG_ADDR_OFFSET, true) != ESP_OK) {
		return ESP_FAIL;
	}
	if (app_devdesc_id_field_len_write(SPI_FLASH_USER_TOKEN_SIZE_ADDR_OFFSET, (uint32_t)slen) != ESP_OK) {
		return ESP_FAIL;
	}
	ESP_LOGI(tag, "The user token has been saved");
	return ESP_OK;
}

/* Read device identification data field related length in bytes */
int app_devdesc_id_field_len_read(size_t src_offset) {
	uint32_t lvalue = 0;
	uint32_t *buf = (uint32_t *)calloc(1, sizeof(uint32_t));
	if (!buf) {
		return ESP_ERR_NO_MEM;
	}
	if (esp_partition_read(	devdesc_part,
							src_offset,
							buf,
							sizeof(uint32_t)) != ESP_OK) {
		return ESP_FAIL;
	}
	lvalue = buf[0];
	free(buf);
	return (int)lvalue;
}

/* Write device identification data field related length in bytes */
esp_err_t app_devdesc_id_field_len_write(size_t dst_offset, uint32_t len) {
	uint32_t *buf = (uint32_t *)calloc(1, sizeof(uint32_t));
	if (!buf) {
		return ESP_ERR_NO_MEM;
	}
	buf[0] = len;
	if (esp_partition_write(devdesc_part,
							dst_offset,
							buf,
							sizeof(uint32_t)) != ESP_OK) {
		return ESP_FAIL;
	}
	free(buf);
	return ESP_OK;
}

/* Get device identification field related flag state */
int app_devdesc_id_field_flag_get(size_t src_offset) {
	bool lvalue = false;
	uint32_t *buf = (uint32_t *)calloc(1, sizeof(uint32_t));
	if (!buf) {
		return ESP_ERR_NO_MEM;
	}
	if (esp_partition_read(	devdesc_part,
							src_offset,
							buf,
							sizeof(uint32_t)) != ESP_OK) {
		return ESP_FAIL;
	}
	if (buf[0] & true) {
		lvalue = false;
	} else {
		lvalue = true;
	}
	free(buf);
	return (int)lvalue;
}

/* Set device identification field related flag state */
esp_err_t app_devdesc_id_field_flag_set(size_t dst_offset, bool rvalue) {
	uint32_t *buf = (uint32_t *)calloc(1, sizeof(uint32_t));
	if (!buf) {
		return ESP_ERR_NO_MEM;
	}
	buf[0] = ~rvalue;
	if (esp_partition_write(devdesc_part,
							dst_offset,
							buf,
							sizeof(uint32_t)) != ESP_OK) {
		return ESP_FAIL;
	}
	free(buf);
	return ESP_OK;
}

/* Write URLs to the partition */
esp_err_t app_devdesc_url_write(app_devdesc_t *desc, const char *url_work, const char *url_upgrade) {
	if (esp_partition_erase_range(	devdesc_part,
									SPI_FLASH_URLS_ADDR_OFFSET,
									0x1000) != ESP_OK) {
		return ESP_FAIL;
	}
	vTaskDelay(1);
	size_t slen;
	if (url_work != NULL) {
		slen = strlen(url_work);
		if (slen >= MAX_WORK_URL_LENGTH) {
			return ESP_ERR_INVALID_ARG;
		}
		if (esp_partition_write(devdesc_part,
								SPI_FLASH_URL_WORK_ADDR_OFFSET + sizeof(slen),
								url_work,
								slen) != ESP_OK) {
			return ESP_FAIL;
		}
		if (esp_partition_write(devdesc_part,
								SPI_FLASH_URL_WORK_ADDR_OFFSET,
								&slen,
								sizeof(slen)) != ESP_OK) {
			return ESP_FAIL;
		}
	}
	if (url_upgrade != NULL) {
		slen = strlen(url_upgrade);
		if (slen >= MAX_FIRMWARE_UPGRADE_URL_LENGTH) {
			return ESP_ERR_INVALID_ARG;
		}
		if (esp_partition_write(devdesc_part,
								SPI_FLASH_URL_UPGRADE_ADDR_OFFSET + sizeof(slen),
								url_upgrade,
								slen) != ESP_OK) {
			return ESP_FAIL;
		}
		if (esp_partition_write(devdesc_part,
								SPI_FLASH_URL_UPGRADE_ADDR_OFFSET,
								&slen,
								sizeof(slen)) != ESP_OK) {
			return ESP_FAIL;
		}
	}
	// инициализируем сразу и в дескрипторе ссылку на сервер
	app_devdesc_string_read(desc->server_url, SPI_FLASH_URL_WORK_ADDR_OFFSET, MAX_WORK_URL_LENGTH);
	ESP_LOGI(tag, "Server URL \t%s", desc->server_url[0] == 0 ? "[empty]" : desc->server_url);
	ESP_LOGI(tag, "The upgrade URL has been saved");
	return ESP_OK;
}

/* Read string length from the partition */
size_t app_devdesc_string_get_length(size_t src_offset, size_t limit) {
	size_t len;
	if (esp_partition_read(	devdesc_part,
							src_offset,
							&len,
							sizeof(size_t)) != ESP_OK) {
		return 0;
	}
	if (len > limit) {
		return 0;
	}
	return len;
}

/* Read string from the partition */
esp_err_t app_devdesc_string_read(char *dest, size_t src_offset, size_t limit) {
	dest[0] = '\0';
	size_t len = app_devdesc_string_get_length(src_offset, limit);
	if (len == 0) {
		return ESP_FAIL;
	}
	if (esp_partition_read(	devdesc_part,
							src_offset + sizeof(size_t),
							dest,
							len) != ESP_OK) {
		return ESP_FAIL;
	}
	dest[len] = '\0';
	return ESP_OK;
}

/* Delete identification data from device memory */
esp_err_t app_devdesc_clear_device_descriptor_data(void) {
	if (esp_partition_erase_range(	devdesc_part,
									SPI_FLASH_PASS_HASH_ADDR_OFFSET,
									0x4000) != ESP_OK) {
		return ESP_FAIL;
	}
	return ESP_OK;
}
