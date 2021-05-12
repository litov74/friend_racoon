/**
 * *****************************************************************************
 * @file		uuid.c
 * *****************************************************************************
 * @brief		UUID Generator Tool
 * @attention	When WiFi or Bluetooth are enabled, numbers returned by hardware random number generator (RNG)
 * 				can be considered true random numbers. Without Wi-Fi or Bluetooth enabled, hardware RNG is a pseudo-random number generator.
 * 				At startup, ESP-IDF bootloader seeds the hardware RNG with entropy, but care must be taken when reading
 * 				random values between the start of app_main and initialization of Wi-Fi or Bluetooth drivers
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdbool.h>
#include <string.h>

/* Framework */
#include <ctype.h>
#include <esp_err.h>
#include <esp_system.h>

/* User files */
#include "uuid.h"

/* Export variables ----------------------------------------------------------*/

const uint8_t uuid_index[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

/* Private functions ---------------------------------------------------------*/

/**
 * @brief		Convert a hex digit to its real value
 * @param[in]	ch	ASCII character represents hex digit
 * @return
 * 				-	hex_to_bin() converts one hex digit to its actual value or
 * 					ESP_FAIL in case of bad input
 */
static inline esp_err_t _hex_to_bin(char ch) {
	if ((ch >= '0') && (ch <= '9')) {
		return ch - '0';
	}
	ch = tolower((int)ch);
	if ((ch >= 'a') && (ch <= 'f')) {
		return ch - 'a' + 10;
	}
	return ESP_FAIL;
}

static esp_err_t _uuid_parse(const char *uuid, uint8_t b[16], const uint8_t ei[16]) {
	static const uint8_t si[16] = { 0, 2, 4, 6, 9, 11, 14, 16, 19, 21, 24, 26, 28, 30, 32, 34 };
	unsigned int i;
	if (!uuid_is_valid(uuid)) {
		return -ESP_ERR_INVALID_ARG;
	}
	for (i = 0; i < 16; i++) {
		int hi = _hex_to_bin(uuid[si[i] + 0]);
		int lo = _hex_to_bin(uuid[si[i] + 1]);
		b[ei[i]] = (hi << 4) | lo;
	}
	return ESP_OK;
}

/* Export functions ----------------------------------------------------------*/

/* Generate a random UUID */
void generate_random_uuid(unsigned char uuid[16]) {
	esp_fill_random(uuid, 16);
	/* Set UUID version to 4 --- truly random generation */
	uuid[6] = (uuid[6] & 0x0F) | 0x40;
	/* Set the UUID variant to DCE */
	uuid[8] = (uuid[8] & 0x3F) | 0x80;
}

/* Checks if a UUID string is valid */
bool uuid_is_valid(const char *uuid) {
	unsigned int i;
	for (i = 0; i < UUID_STRING_LEN; i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			if (uuid[i] != '-') {
				return false;
			}
		} else if (!isxdigit((int)uuid[i])) {
			return false;
		}
	}
	return true;
}

esp_err_t uuid_parse(const char *uuid, uuid_t *u) {
	return _uuid_parse(uuid, u->b, uuid_index);
}

esp_err_t uuid_to_string(const uuid_t *u, char *uuid, size_t len) {
	if (len < UUID_NULL_TERM_STRING_LEN) return -ESP_ERR_INVALID_ARG;
	snprintf(uuid, UUID_NULL_TERM_STRING_LEN, "%02x%02x%02x%02x-"
			"%02x%02x-"
			"%02x%02x-"
			"%02x%02x-"
			"%02x%02x%02x%02x%02x%02x",
			u->b[0], u->b[1], u->b[2], u->b[3], u->b[4], u->b[5], u->b[6], u->b[7],
			u->b[8], u->b[9], u->b[10], u->b[11], u->b[12], u->b[13], u->b[14], u->b[15]);
	return ESP_OK;
}
