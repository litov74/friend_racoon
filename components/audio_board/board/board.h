/**
 * *****************************************************************************
 * @file		board.h
 * @author		S. Naumov
 * *****************************************************************************
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef AUDIO_BOARD_H__
#define AUDIO_BOARD_H__

/* Includes ------------------------------------------------------------------*/

/* User files */
#include "board_def.h"
#include "board_pins_config.h"

/* Export functions ----------------------------------------------------------*/

/**
 * @brief	Peripherals HAL initialization
 * @param	None
 * @return
 * 			- None
 */
void board_init(void);

#endif	/* AUDIO_BOARD_H__ */
