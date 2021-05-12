/**
 * *****************************************************************************
 * @file		app_server.h
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Application's server node
 *
 * *****************************************************************************
 */

/* Define to prevent recursive inclusion */
#ifndef APP_SERVER_H__
#define APP_SERVER_H__

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <esp_http_server.h>

/* Export functions ----------------------------------------------------------*/

/**
 * @brief		Starts the web server
 * @param[in]	arg	Pointer to user context data
 * @return
 * 				- Handle to HTTPD server instance
 */
httpd_handle_t app_server_start(void *arg);

/**
 * @brief		Stops the web server
 * @param[in]	server	Handle to HTTPD server instance
 * @return
 * 				- None
 */
void app_server_stop(httpd_handle_t server);

#endif	/* APP_SERVER_H__ */
