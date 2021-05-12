/**
 * *****************************************************************************
 * @file		board_pins_config.h
 * @author		S. Naumov
 * *****************************************************************************
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef BOARD_PINS_CFG_H__
#define BOARD_PINS_CFG_H__

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <esp_err.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <driver/spi_slave.h>

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

/* Export functions ----------------------------------------------------------*/

/**
 * @brief		Get SPI pins configuration
 * @param[out]	spi_cfg	SPI bus configuration parameters
 * @return
 * 				- ESP_OK: Success
 */
esp_err_t get_spi_pins(spi_bus_config_t *spi_cfg);

#ifdef __cplusplus
extern "C" }
#endif	/* __cplusplus */

#endif	/* BOARD_PINS_CFG_H__ */
