/**
 * *****************************************************************************
 * @file		app_wifi.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Application functionality related to wireless network connection
 *
 * *****************************************************************************
 */

#define LOG_LOCAL_LEVEL		ESP_LOG_DEBUG

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <string.h>
#include <sys/param.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_err.h>
#include <esp_event_loop.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_wifi.h>
#include <cJSON.h>

/* User files */
#include "app.h"
#include "app_client.h"
#include "app_update.h"
#include "board_def.h"

/* Export constants ----------------------------------------------------------*/

const char *wifi_ap_recs_path = "/spiffs/wifi_ap_record.csv";

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_wifi";

/* Change the below entries to strings with the required values */
#define SOFTAP_ESP_WIFI_SSID	"RACCOON_APSTA"
#define SOFTAP_ESP_WIFI_PASSWD	""
#define SOFTAP_MAX_STA_CONN		1

/* Private functions prototypes ----------------------------------------------*/

/**
 * @brief		Perform the GET HTTP request to login
 * @param[in]	arg			User context
 * @param[out]	status_code	Variable used to store the HTTP status code
 * @return
 * 				- ESP_FAIL: Error performing HTTP request
 * 				- ESP_OK: Success
 */
static esp_err_t _exec_login_request(void *arg, int32_t *status_code);

/* Private functions ---------------------------------------------------------*/

/* Dummy HTTP client event handler */
static esp_err_t _http_client_event_handler(esp_http_client_event_t *evt) {
	return ESP_OK;
}

/* WiFi Event Handler */
static esp_err_t _wifi_event_handler(void *arg, system_event_t *event) {
	app_network_conn_t *ctx = (app_network_conn_t *)arg;
	EventBits_t event_bits = xEventGroupGetBits(ctx->event_group);
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_GOT_IP:
		ESP_LOGD(	tag,
					"Got IP: %s",
					ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
		xEventGroupClearBits(ctx->event_group, BIT_STA_DISCONNECTED);
		xEventGroupSetBits(ctx->event_group, BIT_STA_CONNECTED);
		esp_timer_stop(ctx->tim);
		if (event_bits & BIT_NEW_WIFI_CONF) {
			xEventGroupClearBits(ctx->event_group, BIT_NEW_WIFI_CONF);
			app_semaphore_take(ctx->spi_flash_mtx, portMAX_DELAY);
			int ret = app_spiffs_get_lines_num(wifi_ap_recs_path);
			if (ret <= 0) {
				if (ret == ESP_FAIL) {
					ESP_LOGD(tag, "File doesn't exist");
					app_spiffs_create_file(wifi_ap_recs_path);
				} else {
					ESP_LOGD(tag, "File is empty");
				}
			}
			app_spiffs_insert_record(	wifi_ap_recs_path,
										&ret,
										ctx->wifi_config.ssid,
										ctx->wifi_config.password);
			app_semaphore_give(ctx->spi_flash_mtx);
		}
		if (event_bits & BIT_CHECK_PENDING) {
			xEventGroupClearBits(ctx->event_group, BIT_CHECK_PENDING);
			xEventGroupSetBits(ctx->event_group, BIT_CONN_TO_INTERNET_OK);
		}
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		xEventGroupClearBits(ctx->event_group, BIT_STA_CONNECTED);
		xEventGroupSetBits(ctx->event_group, BIT_STA_DISCONNECTED);
		if (event_bits & BIT_RECONNECT) {
			if (!(event_bits & BIT_CONN_CORRUPTED)) {
				ESP_LOGD(tag, "Attempt to connect to the access point failed. Trying to reconnect");
				esp_timer_start_once(ctx->tim, 60000000);
				esp_wifi_connect();
			} else {
				xEventGroupClearBits(ctx->event_group, BIT_CONN_CORRUPTED);
				ESP_LOGE(tag, "The time allotted for reconnection has expired");
				ESP_LOGD(tag, "Finally failed to reconnect to the access point");
				/* TODO: Handle the connection corruption case (i.e., LED blinking) */
				app_clear_device_connection_data();
			}
		} else {
			if (event_bits & BIT_NEW_WIFI_CONF) {
				xEventGroupClearBits(ctx->event_group, BIT_NEW_WIFI_CONF);
			}
			if (event_bits & BIT_CHECK_PENDING) {
				xEventGroupClearBits(ctx->event_group, BIT_CHECK_PENDING);
				xEventGroupSetBits(ctx->event_group, BIT_CONN_TO_INTERNET_FAIL);
			}
			if (!(event_bits & (BIT_NEW_WIFI_CONF | BIT_CHECK_PENDING))) {
				ESP_LOGD(tag, "Station disabled");
			}
		}
		break;
	case SYSTEM_EVENT_AP_START:
		if (ctx->web_server == NULL) {
			ctx->web_server = app_server_start(ctx);
		}
		break;
	case SYSTEM_EVENT_AP_STOP:
		if (ctx->web_server != NULL) {
			app_server_stop(ctx->web_server);
			ctx->web_server = NULL;
		}
		break;
	case SYSTEM_EVENT_AP_STACONNECTED:
		ESP_LOGD(	tag,
					"Station: "MACSTR" joined, AID = %d",
					MAC2STR(event->event_info.sta_connected.mac),
					event->event_info.sta_connected.aid);
		break;
	case SYSTEM_EVENT_AP_STADISCONNECTED:
		ESP_LOGD(	tag,
					"Station: "MACSTR" has left, AID = %d",
					MAC2STR(event->event_info.sta_disconnected.mac),
					event->event_info.sta_disconnected.aid);
		break;
	default:
		break;
	}
	return ESP_OK;
}

/* Perform the GET HTTP request to login */
static esp_err_t _exec_login_request(void *arg, int32_t *status_code) {
	app_network_conn_t *ctx = (app_network_conn_t *)arg;
	int32_t ret = -1, data_len = -1, status = -1, err_cnt = 0;
	EventBits_t event_bits = 0;
	static char tx_item[MAX_HTTP_RECV_BUF + 1] = { 0 };
	char *uri_buf = (char *)calloc(DEFAULT_HTTP_BUF_SIZE, sizeof(char));
	if (!uri_buf) {
		while (!uri_buf) {
			vTaskDelay(1);
			uri_buf = (char *)calloc(DEFAULT_HTTP_BUF_SIZE, sizeof(char));
		}
	}
	strlcpy(uri_buf, (const char *)ctx->uri.login, DEFAULT_HTTP_BUF_SIZE);
	const esp_partition_t *partition = esp_ota_get_running_partition();
	esp_app_desc_t app_desc;
	esp_ota_get_partition_description(partition, &app_desc);
	strlcat(uri_buf,
			(const char *)app_desc.version,
			DEFAULT_HTTP_BUF_SIZE);
	esp_http_client_config_t client_cfg = {
			.url = uri_buf,
			.username = ctx->device.login,
			.password = ctx->device.passwd,
			.auth_type = HTTP_AUTH_TYPE_BASIC,
			.method = HTTP_METHOD_GET,
			.event_handler = _http_client_event_handler,
	};
	esp_http_client_handle_t tmpcli = esp_http_client_init(&client_cfg);
	ret = esp_http_client_open(tmpcli, 0);
	if (ret != ESP_OK) {
		while (ret != ESP_OK) {
			vTaskDelay(1);
			++err_cnt;
			event_bits = xEventGroupGetBits(ctx->event_group);
			if (err_cnt > 99 || (event_bits & BIT_STA_DISCONNECTED)) {
				free(uri_buf);
				return ESP_FAIL;
			}
			ret = esp_http_client_open(tmpcli, 0);
		}
	}
	esp_http_client_write(tmpcli, NULL, 0);
	data_len = esp_http_client_fetch_headers(tmpcli);
	status = esp_http_client_get_status_code(tmpcli);
	ESP_LOGD(	tag,
				"Performing GET for the URL %s\n[status:%d][length:%d]",
				(const char *)uri_buf,
				status,
				data_len);
	if (data_len < COMMON_RING_BUF_SIZE) {
		while (data_len > 0) {
			if ((ret = esp_http_client_read(tmpcli,
											tx_item,
											MIN(data_len, MAX_HTTP_RECV_BUF))) <= 0) {
				break;
			}
			UBaseType_t res = xRingbufferSend(	ctx->rbuf_hdl,
												tx_item,
												strlen(tx_item),
												pdMS_TO_TICKS(1000));
			if (res != pdTRUE) {
				while (res != pdTRUE) {
					res = xRingbufferSend(	ctx->rbuf_hdl,
											tx_item,
											strlen(tx_item),
											pdMS_TO_TICKS(1000));
				}
			}
			memset(tx_item, 0, sizeof tx_item);
			data_len -= ret;
		}
		size_t item_size;
		char *item = (char *)xRingbufferReceiveUpTo(ctx->rbuf_hdl,
													&item_size,
													pdMS_TO_TICKS(1000),
													COMMON_RING_BUF_SIZE);
		if (item != NULL) {
			item[item_size] = '\0';
			ESP_LOGD(tag, "Login response HTTP message: %s", (const char *)item);
			vRingbufferReturnItem(ctx->rbuf_hdl, (void *)item);
		} else {
			ESP_LOGD(tag, "Failed to receive login response HTTP message");
		}
	} else {
		char *buf = (char *)malloc(MAX_HTTP_RECV_BUF + 1);
		if (!buf) {
			while (!buf) {
				vTaskDelay(1);
				buf = (char *)malloc(MAX_HTTP_RECV_BUF + 1);
			}
		}
		while (data_len > 0) {
			if ((ret = esp_http_client_read(tmpcli,
											buf,
											MIN(data_len, MAX_HTTP_RECV_BUF))) <= 0) {
				break;
			}
			data_len -= ret;
		}
		free(buf);
		ESP_LOGD(tag, "The memory required to hold the login response HTTP message could not be allocated");
	}
	esp_http_client_close(tmpcli);
	esp_http_client_cleanup(tmpcli);
	free(uri_buf);
	*status_code = status;
	return ESP_OK;
}

/* Export functions ----------------------------------------------------------*/

/**
 * @ingroup	app_wifi_event
 * Wait for signal when the station is connected & ready to make a request
 */
void app_wifi_wait_sta_connected(EventGroupHandle_t event_group) {
	xEventGroupWaitBits(event_group,
						BIT_STA_CONNECTED,
						false,
						true,
						portMAX_DELAY);
}

/**
 * @ingroup	app_wifi_event
 * Wait for signal when the station is disconnected
 */
void app_wifi_wait_sta_disconnected(EventGroupHandle_t event_group) {
	xEventGroupWaitBits(event_group,
						BIT_STA_DISCONNECTED,
						false,
						true,
						portMAX_DELAY);
}

/**
 * @ingroup	app_wifi_event
 * Wait for signal when the station made an attempt to connect to
 * access point with the given parameters
 */
void app_wifi_wait_conn_attempt(EventGroupHandle_t event_group) {
	xEventGroupWaitBits(event_group,
						BIT_CONN_TO_INTERNET_OK | BIT_CONN_TO_INTERNET_FAIL,
						false,
						false,
						portMAX_DELAY);
}

/**
 * @ingroup	app_wifi_init
 * Initialize the application WiFi node
 */
void app_wifi_init(void *ctx, int arg) {
	app_network_conn_t *app = (app_network_conn_t *)ctx;
	int32_t status = -1;
	tcpip_adapter_init();
	esp_event_loop_init(_wifi_event_handler, app);
	if (arg != ESP_OK) {
		if (arg == ESP_ERR_NOT_FOUND) {
			app_wifi_apsta_set(SOFTAP_ESP_WIFI_SSID, SOFTAP_ESP_WIFI_PASSWD);
			return;
		}
	}
	if (strlen(app->device.server_url) == 0) {
		app_wifi_apsta_set(SOFTAP_ESP_WIFI_SSID, SOFTAP_ESP_WIFI_PASSWD);
		return;
	}
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&cfg);
	esp_wifi_set_storage(WIFI_STORAGE_RAM);
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_start();
	int32_t read_ret = app_spiffs_get_lines_num(wifi_ap_recs_path);
	if (read_ret <= 0) {
		if (read_ret == ESP_FAIL) {
			ESP_LOGD(tag, "File access failed");
			app_spiffs_create_file(wifi_ap_recs_path);
		} else {
			ESP_LOGD(tag, "File is empty");
		}
		app_wifi_switch_to_apsta();
		return;
	}
	app_spiffs_ap_record_t ap_saved[read_ret];
	memset(ap_saved, 0, sizeof ap_saved);
	if (app_spiffs_read_records(wifi_ap_recs_path, &read_ret, ap_saved) != ESP_OK) {
		ESP_LOGD(tag, "File is damaged");
		app_spiffs_create_file(wifi_ap_recs_path);
		app_wifi_switch_to_apsta();
		return;
	}
	for (int i = 0; i < read_ret; ++i) {
		ESP_LOGD(tag, "Read SSID: %s", (const char *)ap_saved[i].ssid);
	}
	for (int i = read_ret - 1; i >= 0; --i) {
		if (app_spiffs_get_password(wifi_ap_recs_path, (uint16_t *)&i, ap_saved) != ESP_OK) {
			ESP_LOGD(tag, "File is damaged");
			app_spiffs_create_file(wifi_ap_recs_path);
			app_wifi_switch_to_apsta();
			return;
		}
		ESP_LOGD(tag, "Read password: %s", (const char *)ap_saved[i].password);
		memset(app->wifi_config.ssid, 0, sizeof app->wifi_config.ssid);
		memset(app->wifi_config.password, 0, sizeof app->wifi_config.password);
		strlcpy(app->wifi_config.ssid, (const char *)ap_saved[i].ssid, sizeof app->wifi_config.ssid);
		strlcpy(app->wifi_config.password, (const char *)ap_saved[i].password, sizeof app->wifi_config.password);
		xEventGroupClearBits(app->event_group, BIT_RECONNECT);
		xEventGroupSetBits(app->event_group, BIT_CHECK_PENDING);
		app_wifi_sta_join(app, WIFI_MODE_STA, app->wifi_config.ssid, app->wifi_config.password);
		app_wifi_wait_conn_attempt(app->event_group);
		if (xEventGroupGetBits(app->event_group) & BIT_CONN_TO_INTERNET_OK) {
			xEventGroupSetBits(app->event_group, BIT_RECONNECT);
			xEventGroupClearBits(app->event_group, BIT_CONN_TO_INTERNET_OK);
			esp_err_t ret = _exec_login_request(app, &status);
			if (ret == ESP_OK) {
				if (status == HTTP_200) {
					xTaskCreatePinnedToCore(&http_profile_getter_task,
											"get_profile",
											8192,
											&app->client,
											4,
											&app->client.hdl,
											0);
					app_update_get_and_check_version();
				} else if (status == 401) {
					ESP_LOGW(tag, "Reset device settings due to 401 error");
					app_clear_device_connection_data();
				} else {
					app_restart_device();
				}
			} else {
				ESP_LOGD(	tag,
							"Error performing GET request for the URL %s",
							(const char *)app->uri.login);
				app_restart_device();
			}
			return;
		} else if (xEventGroupGetBits(app->event_group) & BIT_CONN_TO_INTERNET_FAIL) {
			xEventGroupSetBits(app->event_group, BIT_RECONNECT);
			xEventGroupClearBits(app->event_group, BIT_CONN_TO_INTERNET_FAIL);
			continue;
		}
	}
	ESP_LOGW(tag, "No suitable SSID exists");
	app_wifi_switch_to_apsta();
}

/**
 * @ingroup	app_wifi_init
 * Join to the specified access point
 */
esp_err_t app_wifi_sta_join(void *arg,
							wifi_mode_t mode,
							const char *ssid,
							const char *pass) {
	app_network_conn_t *ctx = (app_network_conn_t *)arg;
	if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA) {
		return ESP_ERR_INVALID_ARG;
	}
	wifi_config_t wifi_config;
	memset(&wifi_config, 0, sizeof wifi_config);
	strlcpy((char *)wifi_config.sta.ssid,
			(const char *)ssid,
			sizeof wifi_config.sta.ssid);
	if (pass) {
		strlcpy((char *)wifi_config.sta.password,
				(const char *)pass,
				sizeof wifi_config.sta.password);
	}
	app_wifi_sta_detach(ctx);
	if (strlen(pass) == 0) {
		ESP_LOGD(	tag,
					"Trying to connect. SSID: %s",
					(const char *)wifi_config.sta.ssid);
	} else {
		ESP_LOGD(	tag,
					"Trying to connect. SSID: %s; password: %s",
					(const char *)wifi_config.sta.ssid,
					(const char *)wifi_config.sta.password);
	}
	esp_wifi_set_mode(mode);
	esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
	esp_wifi_connect();
	return ESP_OK;
}

/**
 * @ingroup	app_wifi_init
 * Disconnect from an access point
 */
void app_wifi_sta_detach(void *arg) {
	app_network_conn_t *ctx = (app_network_conn_t *)arg;
	EventBits_t event_bits = xEventGroupGetBits(ctx->event_group);
	if (event_bits & BIT_STA_CONNECTED) {
		xEventGroupClearBits(ctx->event_group, BIT_STA_CONNECTED | BIT_RECONNECT);
		esp_wifi_disconnect();
		app_wifi_wait_sta_disconnected(ctx->event_group);
		xEventGroupSetBits(ctx->event_group, BIT_RECONNECT);
	}
}

/**
 * @ingroup	app_wifi_init
 * Set up a soft access point
 */
void app_wifi_apsta_set(const char *ssid, const char *pass) {
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&cfg);
	esp_wifi_set_storage(WIFI_STORAGE_RAM);
	wifi_config_t wifi_config = {
			.ap = {
					.ssid = "",
					.password = "",
					.ssid_len = 0,
					.authmode = WIFI_AUTH_WPA_WPA2_PSK,
					.max_connection = SOFTAP_MAX_STA_CONN,
			},
	};
	strlcpy((char *)wifi_config.ap.ssid,
			(const char *)ssid,
			sizeof wifi_config.ap.ssid);
	if (pass) {
		strlcpy((char *)wifi_config.ap.password,
				(const char *)pass,
				sizeof wifi_config.ap.password);
	}
	if (strlen(pass) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}
	esp_wifi_set_mode(WIFI_MODE_APSTA);
	esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
	esp_wifi_start();
	if (strlen(pass) == 0) {
		ESP_LOGD(	tag,
					"'app_wifi_apsta_set' finished. SSID: %s",
					(const char *)wifi_config.ap.ssid);
	} else {
		ESP_LOGD(	tag,
					"'app_wifi_apsta_set' finished. SSID: %s; password: %s",
					(const char *)wifi_config.ap.ssid,
					(const char *)wifi_config.ap.password);
	}
}

/**
 * @ingroup	app_wifi_init
 * Initialize station+soft-AP mode
 */
void app_wifi_switch_to_apsta(void) {
	esp_wifi_stop();
	esp_wifi_deinit();
	app_wifi_apsta_set(SOFTAP_ESP_WIFI_SSID, SOFTAP_ESP_WIFI_PASSWD);
}

/**
 * @ingroup	app_wifi_init
 * Scan for available set of APs
 */
int32_t app_wifi_scan(uint16_t *ap_num, wifi_ap_record_t *ap_list_buf) {
	uint16_t cnt;
	esp_wifi_scan_start(NULL, true);
	esp_wifi_scan_get_ap_records(ap_num, ap_list_buf);
	esp_wifi_scan_get_ap_num(&cnt);
	ESP_LOGD(tag, "Total APs scanned = %d", cnt);
	for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < cnt); ++i) {
		ESP_LOGD(	tag,
					"[SSID:%s][RSSI:%d][channel:%d]",
					ap_list_buf[i].ssid,
					ap_list_buf[i].rssi,
					ap_list_buf[i].primary);
	}
	return (int32_t)cnt;
}

/**
 * @ingroup	app_wifi_init
 * Compare list of available APs with list of APs that is stored in memory
 */
esp_err_t app_wifi_check_if_ap_exists(	int scan_ap_num,
										wifi_ap_record_t *scan_ap_records,
										int spiffs_ap_num,
										app_spiffs_ap_record_t *spiffs_ap_records) {
	for (int i = 0; i < scan_ap_num; ++i) {
		for (int j = 0; j < spiffs_ap_num; ++j) {
			if (!strncmp(	(const char *)scan_ap_records[i].ssid,
							(const char *)spiffs_ap_records[j].ssid,
							DEFAULT_WIFI_SSID_LEN)) {
				ESP_LOGD(tag, "Suitable SSID has been found: %s", scan_ap_records[i].ssid);
				return j;
			}
		}
	}
	return ESP_ERR_NOT_FOUND;
}

/* WiFi initialization task */
void app_wifi_init_task(void *arg) {
	app_wifi_initializer_t *wifi_cfg = (app_wifi_initializer_t *)arg;
	app_wifi_init(wifi_cfg->app_ptr, wifi_cfg->init_state);
	vTaskDelete(NULL);
}
