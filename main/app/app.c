/**
 * *****************************************************************************
 * @file		app.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Application's common module
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <esp_err.h>
#include <esp_himem.h>
#include <esp_log.h>

/* User files */
#include "app.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_common";

static const char *http_device_register_req_url =
		"anonymous/registerDevice";
//		"http://enot.tripfinance.ru/teddyserver-rest/webapis/0.1/anonymous/registerDevice";
static const char *http_device_login_req_url =
		"device/login?version=";
//		"http://enot.tripfinance.ru/teddyserver-rest/webapis/0.1/device/login?version=";
static const char *http_device_profile_req_url =
		"device/profile";
//		"http://enot.tripfinance.ru/teddyserver-rest/webapis/0.1/device/profile";
static const char *http_device_radio_req_url =
		"device/radio";
//		"http://enot.tripfinance.ru/teddyserver-rest/webapis/0.1/device/radio";
static const char *http_device_sound_req_url =
		"device/sound?id=";
//		"http://enot.tripfinance.ru/teddyserver-rest/webapis/0.1/device/sound?id=";

/* Private variables ---------------------------------------------------------*/

static app_wifi_initializer_t app_init_conditions;

/* Private functions ---------------------------------------------------------*/

static void oneshot_timer_callback(void *arg) {
	app_network_conn_t *ctx = (app_network_conn_t *)arg;
	xEventGroupSetBits(ctx->event_group, BIT_CONN_CORRUPTED);
}

/* Export functions ----------------------------------------------------------*/

/* Access the shared resource */
esp_err_t app_semaphore_take(SemaphoreHandle_t semphr, TickType_t block_time) {
	if (semphr) {
		if (xSemaphoreTake(semphr, block_time)) {
			return ESP_OK;
		}
	}
	return ESP_FAIL;
}

/* Free the semaphore */
void app_semaphore_give(SemaphoreHandle_t semphr) {
	if (semphr) {
		xSemaphoreGive(semphr);
	}
}

void app_uri_init(app_network_conn_t *arg)
{
	// структура инициализируется нулями.
	memset(&arg->uri, 0, sizeof arg->uri);

	// Коипруем и складываем строки
	strlcpy(arg->uri.regdev, arg->device.server_url, sizeof arg->uri.regdev);
	strlcat(arg->uri.regdev, (const char *)http_device_register_req_url, sizeof arg->uri.regdev);
	
	strlcpy(arg->uri.login, arg->device.server_url, sizeof arg->uri.login);
	strlcat(arg->uri.login, (const char *)http_device_login_req_url, sizeof arg->uri.login);
	
	strlcpy(arg->uri.player, arg->device.server_url, sizeof arg->uri.player);
	strlcat(arg->uri.player, (const char *)http_device_sound_req_url, sizeof arg->uri.player);
	
	strlcpy(arg->uri.profile, arg->device.server_url, sizeof arg->uri.profile);
	strlcat(arg->uri.profile, (const char *)http_device_profile_req_url, sizeof arg->uri.profile);
	
	strlcpy(arg->uri.sampler, arg->device.server_url, sizeof arg->uri.sampler);
	strlcat(arg->uri.sampler, (const char *)http_device_radio_req_url, sizeof arg->uri.sampler);
}

/* Application initialization phase */
esp_err_t app_init(app_network_conn_t *arg) {
	arg->event_group = xEventGroupCreate();
	xEventGroupSetBits(arg->event_group, BIT_RECONNECT);
	/* Attempt to create the MUTEX */
	arg->spi_flash_mtx = xSemaphoreCreateMutex();
	xSemaphoreGive(arg->spi_flash_mtx);
	app_spiffs_init();
	esp_err_t ret = app_devdesc_init(&arg->device);
	if (ret != ESP_OK) {
		if (ret != ESP_ERR_NOT_FOUND) {
			ESP_LOGD(tag, "Failed to initialize device descriptor");
			return ESP_FAIL;
		}
	}
	app_init_conditions.init_state = ret;
	app_init_conditions.app_ptr = arg;
	/* Attempt to create the binary semaphore */
	arg->client.semphr = xSemaphoreCreateBinary();
	xSemaphoreGive(arg->client.semphr);
	/* Attempt to create the ring buffer */
	arg->rbuf_hdl = xRingbufferCreate(COMMON_RING_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
	/* Media functionalities initialization */
	sound_player_init(&arg->client.player);
	sound_recorder_init(&arg->client.sampler);
	arg->client.led_tracker = pdFALSE;

	/* Set of URIs */
	app_uri_init(arg);

	/* Create the application timer */
	const esp_timer_create_args_t oneshot_timer_args = {
			.callback = &oneshot_timer_callback,
			.arg = arg,
			.name = "one-shot",
	};
	esp_timer_create(&oneshot_timer_args, &arg->tim);
	xTaskCreatePinnedToCore(&app_wifi_init_task,
							"wifi_init",
							12228,
							&app_init_conditions,
							4,
							NULL,
							0);
	return ESP_OK;
}

/* Get total amount and free amount of memory under control of HIMEM API */
void app_himem_get_size_info(void) {
	size_t mem_cnt = esp_himem_get_phys_size();
	size_t mem_free = esp_himem_get_free_size();
	ESP_LOGD(	tag,
				"HIMEM has %dKiB of memory, %dKiB of which is free",
				(int)(mem_cnt / 1024),
				(int)(mem_free / 1024));
	ESP_LOGD(	tag,
				"ESP_HIMEM_BLKSZ = %d",
				ESP_HIMEM_BLKSZ);
}

/* Restarts the device */
void app_restart_device(void) {
	app_wifi_sta_detach(&app_instance);
	esp_wifi_stop();
	esp_wifi_deinit();
	esp_restart();
}

/* Delete connection settings from device memory */
void app_clear_device_connection_data(void) {
	app_wifi_sta_detach(&app_instance);
	esp_wifi_stop();
	esp_wifi_deinit();
	while (app_spiffs_erase_file(wifi_ap_recs_path) != ESP_OK);
	while (app_devdesc_clear_device_descriptor_data() != ESP_OK);
	esp_restart();
}
