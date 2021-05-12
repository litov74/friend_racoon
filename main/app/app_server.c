/**
 * *****************************************************************************
 * @file		app_server.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Application's server node
 *
 * *****************************************************************************
 */

#define LOG_LOCAL_LEVEL		ESP_LOG_DEBUG

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdbool.h>
#include <string.h>
#include <sys/param.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <cJSON.h>

/* User files */
#include "app.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_server";

/* Private variables ---------------------------------------------------------*/

xTaskHandle reg_task_hdl = NULL;

/* Private functions prototypes ----------------------------------------------*/

/**
 * @brief		Extract the request parameters from JSON string
 * @param[in]	req	Pointer to HTTP request data structure
 * @return
 * 				- ESP_FAIL: Unexpected error
 * 				- ESP_OK: Success
 */
static esp_err_t app_server_extract_request_params(httpd_req_t *req);

/**
 * @brief		Send registration parameters to the server
 * @param[in]	arg	A pointer to the application sound recorder instance
 * @return
 * 				- None
 */
void http_registration_task(void *arg);

/* Private functions ---------------------------------------------------------*/

/* Dummy HTTP client event handler */
static esp_err_t _http_client_event_handler(esp_http_client_event_t *evt) {
	return ESP_OK;
}

/* Extract the request parameters from JSON string */
static esp_err_t app_server_extract_request_params(httpd_req_t *req) {
	app_network_conn_t *ctx = (app_network_conn_t *)req->user_ctx;
	int32_t bytes_num = -1;
	static char tx_item[MAX_HTTP_RECV_BUF + 1];
	memset(tx_item, 0, sizeof tx_item);
	int32_t remaining = req->content_len;
	if (remaining < COMMON_RING_BUF_SIZE) {
		while (remaining > 0) {
			if ((bytes_num = httpd_req_recv(req,
											tx_item,
											MIN(remaining, MAX_HTTP_RECV_BUF))) <= 0) {
				if (bytes_num == HTTPD_SOCK_ERR_TIMEOUT) {
					continue;
				}
				return ESP_FAIL;
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
			remaining -= bytes_num;
		}
		memset(ctx->wifi_config.ssid, 0, sizeof ctx->wifi_config.ssid);
		memset(ctx->wifi_config.password, 0, sizeof ctx->wifi_config.password);
		memset(ctx->device.client_id, 0, sizeof ctx->device.client_id);
		size_t item_size;
		char *item = (char *)xRingbufferReceiveUpTo(ctx->rbuf_hdl,
													&item_size,
													pdMS_TO_TICKS(1000),
													COMMON_RING_BUF_SIZE);
		if (item != NULL) {
			item[item_size] = '\0';
		} else {
			return ESP_FAIL;
		}
		cJSON *root = cJSON_Parse(item), *json_param = NULL;
		do {
			json_param = cJSON_GetObjectItem(root, "ssid");
			if (json_param != NULL) {
				char *tmpstr = json_param->valuestring;
				strncpy(ctx->wifi_config.ssid, tmpstr, strlen(tmpstr));
				json_param = NULL;
				ESP_LOGD(	tag,
							"[ssid:%s]\n[length:%d]",
							(const char *)ctx->wifi_config.ssid,
							strlen(ctx->wifi_config.ssid));
			} else {
				break;
			}
			json_param = cJSON_GetObjectItem(root, "password");
			if (json_param != NULL) {
				char *tmpstr = json_param->valuestring;
				strncpy(ctx->wifi_config.password, tmpstr, strlen(tmpstr));
				json_param = NULL;
				ESP_LOGD(	tag,
							"[password:%s]\n[length:%d]",
							(const char *)ctx->wifi_config.password,
							strlen(ctx->wifi_config.password));
			} else {
				break;
			}
			json_param = cJSON_GetObjectItem(root, "clientId");
			if (json_param != NULL) {
				char *tmpstr = json_param->valuestring;
				app_semaphore_take(ctx->spi_flash_mtx, portMAX_DELAY);
				if (app_devdesc_client_id_write(tmpstr, strlen(tmpstr)) == ESP_OK) {
					uint16_t len = (uint16_t)app_devdesc_id_field_len_read(SPI_FLASH_CLIENT_ID_SIZE_ADDR_OFFSET);
					ESP_ERROR_CHECK( app_devdesc_client_id_read(ctx->device.client_id, len) );
					app_semaphore_give(ctx->spi_flash_mtx);
					json_param = NULL;
					ESP_LOGD(	tag,
								"[clientId:%s]\n[length:%u]",
								ctx->device.client_id,
								len);
				} else {
					app_semaphore_give(ctx->spi_flash_mtx);
					break;
				}
			} else {
				break;
			}
			json_param = cJSON_GetObjectItem(root, "userToken");
			if (json_param != NULL) {
				char *tmpstr = json_param->valuestring;
				app_semaphore_take(ctx->spi_flash_mtx, portMAX_DELAY);
				if (app_devdesc_user_token_write(tmpstr, strlen(tmpstr)) == ESP_OK) {
					uint16_t len = (uint16_t)app_devdesc_id_field_len_read(SPI_FLASH_USER_TOKEN_SIZE_ADDR_OFFSET);
					app_semaphore_give(ctx->spi_flash_mtx);
					json_param = NULL;
					ESP_LOGD(	tag,
								"[userToken:%s]\n[length:%u]",
								tmpstr,
								len);
				} else {
					app_semaphore_give(ctx->spi_flash_mtx);
					break;
				}
			} else {
				break;
			}
			char *update_url, *working_url;
			json_param = cJSON_GetObjectItem(root, "urlUpgrade");
			if (json_param != NULL) {
				update_url = json_param->valuestring;
				json_param = NULL;
			} else {
				break;
			}
			json_param = cJSON_GetObjectItem(root, "urlWork");
			if (json_param != NULL) {
				working_url = json_param->valuestring;
				app_semaphore_take(ctx->spi_flash_mtx, portMAX_DELAY);
				if (app_devdesc_url_write(&ctx->device, working_url, update_url) == ESP_OK) {
					uint16_t len1 = (uint16_t)app_devdesc_id_field_len_read(SPI_FLASH_URL_UPGRADE_ADDR_OFFSET);
					uint16_t len2 = (uint16_t)app_devdesc_id_field_len_read(SPI_FLASH_URL_WORK_ADDR_OFFSET);
					app_semaphore_give(ctx->spi_flash_mtx);

					// обновим ссылки
					app_uri_init(ctx);

					ESP_LOGD(	tag,
								"[urlUpgrade:%s]\n[length:%u]",
								update_url,
								len1);

					ESP_LOGD(	tag,
								"[urlWork:%s]\n[length:%u]",
								working_url,
								len2);
				} else {
					app_semaphore_give(ctx->spi_flash_mtx);
					break;
				}
			} else {
				break;
			}
			cJSON_Delete(root);
			vRingbufferReturnItem(ctx->rbuf_hdl, (void *)item);
			return ESP_OK;
		} while (0);
		cJSON_Delete(root);
		vRingbufferReturnItem(ctx->rbuf_hdl, (void *)item);
		return ESP_FAIL;
	} else {
		return ESP_FAIL;
	}
	return ESP_OK;
}

/* Send registration parameters to the server */
void http_registration_task(void *arg) {
	app_network_conn_t *ctx = (app_network_conn_t *)arg;
	int32_t ret = -1, data_len = -1, status = -1, err_cnt = 0;
	EventBits_t event_bits = 0;
	BaseType_t xReturned = pdFALSE;
	for (;;) {
		xEventGroupClearBits(ctx->event_group, BIT_RECONNECT);
		xEventGroupSetBits(ctx->event_group, BIT_CHECK_PENDING | BIT_NEW_WIFI_CONF);
		app_wifi_sta_join(ctx, WIFI_MODE_APSTA, ctx->wifi_config.ssid, ctx->wifi_config.password);
		app_wifi_wait_conn_attempt(ctx->event_group);
		if (xEventGroupGetBits(ctx->event_group) & BIT_CONN_TO_INTERNET_OK) {
			xEventGroupSetBits(ctx->event_group, BIT_RECONNECT);
			xEventGroupClearBits(ctx->event_group, BIT_CONN_TO_INTERNET_OK);
			cJSON *root = cJSON_CreateObject();
			cJSON_AddItemToObject(root, "login", cJSON_CreateString(ctx->device.login));
			cJSON_AddItemToObject(root, "password", cJSON_CreateString(ctx->device.pass_hash));
			cJSON *client_data = cJSON_CreateObject();
			cJSON_AddItemToObject(client_data, "id", cJSON_CreateString(ctx->device.client_id));
			app_semaphore_take(ctx->spi_flash_mtx, portMAX_DELAY);
			uint16_t len = (uint16_t)app_devdesc_id_field_len_read(SPI_FLASH_USER_TOKEN_SIZE_ADDR_OFFSET);
			char *client_token = (char *)calloc(len + 1, sizeof(char));
			if (client_token == NULL) {
				while (client_token == NULL) {
					vTaskDelay(1);
					client_token = (char *)calloc(len + 1, sizeof(char));
				}
			}
			ESP_ERROR_CHECK( app_devdesc_user_token_read(client_token, len) );
			app_semaphore_give(ctx->spi_flash_mtx);
			cJSON_AddItemToObject(client_data, "userToken", cJSON_CreateString(client_token));
			cJSON_AddItemToObject(root, "client", client_data);
			char *str = cJSON_Print(root);
			esp_http_client_config_t cli_cfg = {
					.url = ctx->uri.regdev,
					.method = HTTP_METHOD_POST,
					.event_handler = _http_client_event_handler,
			};
			esp_http_client_handle_t tmpcli = esp_http_client_init(&cli_cfg);
			esp_http_client_set_post_field(tmpcli, str, strlen(str));
			esp_http_client_set_header(tmpcli, "Content-Type", HTTPD_TYPE_JSON);
			ESP_LOGD(tag, "Performing POST for the URL %s", (const char *)ctx->uri.regdev);
			ret = esp_http_client_open(tmpcli, strlen(str));
			if (ret != ESP_OK) {
				while (ret != ESP_OK) {
					vTaskDelay(1);
					++err_cnt;
					event_bits = xEventGroupGetBits(ctx->event_group);
					if (err_cnt > 99 || (event_bits & BIT_STA_DISCONNECTED)) {
						break;
					}
					ret = esp_http_client_open(tmpcli, strlen(str));
				}
			} else {
				esp_http_client_write(tmpcli, str, strlen(str));
			}
			cJSON_Delete(root);
			free(client_token);
			if (err_cnt > 99 || (event_bits & BIT_STA_DISCONNECTED)) {
				break;
			}
			data_len = esp_http_client_fetch_headers(tmpcli);
			status = esp_http_client_get_status_code(tmpcli);
			ESP_LOGD(	tag,
						"Registration request result = [code:%d][length:%d]",
						status,
						data_len);
			static char rx_item[MAX_HTTP_RECV_BUF + 1];
			memset(rx_item, 0, sizeof rx_item);
			while (data_len > 0) {
				if ((ret = esp_http_client_read(tmpcli,
												rx_item,
												MIN(data_len, MAX_HTTP_RECV_BUF))) <= 0) {
					break;
				}
				data_len -= ret;
			}
			esp_http_client_close(tmpcli);
			esp_http_client_cleanup(tmpcli);
			if (status == 200 || status == (-1)) {
				xReturned = pdTRUE;
			}
			break;
		} else if (xEventGroupGetBits(ctx->event_group) & BIT_CONN_TO_INTERNET_FAIL) {
			xEventGroupSetBits(ctx->event_group, BIT_RECONNECT);
			xEventGroupClearBits(ctx->event_group, BIT_CONN_TO_INTERNET_FAIL);
			break;
		}
	}
	if (xReturned == pdTRUE) {
		app_restart_device();
	}
	reg_task_hdl = NULL;
	vTaskDelete(NULL);
}

/* HTTP GET handler */
static esp_err_t _network_get_handler(httpd_req_t *req) {
	char *buf;
	size_t buf_len;
	/* Get header value string length and allocate memory for length + 1,
	 * extra byte for null termination */
	buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
	if (buf_len > 1) {
		buf = (char *)malloc(buf_len);
		if (!buf) {
			while (!buf) {
				vTaskDelay(1);
				buf = (char *)malloc(buf_len);
			}
		}
		/* Copy null terminated value string into buffer */
		if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
			ESP_LOGD(tag, "Found header => Host: %s", buf);
		}
		free(buf);
	}
	buf_len = httpd_req_get_hdr_value_len(req, "Accept") + 1;
	if (buf_len > 1) {
		buf = (char *)malloc(buf_len);
		if (!buf) {
			while (!buf) {
				vTaskDelay(1);
				buf = (char *)malloc(buf_len);
			}
		}
		/* Copy null terminated value string into buffer */
		if (httpd_req_get_hdr_value_str(req, "Accept", buf, buf_len) == ESP_OK) {
			ESP_LOGD(tag, "Found header => Accept: %s", buf);
		}
		free(buf);
	}
	/* Send response with custom headers and body set as the
	 * string passed in user context */
	uint16_t ap_num = DEFAULT_SCAN_LIST_SIZE;
	wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
	memset(ap_info, 0, sizeof ap_info);
	uint16_t ap_count = (uint16_t)app_wifi_scan(&ap_num, ap_info);
	/* Create the root object */
	cJSON *root = cJSON_CreateArray();
	for (int i = 0; i < DEFAULT_SCAN_LIST_SIZE && i < ap_count; ++i) {
		cJSON *network = cJSON_CreateObject();
		cJSON_AddItemToArray(root, network);
		cJSON_AddItemToObject(network, "ssid", cJSON_CreateString((const char *)ap_info[i].ssid));
		cJSON_AddItemToObject(network, "rssi", cJSON_CreateNumber((double)ap_info[i].rssi));
	}
	char *tmpstr = cJSON_Print(root);
	/* Set some custom headers */
	httpd_resp_set_status(req, HTTPD_200);
	httpd_resp_set_type(req, HTTPD_TYPE_JSON);
	httpd_resp_send(req, tmpstr, strlen(tmpstr));
	/* Cleanup the root object */
	cJSON_Delete(root);
	/* After sending the HTTP response the old HTTP request
	 * headers are lost. Check if HTTP request headers can be read now */
	if (!httpd_req_get_hdr_value_len(req, "Host")) {
		ESP_LOGD(tag, "Request headers lost");
	}
	int mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
	ESP_LOGD(tag, "Current free memory: %d", mem);
	return ESP_OK;
}

/* HTTP POST handler */
static esp_err_t _network_post_handler(httpd_req_t *req) {
	esp_err_t ret = ESP_FAIL;
	app_network_conn_t *ctx = (app_network_conn_t *)req->user_ctx;
	if (app_server_extract_request_params(req) != ESP_OK) {
		ret = ESP_FAIL;
	} else {
		if (reg_task_hdl == NULL) {
			xTaskCreatePinnedToCore(http_registration_task,
									"register",
									8192,
									ctx,
									4,
									&reg_task_hdl,
									0);
			ret = ESP_OK;
		}
	}
	int mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
	ESP_LOGD(tag, "Current free memory: %d", mem);
	return ret;
}

/* Export functions ----------------------------------------------------------*/

/* Starts the web server */
httpd_handle_t app_server_start(void *arg) {
	app_network_conn_t *ctx = (app_network_conn_t *)arg;
	httpd_uri_t network_get = {
			.uri = "/esp32/network",
			.method = HTTP_GET,
			.handler = _network_get_handler,
			.user_ctx = ctx,
	};
	httpd_uri_t network_post = {
			.uri = "/esp32/network",
			.method = HTTP_POST,
			.handler = _network_post_handler,
			.user_ctx = ctx,
	};
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	ESP_LOGI(tag, "Starting server on port %d", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		ESP_LOGI(tag, "Registering URI handlers");
		httpd_register_uri_handler(server, &network_get);
		httpd_register_uri_handler(server, &network_post);
		return (httpd_handle_t)server;
	}
	ESP_LOGE(tag, "Error starting server");
	return NULL;
}

/* Stops the web server */
void app_server_stop(httpd_handle_t server) {
	httpd_stop(server);
}
