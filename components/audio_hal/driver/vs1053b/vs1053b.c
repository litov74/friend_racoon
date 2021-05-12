/**
 * *****************************************************************************
 * @file		vs1053b.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		VS1053b audio decoder driver
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <string.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>

/* User files */
#include "board_def.h"
#include "board_pins_config.h"
#include "vs1053b.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "vs1053b";

/* Private variables ---------------------------------------------------------*/

static spi_bus_config_t codec_spi_cfg;
static spi_device_handle_t codec_sci;
static spi_device_handle_t codec_sdi;

static SemaphoreHandle_t sci_semphr;
static SemaphoreHandle_t sdi_semphr;

static float vs1053b_vol_lookup[0xFF];

static spi_device_interface_config_t codec_sci_iface = {
		.command_bits = 8,
		.address_bits = 8,
		.dummy_bits = 0,
		.mode = 0,
		.duty_cycle_pos = 0,
		.cs_ena_pretrans = 0,
		.cs_ena_posttrans = 1,
		.spics_io_num = PIN_NUM_VS1053B_XCS,
		.flags = 0,
		.queue_size = 1,
		.pre_cb = NULL,
		.post_cb = NULL,
};

static spi_device_interface_config_t codec_sdi_iface = {
		.command_bits = 0,
		.address_bits = 0,
		.dummy_bits = 0,
		.mode = 0,
		.duty_cycle_pos = 0,
		.cs_ena_pretrans = 0,
		.cs_ena_posttrans = 1,
		.spics_io_num = PIN_NUM_VS1053B_XDCS,
		.flags = 0,
		.queue_size = 1,
		.pre_cb = NULL,
		.post_cb = NULL,
};

/* Private functions prototypes ----------------------------------------------*/

/**
 * @brief		Write audio data using serial data interface
 * @param[in]	spi		Device handle obtained using spi_host_add_dev
 * @param[in]	data	Pointer to data buffer
 * @param[in]	len		Length of data to write in bytes
 * @return
 * 				- ESP_ERR_INVALID_ARG: Parameter is invalid
 * 				- ESP_OK: Success
 */
static esp_err_t vs1053b_sdi_send_audio(spi_device_handle_t spi, uint8_t *data, size_t len);

/**
 * @brief	Switch device to the MP3 decoder mode
 * @param	None
 * @return
 * 			- None
 */
static void vs1053b_switch_to_mp3_mode(void);

/**
 * @brief	Wait for VS1053b’s 2048-byte FIFO is capable of receiving data
 * @param	None
 * @return
 *			- None
 */
static inline void vs1053b_await_data_req(void);

/* Private functions ---------------------------------------------------------*/

/**
 * Send data to VS1053b. Uses spi_device_polling_transmit, which waits until
 * the transfer is complete. Since data transactions are usually small, they are handled in
 * polling mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete
 */
static esp_err_t vs1053b_sdi_send_audio(spi_device_handle_t spi, uint8_t *data, size_t len) {
	esp_err_t ret = ESP_OK;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = len * 8;
	t.tx_buffer = data;
	vs1053b_await_data_req();
	xSemaphoreTake(sdi_semphr, portMAX_DELAY);
	ret = spi_device_transmit(spi, &t);
	xSemaphoreGive(sdi_semphr);
	while (!gpio_get_level(PIN_NUM_VS1053B_DREQ));
	return (esp_err_t)ret;
}

/* Switch device to the MP3 decoder mode */
static void vs1053b_switch_to_mp3_mode(void) {
	vs1053b_sci_write_reg(VS1053B_SCI_WRAMADDR, 0xC0, 0x17);
	vs1053b_sci_write_reg(VS1053B_SCI_WRAM, 0x00, 0x03);
	vs1053b_sci_write_reg(VS1053B_SCI_WRAMADDR, 0xC0, 0x19);
	vs1053b_sci_write_reg(VS1053B_SCI_WRAM, 0x00, 0x00);
	vTaskDelay(pdMS_TO_TICKS(150));
	vs1053b_hard_reset();
}

/* Wait for VS1053b’s 2048-byte FIFO is capable of receiving data */
static inline void vs1053b_await_data_req(void) {
	while (!gpio_get_level(PIN_NUM_VS1053B_DREQ)) {
		vTaskDelay(1);
	}
}

/* Export functions ----------------------------------------------------------*/

/* Initialize VS1053b codec chip */
esp_err_t vs1053b_init(void) {
	esp_err_t ret = ESP_OK;
	int clock_freq = 0;
	ret |= vs1053b_config_spi();
	/* Attach the VS1053b's chip serial command interface to the SPI bus */
	clock_freq = spi_cal_clock(APB_CLK_FREQ, 1400000, 128, NULL);
	codec_sci_iface.clock_speed_hz = clock_freq;
	ESP_ERROR_CHECK( spi_bus_add_device(HSPI_HOST, &codec_sci_iface, &codec_sci) );
	/* Attach the VS1053b's chip serial data interface to the SPI bus */
	clock_freq = spi_cal_clock(APB_CLK_FREQ, 6100000, 128, NULL);
	codec_sdi_iface.clock_speed_hz = clock_freq;
	ESP_ERROR_CHECK( spi_bus_add_device(HSPI_HOST, &codec_sdi_iface, &codec_sdi) );
	/* Initialize VS1053b related the GPIO pins */
	gpio_config_t io_conf;
	/* Active low asynchronous reset, Schmitt-Trigger input */
	io_conf.pin_bit_mask = GPIO_VS1053B_XRESET_PIN_SEL;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK( gpio_config(&io_conf) );
	gpio_set_level(PIN_NUM_VS1053B_XRESET, 0);
	vTaskDelay(pdMS_TO_TICKS(50));
	/* 'Mute' mode control output */
	io_conf.pin_bit_mask = GPIO_VS1053B_XMUTE_PIN_SEL;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK( gpio_config(&io_conf) );
	gpio_set_level(PIN_NUM_VS1053B_XMUTE, 0);
	/* Mute control input (active low) */
	io_conf.pin_bit_mask = GPIO_AMP_XMUTE_PIN_SEL;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK( gpio_config(&io_conf) );
	gpio_set_level(PIN_NUM_AMP_XMUTE, 1);
	/* Shutdown control input (active low)*/
	io_conf.pin_bit_mask = GPIO_AMP_XSHDN_PIN_SEL;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK( gpio_config(&io_conf) );
	gpio_set_level(PIN_NUM_AMP_XSHDN, 1);
	/* Data request, input bus */
	io_conf.pin_bit_mask = GPIO_VS1053B_DREQ_PIN_SEL;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK( gpio_config(&io_conf) );
	/* Fill the lookup table */
	int init_val = (int)((VS1053B_VOL_RANGE * 255.0) / 100.0);
	for (int i = 0; i <= init_val; ++i) {
		vs1053b_vol_lookup[i] = (float)(init_val - i);
	}
	ESP_LOGI(tag, "'vs1053b_init' finished");
	if (ret != ESP_OK) {
		ret = ESP_FAIL;
	}
	return (esp_err_t)ret;
}

/* Start VS1053b codec chip */
esp_err_t vs1053b_start(void) {
	esp_err_t ret = ESP_OK;
	int status = 0;
	gpio_set_level(PIN_NUM_VS1053B_XRESET, 1);
	vTaskDelay(pdMS_TO_TICKS(500));
	gpio_set_level(PIN_NUM_VS1053B_XMUTE, 1);
	vTaskDelay(pdMS_TO_TICKS(50));
	if (!gpio_get_level(PIN_NUM_VS1053B_DREQ)) {
		ESP_LOGE(tag, "VS1053b audio decoder driver is unavailable");
		return ESP_FAIL;
	}
	vs1053b_switch_to_mp3_mode();
	status = vs1053b_sci_read_reg(VS1053B_SCI_STATUS);
	status = (status >> 4) & 0x0F;
	ret = vs1053b_sci_write_reg(VS1053B_SCI_CLOCKF, 0xB8, 0x00);
	vs1053b_soft_reset();
	vs1053b_await_data_req();
	vs1053b_set_volume(100.0);
	vTaskDelay(pdMS_TO_TICKS(50));
	ESP_LOGI(tag, "'vs1053b_start' finished. The default mode = %x", status);
	if (ret != ESP_OK) {
		ret = ESP_FAIL;
	}
	return (esp_err_t)ret;
}

/* Configure VS1053b codec mode and SPI interface */
esp_err_t vs1053b_config_spi(void) {
	esp_err_t ret = ESP_OK;
	/* Attempt to create the SCI data bus binary semaphore */
	if (!sci_semphr) {
		if ((sci_semphr = xSemaphoreCreateBinary()) != NULL) {
			ESP_LOGI(tag, "The SCI data bus binary semaphore was created successfully");
		} else {
			ESP_LOGW(tag, "The memory required to hold the SCI data bus binary semaphore could not be allocated");
			while (!sci_semphr) {
				vTaskDelay(1);
				if ((sci_semphr = xSemaphoreCreateBinary()) != NULL) {
					ESP_LOGI(tag, "The SCI data bus binary semaphore was created successfully");
				}
			}
		}
	}
	xSemaphoreGive(sci_semphr);
	/* Attempt to create the SDI data bus binary semaphore */
	if (!sdi_semphr) {
		if ((sdi_semphr = xSemaphoreCreateBinary()) != NULL) {
			ESP_LOGI(tag, "The SDI data bus binary semaphore was created successfully");
		} else {
			ESP_LOGW(tag, "The memory required to hold the SDI data bus binary semaphore could not be allocated");
			while (!sdi_semphr) {
				vTaskDelay(1);
				if ((sdi_semphr = xSemaphoreCreateBinary()) != NULL) {
					ESP_LOGI(tag, "The SDI data bus binary semaphore was created successfully");
				}
			}
		}
	}
	xSemaphoreGive(sdi_semphr);
	get_spi_pins(&codec_spi_cfg);
	codec_spi_cfg.flags = SPICOMMON_BUSFLAG_MASTER;
	ret = spi_bus_initialize(HSPI_HOST, &codec_spi_cfg, 1);
	return (esp_err_t)ret;
}

/* Read VS1053b register via serial command interface */
uint16_t vs1053b_sci_read_reg(uint8_t reg_addr) {
	spi_transaction_t t;
	uint16_t res = 0;
	memset(&t, 0, sizeof(t));
	t.flags = SPI_TRANS_USE_RXDATA;
	t.cmd = VS1053B_OPCODE_READ;
	t.addr = reg_addr;
	t.length = 2 * 8;
	vs1053b_await_data_req();
	xSemaphoreTake(sci_semphr, portMAX_DELAY);
	spi_device_transmit(codec_sci, &t);
	res = ((t.rx_data[0] & 0xFF) << 8) | (t.rx_data[1] & 0xFF);
	xSemaphoreGive(sci_semphr);
	while (!gpio_get_level(PIN_NUM_VS1053B_DREQ));
	return (esp_err_t)res;
}

/* Write VS1053b register via serial command interface */
esp_err_t vs1053b_sci_write_reg(uint8_t reg_addr, uint8_t data_hi, uint8_t data_lo) {
	esp_err_t ret = ESP_OK;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.flags |= SPI_TRANS_USE_TXDATA;
	t.cmd = VS1053B_OPCODE_WRITE;
	t.addr = reg_addr;
	t.length = 2 * 8;
	t.tx_data[0] = data_hi;
	t.tx_data[1] = data_lo;
	vs1053b_await_data_req();
	xSemaphoreTake(sci_semphr, portMAX_DELAY);
	ret = spi_device_transmit(codec_sci, &t);
	xSemaphoreGive(sci_semphr);
	while (!gpio_get_level(PIN_NUM_VS1053B_DREQ));
	return (esp_err_t)ret;
}

/* Send chunk of data to the VS1053b */
void vs1053b_play_chunk(uint8_t *data, size_t len) {
	int chunk_len;
	while (len) {
		vs1053b_await_data_req();
		chunk_len = len;
		if (len > VS1053B_CHUNK_SIZE_MAX) {
			chunk_len = VS1053B_CHUNK_SIZE_MAX;
		}
		len -= chunk_len;
		vs1053b_sdi_send_audio(codec_sdi, data, chunk_len);
		data += chunk_len;
	}
}

/* Get number of kilobits that are conveyed or processed per second */
uint16_t vs1053b_get_bitrate(void) {
	uint16_t res = 0;
	uint16_t bitrate = (vs1053b_sci_read_reg(VS1053B_SCI_HDAT0) & 0xF000) >> 12;
	uint8_t id = (vs1053b_sci_read_reg(VS1053B_SCI_HDAT1) & 0x18) >> 3;
	if (id == 3) {
		res = 32;
		while (bitrate > 13) {
			res += 64;
			bitrate--;
		}
		while (bitrate > 9) {
			res += 32;
			bitrate--;
		}
		while (bitrate > 5) {
			res += 16;
			bitrate--;
		}
		while (bitrate > 1) {
			res += 8;
			bitrate--;
		}
	} else {
		res = 8;
		while (bitrate > 8) {
			res += 16;
			bitrate--;
		}
		while (bitrate > 1) {
			res += 8;
			bitrate--;
		}
	}
	return (uint16_t)res;
}

/* Reset VS1053b codec by the hardware */
void vs1053b_hard_reset(void) {
	gpio_set_level(PIN_NUM_VS1053B_XRESET, 0);
	vTaskDelay(pdMS_TO_TICKS(20));
	gpio_set_level(PIN_NUM_VS1053B_XRESET, 1);
	vTaskDelay(pdMS_TO_TICKS(20));
	if (gpio_get_level(PIN_NUM_VS1053B_DREQ)) {
		return;
	}
	vTaskDelay(pdMS_TO_TICKS(20));
}

/* Reset VS1053b codec by the software */
void vs1053b_soft_reset(void) {
	vs1053b_sci_write_reg(VS1053B_SCI_MODE, ((VS1053B_SM_SDINEW | VS1053B_SM_LINE1) >> 8), VS1053B_SM_RESET);
	vs1053b_sci_write_reg(VS1053B_SCI_MODE, ((VS1053B_SM_SDINEW | VS1053B_SM_LINE1) >> 8), VS1053B_SM_LAYER12);
}

/* Set the attenuation from the maximum volume level in 0.5dB steps */
void vs1053b_set_volume(float level) {
	float level_scl = level * VS1053B_VOL_RANGE / 100.0 + VS1053B_VOL_THRESHOLD;
	float init_val = (100.0 - level_scl) * 255.0 / 100.0;
	/* Find out the final value */
	float nearest_diff_left = 0.0, nearest_diff_right = 0.0;
	int result = 0;
	if ((int)init_val == (int)vs1053b_vol_lookup[0]) {
		result = (int)vs1053b_vol_lookup[0];
	} else if (!init_val) {
		result = 0;
	} else {
		for (int i = 0; i < sizeof vs1053b_vol_lookup / sizeof(float); ++i) {
			if (vs1053b_vol_lookup[i] >= init_val && init_val >= vs1053b_vol_lookup[i + 1]) {
				nearest_diff_left = vs1053b_vol_lookup[i] - init_val;
				nearest_diff_right = init_val - vs1053b_vol_lookup[i + 1];
				if (nearest_diff_left < nearest_diff_right) {
					result = (int)vs1053b_vol_lookup[i];
				} else {
					result = (int)vs1053b_vol_lookup[i + 1];
				}
				break;
			}
		}
	}
	if (result == 0xFF) {
		result = 0xFE;
	}
	vs1053b_sci_write_reg(VS1053B_SCI_VOL, result, result);
	if (level_scl <= VS1053B_VOL_THRESHOLD) {
		gpio_set_level(PIN_NUM_VS1053B_XMUTE, 0);
	} else {
		gpio_set_level(PIN_NUM_VS1053B_XMUTE, 1);
	}
}
