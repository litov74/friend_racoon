/**
 * *****************************************************************************
 * @file		uuid.h
 * *****************************************************************************
 * @brief		UUID Generator Tool
 * @attention	When WiFi or Bluetooth are enabled, numbers returned by hardware random number generator (RNG)
 * 				can be considered true random numbers. Without Wi-Fi or Bluetooth enabled, hardware RNG is a pseudo-random number generator.
 * 				At startup, ESP-IDF bootloader seeds the hardware RNG with entropy, but care must be taken when reading
 * 				random values between the start of app_main and initialization of Wi-Fi or Bluetooth drivers
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef ESP_UUID_H__
#define ESP_UUID_H__

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdbool.h>

/* Framework */
#include <ctype.h>
#include <esp_err.h>
#include <esp_system.h>

/* Export constants ----------------------------------------------------------*/

#define UUID_SIZE	16	/*!< UUID size in bytes */

/** @brief	The length of a UUID string ("00112233-4455-6677-8899-aabbccddeeff") */
/* Not including trailing zero */
#define	UUID_STRING_LEN				36
/* Including trailing zero */
#define	UUID_NULL_TERM_STRING_LEN	37

/* Export typedef ------------------------------------------------------------*/

typedef struct {
	uint8_t b[UUID_SIZE];
} uuid_t;

/* Export macro --------------------------------------------------------------*/

#define UUID_INIT(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)					\
( (uuid_t)																	\
{{	((a) >> 24) & 0xFF, ((a) >> 16) & 0xFF, ((a) >> 8) & 0xFF, (a) & 0xFF,	\
	((b) >> 8) & 0xFF, (b) & 0xFF,											\
	((c) >> 8) & 0xFF, (c) & 0xFF,											\
	(d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) }} )

/* External variables --------------------------------------------------------*/

extern const uint8_t uuid_index[16];

/* Export functions prototypes -----------------------------------------------*/

/**
 * @brief		Generate a random UUID
 * @param[out]	uuid	Where to put the generated UUID
 * @return
 * 				- None
 */
void generate_random_uuid(unsigned char uuid[16]);

/**
 * @brief		Checks if a UUID string is valid
 * *****************************************************************************
 * @note		It checks if the UUID string is following the format:
 * 				XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
 * 				where X is a hex digit
 * *****************************************************************************
 * @param[in]	uuid	UUID string to check
 * @return
 * 				- true if input is valid UUID string
 */
bool uuid_is_valid(const char *uuid);

esp_err_t uuid_parse(const char *uuid, uuid_t *u);

esp_err_t uuid_to_string(const uuid_t *u, char *uuid, size_t len);

#endif	/* ESP_UUID_H__ */
