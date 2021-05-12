/**
 * *****************************************************************************
 * @file		board_pins_config.c
 * @author		S. Naumov
 * *****************************************************************************
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <string.h>

/* Framework */
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>

/* User files */
#include "board.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "audio_brd";

/* Export functions ----------------------------------------------------------*/

/* Get SPI pins configuration */
esp_err_t get_spi_pins(spi_bus_config_t *spi_cfg) {
	spi_cfg->mosi_io_num = PIN_NUM_VS1053B_SI;
	spi_cfg->miso_io_num = PIN_NUM_VS1053B_SO;
	spi_cfg->sclk_io_num = PIN_NUM_VS1053B_SCLK;
	spi_cfg->quadwp_io_num = -1;
	spi_cfg->quadhd_io_num = -1;
	ESP_LOGI(tag, "'SPI Master Mode' pin configuration");
	return ESP_OK;
}
