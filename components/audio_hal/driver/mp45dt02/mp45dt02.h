/**
 * *****************************************************************************
 * @file		mp45dt02.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		MP45DT02 digital MEMS microphone driver
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef MP45DT02_H__
#define MP45DT02_H__

/* Includes ------------------------------------------------------------------*/

/* STBLIB */
#include <stddef.h>

/* Framework */
#include <esp_err.h>

/* Export functions ----------------------------------------------------------*/

/**
 * @brief	Initialize MP45DT02 digital MEMS microphone
 * @param	None
 * @return
 * 			- ESP_FAIL: Unexpected error
 *     		- ESP_OK: Success
 */
esp_err_t mp45dt02_init(void);

/**
 * @brief	Start MP45DT02 digital MEMS microphone
 * @param	None
 * @return
 * 			- ESP_FAIL: Unexpected error
 * 			- ESP_OK: Success
 */
esp_err_t mp45dt02_start(void);

/**
 * @brief		Read audio samples from the I2S microphone module
 * @param[out]	dest			Destination address to read into
 * @param[in]	size			Size of data in bytes
 * @param[out]	bytes_read		Number of bytes read, if timeout, bytes read will be less than the size passed in
 * @param[in]	ticks_to_wait	RX buffer wait timeout in RTOS ticks. If this many ticks pass without bytes becoming
 * 								available in the DMA receive buffer, then the function will return (note that if data is
 * 								read from the DMA buffer in pieces, the overall operation may still take longer than this
 * 								timeout). Pass portMAX_DELAY for no timeout
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter error
 * 				- ESP_OK: Success
 */
esp_err_t mp45dt02_take_samples(void *dest, size_t size, size_t *bytes_read, TickType_t ticks_to_wait);

#endif	/* MP45DT02_H__ */
