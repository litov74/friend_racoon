/**
 * *****************************************************************************
 * @file		mp45dt02.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		MP45DT02 digital MEMS microphone driver
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdbool.h>
#include <stddef.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2s.h>
#include <driver/gpio.h>

/* User files */
#include "board_def.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "mp45dt02";

static const int i2s_num = 0;		/*!< I2S port number */
#define I2S_SAMPLE_RATE		16000	/*!< I2S sample rate */
#define I2S_DMA_BUF_COUNT	4		/*!< I2S DMA buffer count */
#define I2S_DMA_BUF_LEN		1024	/*!< I2S DMA buffer length */

/* Private structures --------------------------------------------------------*/

static const i2s_config_t i2s_iface_cfg = {
		.mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM,
		.sample_rate = I2S_SAMPLE_RATE,
		.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
		.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
		.communication_format = I2S_COMM_FORMAT_PCM,
		.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
		.dma_buf_count = I2S_DMA_BUF_COUNT,
		.dma_buf_len = I2S_DMA_BUF_LEN,
		.use_apll = false,
};

static const i2s_pin_config_t i2s_pin_cfg = {
		.ws_io_num = PIN_NUM_MP45DT02_CLK,
		.data_in_num = PIN_NUM_MP45DT02_DOUT,
};

/* Export functions ----------------------------------------------------------*/

/* Initialize MP45DT02 digital MEMS microphone */
esp_err_t mp45dt02_init(void) {
	esp_err_t ret = ESP_OK;
	float clk;
	/* Install and start I2S driver */
	ret |= !i2s_driver_install(i2s_num, &i2s_iface_cfg, 0, NULL) ? ESP_OK : ESP_FAIL;
	/* Get clock set on particular port number */
	clk = i2s_get_clk(i2s_num);
	ESP_LOGI(tag, "'mp45dt02_start' finished. Bit clock rate = %.6f", clk);
	return (esp_err_t)ret;
}

/* Start MP45DT02 digital MEMS microphone */
esp_err_t mp45dt02_start(void) {
	esp_err_t ret = ESP_OK;
	/* Set I2S pin numbers */
	ret |= i2s_set_pin(i2s_num, &i2s_pin_cfg);
	/* Initialize the GPIO pins */
	gpio_config_t io_conf;
	/* Left or right channel selection */
	io_conf.pin_bit_mask = BIT64(PIN_NUM_MP45DT02_LR);
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ret |= gpio_config(&io_conf);
	ret |= gpio_set_level(PIN_NUM_MP45DT02_LR, 0);
	ESP_LOGI(tag, "'mp45dt02_init' finished");
	if (ret != ESP_OK) ret = ESP_FAIL;
	return (esp_err_t)ret;
}

/* Read audio samples from the I2S microphone module */
esp_err_t mp45dt02_take_samples(void *dest, size_t size, size_t *bytes_read, TickType_t ticks_to_wait) {
	return (esp_err_t)i2s_read(i2s_num, dest, size, bytes_read, ticks_to_wait);
}
