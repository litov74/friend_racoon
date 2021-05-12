/**
 * *****************************************************************************
 * @file		app_update.c
 * *****************************************************************************
 * @brief		Wireless firmware update
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <string.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_http_client.h>
#include <esp_image_format.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <cJSON.h>

/* User files */
#include "app_update.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_update";

#define BUF_LEN					1024
#define MAX_JSON_BUF			2048
#define MAX_ATTEMPTS_ALLOC_BUF	10

/* Private typedef -----------------------------------------------------------*/

typedef enum {
	UPGRADE_STATE_IDLE = 0,
	UPGRADE_STATE_STARTED,
	UPGRADE_STATE_WORK,
	UPGRADE_STATE_STOPPED
} upgrade_state_e;

typedef struct {
	upgrade_state_e state;
	char version[MAX_FIRMWARE_UPGRADE_VERSION_LENGTH + 1];
	char url[MAX_FIRMWARE_UPGRADE_URL_LENGTH + 1];
} upgrade_struct_t;

static upgrade_struct_t upgrade;

/* Private functions prototypes ----------------------------------------------*/

static void _check_version_and_update(app_network_conn_t *app, char *ver, char *url);
static void _get_update_info(char *info_url);
static void http_device_update_task(void *arg);

/* Private functions ---------------------------------------------------------*/

static void __attribute__((noreturn)) task_exit(const upgrade_state_e state) {
	ESP_LOGI(tag, "'http_device_update_task' finished");
	upgrade.state = state;
	(void)vTaskDelete(NULL);
	while (1) {;}
}

static void task_fatal_error(const upgrade_state_e state) {
	ESP_LOGE(tag, "Exiting task due to fatal error");
	task_exit(state);
}

void _check_version_and_update(app_network_conn_t *app, char *ver, char *url) {
	if (upgrade.state != UPGRADE_STATE_IDLE) {
		if (upgrade.state == UPGRADE_STATE_STOPPED) {
			ESP_LOGW(tag, "The firmware upgrade process is blocked");
		} else {
			ESP_LOGW(tag, "The firmware upgrade process is already running");
		}
		return;
	}
	size_t len = strlen(FIRMWARE_VERSION_PREFIX);
	if (strncasecmp(ver, FIRMWARE_VERSION_PREFIX, len) != 0) {
		ESP_LOGW(tag, "The firmware version prefix does not match: %s", FIRMWARE_VERSION_PREFIX);
		return;
	}
	strncpy(upgrade.version, ver, MAX_FIRMWARE_UPGRADE_VERSION_LENGTH);
	strncpy(upgrade.url, url, MAX_FIRMWARE_UPGRADE_URL_LENGTH);
	upgrade.state = UPGRADE_STATE_STARTED;
	xTaskCreatePinnedToCore(&http_device_update_task,
							"fw_update",
							8192,
							app,
							23,
							NULL,
							1);
}

static void _get_update_info(char *info_url) {
	esp_err_t err;
	ESP_LOGI(tag, "Checking for updates");
	esp_http_client_config_t http_client_config = {
			.url = info_url,
			.method = HTTP_METHOD_GET,
	};
	esp_http_client_handle_t http_client = esp_http_client_init(&http_client_config);
	esp_http_client_set_header(http_client, "Accept", "application/json");
	err = esp_http_client_open(http_client, 0);
	if (err != ESP_OK) {
		while (err != ESP_OK) {
			err = esp_http_client_open(http_client, 0);
		}
	}
	int content_len = esp_http_client_fetch_headers(http_client);
	if (content_len >= MAX_HTTP_TRANS_BUF) {
		ESP_LOGE(tag, "The size of the received data is larger than it can be accepted. "
				"Received data size = %d, buffer size = %d", content_len, MAX_JSON_BUF - 1);
		esp_http_client_cleanup(http_client);
		return;
	}
	int status_code = esp_http_client_get_status_code(http_client);
	ESP_LOGI(tag, "HTTP status code = %d, content length = %d", status_code, content_len);
	char *buf = malloc(MAX_JSON_BUF);
	if (!buf) {
		while (!buf) {
			buf = malloc(MAX_JSON_BUF);
		}
	}
	int data_read = esp_http_client_read(http_client, buf, content_len);
	esp_http_client_cleanup(http_client);
	if (data_read != content_len) {
		free(buf);
    	return;
	}
	buf[content_len] = 0;
	char *url = NULL;
	char *version = NULL;
	cJSON *json_root = cJSON_Parse(buf);
	cJSON *json_item = cJSON_GetObjectItem(json_root, "url");
	if (json_item != NULL) {
		url = json_item->valuestring;
	}
	json_item = cJSON_GetObjectItem(json_root, "version");
	if (json_item != NULL) {
		version = json_item->valuestring;
	}
	if ((url == NULL) || (version == NULL)) {
		ESP_LOGI(tag, "No information about the new firmware version");
	} else {
		ESP_LOGI(tag, "Version: %s, URL: %s", version, url);
		esp_task_wdt_reset();
		_check_version_and_update(NULL, version, url);
	}
	cJSON_Delete(json_root);
	free(buf);
}

static void http_device_update_task(void *arg) {
	esp_err_t err;
	esp_ota_handle_t update_handle = 0;
	char ota_write_data[BUF_LEN + 1] = { 0 };
	const esp_partition_t *update_partition = NULL;
	ESP_LOGI(tag, "'vHttpDeviceUpgradeTask' started");
	upgrade.state = UPGRADE_STATE_WORK;
	app_network_conn_t *app = (app_network_conn_t *)arg;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (configured != running) {
    	ESP_LOGW(tag, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
    	ESP_LOGW(tag, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(tag, "Running partition type %d subtype %d (offset 0x%08x)", running->type, running->subtype, running->address);
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
    	ESP_LOGI(tag, "Running firmware version: %s", running_app_info.version);
    	if (strncasecmp(upgrade.version, running_app_info.version, MAX_FIRMWARE_UPGRADE_VERSION_LENGTH) == 0) {
    		ESP_LOGI(tag, "The version of the new firmware is the same as the running version.");
    		task_exit(UPGRADE_STATE_IDLE);
    	}
    	size_t len = strlen(FIRMWARE_VERSION_PREFIX);
    	if (strncasecmp(running_app_info.version, FIRMWARE_VERSION_PREFIX, len) != 0) {
    		ESP_LOGW(tag, "The firmware version prefix does not match: %s", FIRMWARE_VERSION_PREFIX);
    		task_fatal_error(UPGRADE_STATE_STOPPED);
    	}
    }
    esp_task_wdt_reset();
    esp_http_client_config_t http_client_config = { 0 };
    if (app == NULL) {
    	http_client_config.url = upgrade.url;
    	http_client_config.method = HTTP_METHOD_GET;
    	http_client_config.disable_auto_redirect = false;
    } else {
    	http_client_config.url = upgrade.url;
    	http_client_config.username = app->device.login;
    	http_client_config.password = app->device.passwd;
    	http_client_config.auth_type = HTTP_AUTH_TYPE_BASIC;
    	http_client_config.method = HTTP_METHOD_GET;
    }
    ESP_LOGI(tag, "Connecting to the server: %s", upgrade.url);
    esp_http_client_handle_t client = esp_http_client_init(&http_client_config);
    if (client == NULL) {
    	ESP_LOGE(tag, "Failed to initialize HTTP connection");
    	task_fatal_error(UPGRADE_STATE_IDLE);
    }
    int data_read;
    int status_code;
    int content_len;
    int redirect_count = 0;
    do {
    	err = esp_http_client_open(client, 0);
    	if (err != ESP_OK) {
    		ESP_LOGE(tag, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    		esp_http_client_cleanup(client);
    		task_fatal_error(UPGRADE_STATE_IDLE);
    	}
    	content_len = esp_http_client_fetch_headers(client);
    	status_code = esp_http_client_get_status_code(client);
    	ESP_LOGI(tag, "HTTP status code = %d, content length = %d", status_code, content_len);
    	if ((status_code == 301) || (status_code == 302)) {
    		do {
    			data_read = esp_http_client_read(client, ota_write_data, BUF_LEN);
    		} while (data_read > 0);
    		esp_http_client_set_redirection(client);
    		ESP_LOGI(tag, "Redirecting");
    		redirect_count++;
    	}
    	esp_task_wdt_reset();
    } while (((status_code == 301) || (status_code == 302)) && (redirect_count < 3));
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(tag, "Writing to partition sub-type %d at offset 0x%x", update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);
    int binary_file_length = 0;
    bool image_header_was_checked = false;
    while (1) {
    	esp_task_wdt_reset();
    	data_read = esp_http_client_read(client, ota_write_data, BUF_LEN);
    	if (data_read < 0) {
    		ESP_LOGE(tag, "Error: SSL data read error");
    		esp_http_client_cleanup(client);
    		task_fatal_error(UPGRADE_STATE_IDLE);
    	} else if (data_read > 0) {
    		if (image_header_was_checked == false) {
    			esp_app_desc_t new_app_info;
    			if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
    				memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
    				ESP_LOGI(tag, "New firmware version: %s", new_app_info.version);
    				const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
    				esp_app_desc_t invalid_app_info;
    				if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
    					ESP_LOGI(tag, "Last invalid firmware version: %s", invalid_app_info.version);
    				}
    				if (last_invalid_app != NULL) {
    					if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
    						ESP_LOGW(tag, "New version is the same as invalid version.");
    						ESP_LOGW(tag, "Previously, there was an attempt to launch the firmware with %s version, "
    								"but it failed.", invalid_app_info.version);
    						ESP_LOGW(tag, "The firmware has been rolled back to the previous version.");
    						esp_http_client_cleanup(client);
    						task_exit(UPGRADE_STATE_STOPPED);
    					}
    				}
    				if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0) {
    					ESP_LOGW(tag, "Current running version is the same as a new. We will not continue the update.");
    					esp_http_client_cleanup(client);
    					task_exit(UPGRADE_STATE_IDLE);
    				}
    				image_header_was_checked = true;
    				err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    				if (err != ESP_OK) {
    					ESP_LOGE(tag, "esp_ota_begin failed (%s)", esp_err_to_name(err));
    					esp_http_client_cleanup(client);
    					task_fatal_error(UPGRADE_STATE_IDLE);
    				}
    				ESP_LOGI(tag, "esp_ota_begin succeeded");
    			} else {
    				ESP_LOGE(tag, "received package is not fit len");
    				esp_http_client_cleanup(client);
    				task_fatal_error(UPGRADE_STATE_IDLE);
    			}
    		}
    		err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
    		if (err != ESP_OK) {
    			esp_http_client_cleanup(client);
    			task_fatal_error(UPGRADE_STATE_IDLE);
    		}
    		binary_file_length += data_read;
    		ESP_LOGD(tag, "Written image length %d", binary_file_length);
    	} else if (data_read == 0) {
    		/* As esp_http_client_read never returns negative error code, we rely on
    		 * error code to check for underlying transport connectivity closure if any */
    		if (errno == ECONNRESET || errno == ENOTCONN) {
    			ESP_LOGE(tag, "Connection closed, error code = %d", errno);
    			break;
    		}
    		if (esp_http_client_is_complete_data_received(client) == true) {
    			ESP_LOGI(tag, "Connection closed");
    			break;
    		}
    	}
    }
    ESP_LOGI(tag, "Total Write binary data length: %d", binary_file_length);
    if (esp_http_client_is_complete_data_received(client) != true) {
    	ESP_LOGE(tag, "Error in receiving complete file");
    	esp_http_client_cleanup(client);
    	task_fatal_error(UPGRADE_STATE_IDLE);
    }
    esp_task_wdt_reset();
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
    	if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
    		ESP_LOGE(tag, "Image validation failed, image is corrupted");
    	}
    	ESP_LOGE(tag, "esp_ota_end failed (%s)!", esp_err_to_name(err));
    	esp_http_client_cleanup(client);
    	task_fatal_error(UPGRADE_STATE_IDLE);
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
    	ESP_LOGE(tag, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
    	esp_http_client_cleanup(client);
    	task_fatal_error(UPGRADE_STATE_IDLE);
    }
    ESP_LOGI(tag, "Prepare to restart system");
    esp_restart();
    return;
}

/* Export functions ----------------------------------------------------------*/

/* Requests the current firmware version and download link for it */
void app_update_get_and_check_version() {
	char *url = (char *)calloc(MAX_FIRMWARE_UPGRADE_URL_LENGTH + 1, sizeof(char));
	if (url == NULL) {
		ESP_LOGE(tag, "Cannot allocate memory for upgrade URL");
	}
	app_devdesc_string_read(url, SPI_FLASH_URL_UPGRADE_ADDR_OFFSET, MAX_FIRMWARE_UPGRADE_URL_LENGTH);
	ESP_LOGI(tag, "Firmware upgrade URL: %s", url);
#ifdef DEVELOP_VERSION
	_get_update_info("http://192.168.1.57:8070/anonymous/firmwareVersion");
#else
	_get_update_info(url);
#endif	/* DEVELOP_VERSION */
	free(url);
}
