/**
 * *****************************************************************************
 * @file		main.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		The main program body
 *
 * *****************************************************************************
 */

//#define CERTIFICATION_TASK

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdio.h>
#include <string.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_spi_flash.h>
#include <nvs_flash.h>
#include <driver/gpio.h>

/* User files */
#include "sound_player.h"
#include "sound_recorder.h"
#include "app.h"
#include "board.h"
#include "mp45dt02.h"
#include "vs1053b.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_main";

/* Export variables ----------------------------------------------------------*/

/* The main application structure instance definition */
app_network_conn_t app_instance = { 0 };

#ifdef CERTIFICATION_TASK

extern const uint8_t intro_mp3_start[] asm("_binary_intro_mp3_start");
extern const uint8_t intro_mp3_end[] asm("_binary_intro_mp3_end");
uint8_t buf[VS1053B_CHUNK_SIZE_MAX];

void cert_task(void *arg) {
	int read_size = intro_mp3_end - intro_mp3_start;
	int mp3_pos = 0;
	gpio_set_level(PIN_NUM_USER_LED, 1);
	for (;;) {
		if (read_size <= 0) {
			read_size = intro_mp3_end - intro_mp3_start;
			mp3_pos = 0;
		} else {
			read_size = intro_mp3_end - intro_mp3_start - mp3_pos;
			if (read_size < VS1053B_CHUNK_SIZE_MAX) {
				memcpy(buf, intro_mp3_start + mp3_pos, read_size);
				vs1053b_play_chunk(buf, read_size);
				mp3_pos += read_size;
			} else {
				memcpy(buf, intro_mp3_start + mp3_pos, VS1053B_CHUNK_SIZE_MAX);
				vs1053b_play_chunk(buf, VS1053B_CHUNK_SIZE_MAX);
				mp3_pos += VS1053B_CHUNK_SIZE_MAX;
			}
		}
	}
	gpio_set_level(PIN_NUM_USER_LED, 0);
	vTaskDelete(NULL);
}

#endif	/* CERTIFICATION_TASK */

/**
 * @brief	The main program body
 * @param	None
 * @return
 * 			- None
 */
void app_main(void) {
	esp_err_t ret;
	/* Initialize non-volatile storage */
	ret = nvs_flash_init();
	if (	ret == ESP_ERR_NVS_NO_FREE_PAGES ||
			ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK( nvs_flash_erase() );
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK( ret );
	/* Set the MAC address up */
	uint8_t *mac_buf = (uint8_t *)calloc(DEVICE_MAC_ADDRESS_LENGTH, sizeof(uint8_t));
	ESP_ERROR_CHECK( esp_efuse_mac_get_default(mac_buf) );
	ESP_ERROR_CHECK( esp_base_mac_addr_set(mac_buf) );
	free(mac_buf);
	/* Check external SPI RAM size */
	app_himem_get_size_info();
	/* Initialize the flash device */
	spi_flash_init();
	size_t size = spi_flash_get_chip_size();
	ESP_LOGI(tag, "Flash chip size = %d", size);
	board_init();
#ifndef CERTIFICATION_TASK
	app_init(&app_instance);
#else
	xTaskCreatePinnedToCore(&cert_task,
							"cert_task",
							32768,
							NULL,
							23,
							NULL,
							1);
#endif	/* CERTIFICATION_TASK */
	for (;;) {
		vTaskDelay(1);
	}
}
