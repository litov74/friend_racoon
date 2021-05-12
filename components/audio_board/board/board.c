/**
 * *****************************************************************************
 * @file		board.c
 * @author		S. Naumov
 * *****************************************************************************
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <esp_err.h>
#include <esp_log.h>

/* User files */
#include "board.h"
#include "iot_button.h"
#include "mp45dt02.h"
#include "vs1053b.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "audio_brd";

#define BUTTON_IO_NUM		PIN_NUM_USER_BUTTON
#define BUTTON_ACTIVE_LEVEL	1

/* External functions --------------------------------------------------------*/

extern void app_clear_device_connection_data(void);

/* Private functions ---------------------------------------------------------*/

static void button_tap_cb(void *arg) {
	char *str = (char *)arg;
	ESP_EARLY_LOGD(	tag,
					"Tap callback: %s, heap: %d",
					str,
					esp_get_free_heap_size());
	ESP_LOGW(tag, "Reset device settings initiated by the user");
	app_clear_device_connection_data();
}

static void board_button_init(void) {
	button_handle_t btn_hdl = iot_button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
	if (btn_hdl) {
		iot_button_set_evt_cb(	btn_hdl,
								BUTTON_CB_RELEASE,
								button_tap_cb,
								"release");
	} else {
		ESP_LOGW(tag, "Unable to create button object");
	}
}

/* Export functions ----------------------------------------------------------*/

/* Peripherals HAL initialization */
void board_init(void) {
	board_button_init();
	vs1053b_init();
	vs1053b_start();
	mp45dt02_init();
	mp45dt02_start();
	gpio_config_t io_conf;
	io_conf.pin_bit_mask = BIT64(PIN_NUM_USER_LED);
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	gpio_config(&io_conf);
	gpio_set_level(PIN_NUM_USER_LED, 0);
}
