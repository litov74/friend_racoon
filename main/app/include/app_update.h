/**
 * *****************************************************************************
 * @file		app_update.h
 * *****************************************************************************
 * @brief		Wireless firmware update
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef APP_UPDATE_H__
#define APP_UPDATE_H__

/* Includes ------------------------------------------------------------------*/

/* User files */
#include "app.h"

/* Export functions prototypes -----------------------------------------------*/

/**
 * @brief	Requests the current firmware version and download link for it
 * @param	None
 * @return
 * 			- None
 */
void app_update_get_and_check_version();

#endif	/* APP_UPDATE_H__ */
