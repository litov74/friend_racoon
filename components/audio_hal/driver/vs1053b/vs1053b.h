/**
 * *****************************************************************************
 * @file		vs1053b.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		VS1053b audio decoder driver
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef VS1053B_H__
#define VS1053B_H__

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <esp_system.h>
#include <driver/spi_master.h>

/* Export constants ----------------------------------------------------------*/

#define VS1053B_VOL_THRESHOLD	40.0f
#define VS1053B_VOL_RANGE		(100.0f - VS1053B_VOL_THRESHOLD)

#define VS1053B_CHUNK_SIZE_MAX	32

/**
 * @defgroup	vs_regs VS1053b control bytes
 * @{
 */

/* VS1053b instructions */
#define VS1053B_OPCODE_READ			0x03
#define VS1053B_OPCODE_WRITE		0x02
/* VS1053b SCI registers */
#define VS1053B_SCI_MODE			0x00
#define VS1053B_SCI_STATUS			0x01
#define VS1053B_SCI_BASS			0x02
#define VS1053B_SCI_CLOCKF			0x03
#define VS1053B_SCI_DECODE_TIME		0x04
#define VS1053B_SCI_AUDATA			0x05
#define VS1053B_SCI_WRAM 			0x06
#define VS1053B_SCI_WRAMADDR		0x07
#define VS1053B_SCI_HDAT0			0x08
#define VS1053B_SCI_HDAT1			0x09
#define VS1053B_SCI_AIADDR			0x0A
#define VS1053B_SCI_VOL				0x0B
#define VS1053B_SCI_AICTRL0			0x0C
#define VS1053B_SCI_AICTRL1			0x0D
#define VS1053B_SCI_AICTRL2			0x0E
#define VS1053B_SCI_AICTRL3			0x0F
/* VS1053b SCI mode */
#define VS1053B_SM_DIFF				0x01
#define VS1053B_SM_LAYER12			0x02
#define VS1053B_SM_RESET			0x04
#define VS1053B_SM_CANCEL			0x08
#define VS1053B_SM_EARSPEAKER_LO	0x10
#define VS1053B_SM_TESTS			0x20
#define VS1053B_SM_STREAM			0x40
#define VS1053B_SM_EARSPEAKER_HI	0x80
#define VS1053B_SM_DACT				0x100
#define VS1053B_SM_SDIORD			0x200
#define VS1053B_SM_SDISHARE			0x400
#define VS1053B_SM_SDINEW			0x800
#define VS1053B_SM_ADPCM			0x1000
#define VS1053B_SM_LINE1			0x4000
#define VS1053B_SM_CLK_RANGE		0x8000

/** @}*/

/* Export functions ----------------------------------------------------------*/

/**
 * @brief	Initialize VS1053b codec chip
 * @param	None
 * @return
 * 			- ESP_FAIL: Unexpected error
 * 			- ESP_OK: Success
 */
esp_err_t vs1053b_init(void);

/**
 * @brief	Start VS1053b codec chip
 * @param	None
 * @return
 * 			- ESP_FAIL: Unexpected error
 * 			- ESP_OK: Success
 */
esp_err_t vs1053b_start(void);

/**
 * @brief	Configure VS1053b codec mode and SPI interface
 * @param	None
 * @return
 * 			- ESP_ERR_INVALID_ARG: Configuration is invalid
 * 			- ESP_ERR_INVALID_STATE: Host already is in use
 * 			- ESP_ERR_NO_MEM: Out of memory
 * 			- ESP_OK: Success
 */
esp_err_t vs1053b_config_spi(void);

/**
 * @brief		Read VS1053b register via serial command interface
 * @param[in]	reg_addr	Address of register
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter is invalid
 * 				- ESP_OK: Success
 */
uint16_t vs1053b_sci_read_reg(uint8_t reg_addr);

/**
 * @brief		Write VS1053b register via serial command interface
 * @param[in]	reg_addr	Address of the register
 * @param[in]	data_hi		MSB
 * @param[in]	data_lo		LSB
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter is invalid
 * 				- ESP_OK: Success
 */
esp_err_t vs1053b_sci_write_reg(uint8_t reg_addr, uint8_t data_hi, uint8_t data_lo);

/**
 * @brief		Send chunk of data to the VS1053b
 * @param[in]	data	Pointer to data buffer
 * @param[in]	len		Length of data to play in bytes
 * @return
 * 				- None
 */
void vs1053b_play_chunk(uint8_t *data, size_t len);

/**
 * @brief	Get number of kilobits that are conveyed or processed per second
 * @param	None
 * @return
 *			- Bit rate
 */
uint16_t vs1053b_get_bitrate(void);

/**
 * @brief	Reset VS1053b codec by the hardware
 * @param 	None
 * @return
 *			- None
 */
void vs1053b_hard_reset(void);

/**
 * @brief	Reset VS1053b codec by the software
 * @param	None
 * @return
 * 			- None
 */
void vs1053b_soft_reset(void);

/**
 * @brief		Set the attenuation from the maximum volume level in 0.5dB steps
 * @param[in]	level	The channel sound level
 * @return
 * 				- None
 */
void vs1053b_set_volume(float level);

#endif	/* VS1053B_H__ */
