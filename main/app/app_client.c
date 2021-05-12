/**
 * *****************************************************************************
 * @file		app_client.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		FreeRTOS tasks of the client module of the application
 *
 * *****************************************************************************
 */

#define LOG_LOCAL_LEVEL		ESP_LOG_DEBUG

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

/* Framework */
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/projdefs.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <cJSON.h>

/* User files */
#include "app.h"
#include "app_client.h"
#include "app_update.h"
#include "board_def.h"
#include "mp45dt02.h"
#include "vs1053b.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_client";

/* Private functions ---------------------------------------------------------*/

/* Dummy HTTP client event handler */
static esp_err_t _http_client_event_handler(esp_http_client_event_t *evt) {
	return ESP_OK;
}

/* Export functions ----------------------------------------------------------*/

/* Execute the end-of-reproduction request */
void app_client_delete_track(sound_player_t *player) {
	ESP_LOGW(tag, "Performing DELETE for the URL %s", (const char *)app_instance.uri.player);
	int32_t ret = -1, data_len = -1, read_len = -1, err_cnt = 0;
	EventBits_t event_bits = 0;
	esp_http_client_config_t client_cfg = {
			.url = app_instance.uri.player,
			.username = app_instance.device.login,
			.password = app_instance.device.passwd,
			.auth_type = HTTP_AUTH_TYPE_BASIC,
			.method = HTTP_METHOD_DELETE,
			.event_handler = _http_client_event_handler,
	};
	player->http_cleaner_client = esp_http_client_init(&client_cfg);
	ret = esp_http_client_open(player->http_cleaner_client, 0);
	if (ret != ESP_OK) {
		while (ret != ESP_OK) {
			vTaskDelay(pdMS_TO_TICKS(100));
			++err_cnt;
			event_bits = xEventGroupGetBits(app_instance.event_group);
			if (err_cnt > 99 || (event_bits & BIT_STA_DISCONNECTED)) {
				break;
			}
			ret = esp_http_client_open(player->http_cleaner_client, 0);
		}
	}
	if (err_cnt <= 99 && !(event_bits & BIT_STA_DISCONNECTED)) {
		data_len = esp_http_client_fetch_headers(player->http_cleaner_client);
		esp_http_client_get_status_code(player->http_cleaner_client);
		char *buf = malloc(MAX_HTTP_RECV_BUF + 1);
		while (data_len > 0) {
			if ((read_len = esp_http_client_read(	player->http_cleaner_client,
													buf,
													MIN(data_len, MAX_HTTP_RECV_BUF))) <= 0) {
				break;
			}
			data_len -= read_len;
		}
		esp_http_client_close(player->http_cleaner_client);
		free(buf);
	}
	esp_http_client_cleanup(player->http_cleaner_client);
}

/**
 * @ingroup	app_client_rtos_tasks
 * Requests the current state of the profile from the server and
 * performs actions in accordance with the values ​​of its parameters
 */
void http_profile_getter_task(void *arg) {
	BaseType_t is_deleted = pdFALSE;
	app_client_func_t *client = (app_client_func_t *)arg;
	int32_t ret = -1;
	EventBits_t event_bits = 0;
	app_client_profile_t tmpprof = { 0 };
	esp_http_client_config_t client_cfg = {
			.url = app_instance.uri.profile,
			.username = app_instance.device.login,
			.password = app_instance.device.passwd,
			.auth_type = HTTP_AUTH_TYPE_BASIC,
			.method = HTTP_METHOD_GET,
			.event_handler = _http_client_event_handler,
	};
	client->http_client = esp_http_client_init(&client_cfg);
	esp_http_client_set_header(client->http_client, "Accept", "application/json");
	xTaskCreatePinnedToCore(http_sound_getter_task,
							"song_get",
							8192,
							&client->player,
							4,
							&client->player.getter_hdl,
							0);
	xTaskCreatePinnedToCore(http_sound_sender_task,
							"voice_send",
							8192,
							&client->sampler,
							4,
							&client->sampler.sender_hdl,
							0);
	for (;;) {
		event_bits = xEventGroupGetBits(app_instance.event_group);
		if (event_bits & BIT_STA_DISCONNECTED) {
			app_client_halt_media_tasks(client);
			vTaskDelay(pdMS_TO_TICKS(2000));
		} else {
			if (!xSemaphoreTake(client->semphr, (TickType_t)10)) {
				continue;
			}
			ret = app_client_get_device_profile(client->http_client, &tmpprof);
			if (ret != ESP_OK) {
				ESP_LOGE(tag, "Failed to perform profile HTTP request (%s)", esp_err_to_name(ret));
				if (ret != ESP_ERR_INVALID_STATE) {
					xSemaphoreGive(client->semphr);
					continue;
				} else {
					/* Reset the device configuration, then perform the software reset */
					xSemaphoreGive(client->semphr);
					app_client_halt_media_tasks(client);
					/* Sets the flag for deleting connection settings from device memory and resetting */
					is_deleted = pdTRUE;
					break;
				}
			}
			xSemaphoreTake(client->player.semphr, portMAX_DELAY);
			app_client_set_player_state(&client->player, &tmpprof);
			xSemaphoreGive(client->player.semphr);
			xSemaphoreTake(client->sampler.semphr, portMAX_DELAY);
			app_client_set_sampler_state(&client->sampler, &tmpprof);
			xSemaphoreGive(client->sampler.semphr);
#if BOARD_USER_LED_FEATURE
			if ((	(client->player.state == GETTER_ACTIVE || client->player.state == GETTER_STOP_AT_THE_END) ||
					(client->sampler.state == SAMPLER_ACTIVE)) &&
					!client->led_tracker) {
				gpio_set_level(PIN_NUM_USER_LED, 1);
				client->led_tracker = pdTRUE;
			} else if (((client->player.state != GETTER_ACTIVE) &&
						(client->sampler.state != SAMPLER_ACTIVE)) &&
						client->led_tracker) {
				gpio_set_level(PIN_NUM_USER_LED, 0);
				client->led_tracker = pdFALSE;
			}
#endif	/* BOARD_USER_LED_FEATURE */
			xSemaphoreGive(client->semphr);
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
		int mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
		ESP_LOGD(tag, "Current free memory: %d", mem);
	}
	esp_http_client_cleanup(client->http_client);
	if (is_deleted == pdTRUE) {
		ESP_LOGW(tag, "Resetting device settings due to 401 error");
		app_clear_device_connection_data();
	}
	client->hdl = NULL;
	vTaskDelete(NULL);
}

/**
 * @ingroup	app_client_rtos_tasks
 * Get an audio data from an HTTP server
 */
void http_sound_getter_task(void *arg) {
	sound_player_t *player = (sound_player_t *)arg;
	xTaskCreatePinnedToCore(sound_decoder_task,
							"song_play",
							2048,
							player,
							20,
							&player->decoder_hdl,
							1);
	vTaskSuspend(player->decoder_hdl);
	int32_t ret = -1, data_len = -1, status = -1, remaining = -1;
	BaseType_t is_chunked = pdFALSE, is_data_read = pdFALSE, is_stopped = pdFALSE;
	xSemaphoreTake(player->semphr, portMAX_DELAY);
	player->state = GETTER_IDLE;
	xSemaphoreGive(player->semphr);
	for (;;) {
		xSemaphoreTake(player->semphr, portMAX_DELAY);
		switch (player->state) {
		case GETTER_IDLE:
			xSemaphoreGive(player->semphr);
			vTaskDelay(pdMS_TO_TICKS(100));
			break;
		case GETTER_STARTING:
			ret = -1, data_len = -1, status = -1, remaining = -1;
			is_chunked = pdFALSE, is_data_read = pdFALSE, is_stopped = pdFALSE;
			memset(player->http_buf, 0, sizeof player->http_buf);
			memset(player->codec_buf, 0, sizeof player->codec_buf);
			xQueueReset(player->queue);
			char *url_buf = heap_caps_calloc(	strlen(app_instance.uri.player) + UUID_STRING_LEN + 1,
												sizeof(char),
												MALLOC_CAP_8BIT);
			if (!url_buf) {
				while (!url_buf) {
					vTaskDelay(1);
					url_buf = heap_caps_calloc(	strlen(app_instance.uri.player) + UUID_STRING_LEN + 1,
												sizeof(char),
												MALLOC_CAP_8BIT);
				}
			}
			char *query_buf = heap_caps_calloc(	UUID_NULL_TERM_STRING_LEN,
												sizeof(char),
												MALLOC_CAP_8BIT);
			if (!query_buf) {
				while (!query_buf) {
					vTaskDelay(1);
					query_buf = heap_caps_calloc(	UUID_NULL_TERM_STRING_LEN,
													sizeof(char),
													MALLOC_CAP_8BIT);
				}
			}
			uuid_to_string(&player->pend_tr_id, query_buf, UUID_NULL_TERM_STRING_LEN);
			strlcat(url_buf, app_instance.uri.player, strlen(app_instance.uri.player));
			strlcat(url_buf, query_buf, strlen(query_buf));
			esp_http_client_config_t client_cfg = {
					.url = url_buf,
					.username = app_instance.device.login,
					.password = app_instance.device.passwd,
					.auth_type = HTTP_AUTH_TYPE_BASIC,
					.method = HTTP_METHOD_GET,
					.event_handler = _http_client_event_handler,
			};
			player->http_getter_client = esp_http_client_init(&client_cfg);
			heap_caps_free(url_buf);
			heap_caps_free(query_buf);
			if ((ret = esp_http_client_open(player->http_getter_client, 0)) != ESP_OK) {
				player->state = GETTER_IDLE;
				break;
			}
			data_len = esp_http_client_fetch_headers(player->http_getter_client);
			status = esp_http_client_get_status_code(player->http_getter_client);
			if (status == HTTP_200) {
				if (data_len < 0) {
					player->state = GETTER_HALT;
					break;
				} else if (data_len == 0) {
					is_chunked = pdTRUE;
				} else if (data_len > 0) {
					remaining = data_len;
				}
				player->state = GETTER_BUFFERING;
			} else {
				if (status == HTTP_406) {
					is_stopped = pdTRUE;
				}
				player->state = GETTER_HALT;
			}
			break;
		case GETTER_BUFFERING:
			if (!is_data_read) {
				ret = esp_http_client_read(	player->http_getter_client,
											(char *)player->http_buf,
											PLAYER_RECV_BUF_SIZE);
				if (ret == 0 && is_chunked == pdTRUE) {
					int32_t null_cnt = 0;
					while (ret == 0) {
						++null_cnt;
						ret = esp_http_client_read(	player->http_getter_client,
													(char *)player->http_buf,
													PLAYER_RECV_BUF_SIZE);
						if (ret != 0 || null_cnt > 9) {
							break;
						}
					}
				}
				if (ret == ESP_FAIL) {
					/* XXX: Just leave this section */
				} else {
					switch (is_chunked) {
					case pdFALSE:
						remaining -= ret;
						xQueueSendToBack(player->queue, player->http_buf, portMAX_DELAY);
						if (remaining == 0) {
							is_data_read = pdTRUE;
						}
						break;
					case pdTRUE:
						if (ret == 0) {
							is_data_read = pdTRUE;
						} else {
							xQueueSendToBack(player->queue, player->http_buf, portMAX_DELAY);
						}
						break;
					default:
						break;
					}
					memset(player->http_buf, 0, sizeof player->http_buf);
					if (	(uxQueueMessagesWaiting(player->queue) >= PLAYER_BUF_CAP_MSG && !is_data_read) ||
							is_data_read) {
						player->state = !is_data_read ? GETTER_ACTIVE : GETTER_STOP_AT_THE_END;
						memset(player->codec_buf, 0, sizeof player->codec_buf);
						vTaskResume(player->decoder_hdl);
					}
				}
			} else {
				player->state = GETTER_STOP_AT_THE_END;
				memset(player->codec_buf, 0, sizeof player->codec_buf);
				vTaskResume(player->decoder_hdl);
			}
			break;
		case GETTER_ACTIVE:
			if (!is_data_read) {
				ret = esp_http_client_read(	player->http_getter_client,
											(char *)player->http_buf,
											PLAYER_RECV_BUF_SIZE);
				if (ret == 0 && is_chunked == pdTRUE) {
					int32_t null_cnt = 0;
					while (ret == 0) {
						++null_cnt;
						ret = esp_http_client_read(	player->http_getter_client,
													(char *)player->http_buf,
													PLAYER_RECV_BUF_SIZE);
						if (ret != 0 || null_cnt > 9) {
							break;
						}
					}
				}
				if (ret == ESP_FAIL) {
					/* XXX: Just leave this section */
				} else {
					switch (is_chunked) {
					case pdFALSE:
						remaining -= ret;
						xQueueSendToBack(player->queue, player->http_buf, portMAX_DELAY);
						if (remaining == 0) {
							is_data_read = pdTRUE;
						}
						break;
					case pdTRUE:
						if (ret == 0) {
							is_data_read = pdTRUE;
						} else {
							xQueueSendToBack(player->queue, player->http_buf, portMAX_DELAY);
						}
						break;
					default:
						break;
					}
					memset(player->http_buf, 0, sizeof player->http_buf);
					if ((uxQueueMessagesWaiting(player->queue) < PLAYER_BUF_CAP_MSG) && !is_data_read) {
						vTaskSuspend(player->decoder_hdl);
						player->state = GETTER_BUFFERING;
					} else if (is_data_read) {
						player->state = GETTER_STOP_AT_THE_END;
					}
				}
			} else {
				player->state = GETTER_STOP_AT_THE_END;
			}
			break;
		case GETTER_PAUSE:
			if (uxQueueSpacesAvailable(player->queue) && !is_data_read) {
				ret = esp_http_client_read(	player->http_getter_client,
											(char *)player->http_buf,
											PLAYER_RECV_BUF_SIZE);
				if (ret == 0 && is_chunked == pdTRUE) {
					int32_t null_cnt = 0;
					while (ret == 0) {
						++null_cnt;
						ret = esp_http_client_read(	player->http_getter_client,
													(char *)player->http_buf,
													PLAYER_RECV_BUF_SIZE);
						if (ret != 0 || null_cnt > 9) {
							break;
						}
					}
				}
				if (ret == ESP_FAIL) {
					/* XXX: Just leave this section */
				} else {
					switch (is_chunked) {
					case pdFALSE:
						remaining -= ret;
						xQueueSendToBack(player->queue, player->http_buf, portMAX_DELAY);
						if (remaining == 0) {
							is_data_read = pdTRUE;
						}
						break;
					case pdTRUE:
						if (ret == 0) {
							is_data_read = pdTRUE;
						} else {
							xQueueSendToBack(player->queue, player->http_buf, portMAX_DELAY);
						}
						break;
					default:
						break;
					}
					memset(player->http_buf, 0, sizeof player->http_buf);
				}
			}
			vTaskDelay(pdMS_TO_TICKS(100));
			break;
		case GETTER_STOP_AT_THE_END:
			if (uxQueueMessagesWaiting(player->queue)) {
				break;
			}
			is_stopped = pdTRUE;
			player->state = GETTER_HALT;
			break;
		case GETTER_HALT:
			vTaskSuspend(player->decoder_hdl);
			esp_http_client_close(player->http_getter_client);
			esp_http_client_cleanup(player->http_getter_client);
			if (is_stopped) {
				app_client_delete_track(player);
			}
			player->state = GETTER_IDLE;
			break;
		default:
			break;
		}
		xSemaphoreGive(player->semphr);
		vTaskDelay(1);
	}
	esp_http_client_cleanup(player->http_getter_client);
	player->getter_hdl = NULL;
	vTaskDelete(NULL);
}

/**
 * @ingroup	app_client_rtos_tasks
 * Feed an audio data to the VS1053b decoder chip
 */
void sound_decoder_task(void *arg) {
	sound_player_t *player = (sound_player_t *)arg;
	for (;;) {
		if (	player->state == GETTER_ACTIVE ||
				player->state == GETTER_STOP_AT_THE_END) {
			if (xQueueReceive(player->queue, player->codec_buf, 0)) {
				vs1053b_play_chunk(player->codec_buf, sizeof player->codec_buf);
				memset(player->codec_buf, 0, sizeof player->codec_buf);
			}
		}
		vTaskDelay(1);
	}
}

static int send_chunk(esp_http_client_handle_t http, const void *buffer, int buffer_len)
{
    char str_buf[16];

	int wlen = sprintf(str_buf, "%x\r\n", buffer_len);
	if (esp_http_client_write(http, str_buf, wlen) <= 0) {
		return ESP_FAIL;
	}
	if (esp_http_client_write(http, buffer, buffer_len) <= 0) {
		return ESP_FAIL;
	}
	if (esp_http_client_write(http, "\r\n", 2) <= 0) {
		return ESP_FAIL;
	}
	return buffer_len;
}

/**
 * @ingroup	app_client_rtos_tasks
 * Send audio recordings to the server
 */
void http_sound_sender_task(void *arg) {
	// BaseType_t is_halted = pdFALSE;
	sound_recorder_t *sampler = (sound_recorder_t *)arg;
	xTaskCreatePinnedToCore(sound_recorder_task,
							"voice_rec",
							2048,
							sampler,
							20,
							&sampler->sampler_hdl,
							1);
	vTaskSuspend(sampler->sampler_hdl);
	int32_t ret = -1, data_len = -1, read_len = -1;
	sampler->wav_hdr.ChunkSize = 36 + QUEUE_MESSAGES_WAITING_THRESHOLD * sizeof sampler->http_buf;
	sampler->wav_hdr.Subchunk2Size = QUEUE_MESSAGES_WAITING_THRESHOLD * sizeof sampler->http_buf;
	esp_http_client_config_t client_cfg = {
			.url = app_instance.uri.sampler,
			//.url = "http://192.168.1.57:8070/teddyserver-rest/webapis/0.1/device/radio",
			.username = app_instance.device.login,
			.password = app_instance.device.passwd,
			.auth_type = HTTP_AUTH_TYPE_BASIC,
			.method = HTTP_METHOD_POST,
			.event_handler = _http_client_event_handler,
	};
	sampler->http_client = esp_http_client_init(&client_cfg);
	esp_http_client_set_header(sampler->http_client, "Connection", "keep-alive");
	esp_http_client_set_header(sampler->http_client, "Content-Type", "audio/wav");
	//esp_http_client_set_header(sampler->http_client, "Content-Type", "text/html");
	xSemaphoreTake(sampler->semphr, portMAX_DELAY);
	sampler->state = SAMPLER_IDLE;
	xSemaphoreGive(sampler->semphr);
	for (;;) {
		xSemaphoreTake(sampler->semphr, portMAX_DELAY);
		switch (sampler->state) {
		case SAMPLER_IDLE:
			xSemaphoreGive(sampler->semphr);
			vTaskDelay(pdMS_TO_TICKS(100));
			break;

		case SAMPLER_STARTING:
			ret = -1, data_len = -1, read_len = -1;
			memset(sampler->rec_buf, 0, sizeof sampler->rec_buf);
			memset(sampler->http_buf, 0, sizeof sampler->http_buf);
			xQueueReset(sampler->queue);
			sampler->state = SAMPLER_ACTIVE;
			vTaskResume(sampler->sampler_hdl);
			break;

		case SAMPLER_ACTIVE:
			//if (uxQueueMessagesWaiting(sampler->queue) >= QUEUE_MESSAGES_WAITING_THRESHOLD) {
				ESP_LOGI("REC", "Opening connection... %s", client_cfg.url);
				// сбросим очередь
				xQueueReset(sampler->queue);
				ret = esp_http_client_open(	sampler->http_client, -1);	// write_len = -1 для потока
//											QUEUE_MESSAGES_WAITING_THRESHOLD * sizeof sampler->http_buf +
//											sizeof sampler->wav_hdr);
				if (ret != ESP_OK) {
					break;
				}

				ESP_LOGI("REC", "Writing wave header...");
				//ret = esp_http_client_write(sampler->http_client, (const char *)&sampler->wav_hdr, sizeof sampler->wav_hdr);
				ret = send_chunk(sampler->http_client, &sampler->wav_hdr, sizeof sampler->wav_hdr);
				if (ret > 0) {
					// пока включена наня - читаем из очереди и отправляем
					while (sampler->state == SAMPLER_ACTIVE)
					{
						// ждём данные до 0,5сек, потом повторяем
						if (xQueueReceive(sampler->queue, sampler->http_buf, pdMS_TO_TICKS(500)) == pdTRUE)
						{
							UBaseType_t qCnt = uxQueueMessagesWaiting(sampler->queue);
							if (qCnt > 6)
							{
								// прпускаем все, что накопилось лишнее
								// буфер 4к, если > 4х буферов в ожидании, это больше 0,5 сек - выкидываем всё
								xQueueReset(sampler->queue);
								ESP_LOGI("REC", "REC - Drop buffers: %d", qCnt);
							}
							xSemaphoreGive(sampler->semphr);
							ESP_LOGI("REC", "WR - %d", qCnt);
							// esp_http_client_write(	sampler->http_client, (const char *)sampler->http_buf, sizeof sampler->http_buf);
							ret = send_chunk(sampler->http_client, &sampler->http_buf, sizeof sampler->http_buf);
							if (ret == ESP_FAIL)
							{
								ESP_LOGI("REC", "ERROR");
							}

							xSemaphoreTake(sampler->semphr, portMAX_DELAY);
							if (sampler->state == SAMPLER_HALT) {
								// is_halted = pdTRUE;
								break;
							}
						}
					}
					/*
					for (int i = 0; i < QUEUE_MESSAGES_WAITING_THRESHOLD; ++i) {
						xQueueReceive(sampler->queue, sampler->http_buf, 0);
						int32_t write_ret = -1;
						while (write_ret == -1) {
							xSemaphoreGive(sampler->semphr);
							ESP_LOGI(tag, "REC - Writing buffer");
							write_ret = esp_http_client_write(	sampler->http_client,
																(const char *)sampler->http_buf,
																sizeof sampler->http_buf);
							xSemaphoreTake(sampler->semphr, portMAX_DELAY);
							if (sampler->state == SAMPLER_HALT) {
								is_halted = pdTRUE;
								break;
							}
						}
						memset(sampler->http_buf, 0, sizeof sampler->http_buf);
						if (is_halted == pdTRUE) {
							is_halted = pdFALSE;
							break;
						}
					}*/
				}
				ESP_LOGI("REC", "Close connection");

				// записываем конец потока
 		       	esp_http_client_write(sampler->http_client, "0\r\n\r\n", 5);
 
				// получаем ответ сервера
				data_len = esp_http_client_fetch_headers(sampler->http_client);
				int status_code = esp_http_client_get_status_code(sampler->http_client);
				ESP_LOGI("REC", "Status Code: %d, content length: %d", status_code, data_len);
				char *buf = malloc(MAX_HTTP_RECV_BUF + 1);
				while (data_len > 0) {
					if ((read_len = esp_http_client_read( sampler->http_client, buf, MIN(data_len, MAX_HTTP_RECV_BUF)) ) <= 0)
					{
						break;
					}
					data_len -= read_len;
				}
				free(buf);

				esp_http_client_close(sampler->http_client);
			//}
			break;

		case SAMPLER_HALT:
			vTaskSuspend(sampler->sampler_hdl);
			esp_http_client_close(sampler->http_client);
			sampler->state = SAMPLER_IDLE;
			break;
		default:
			break;
		}
		xSemaphoreGive(sampler->semphr);
		vTaskDelay(1);
	}
	esp_http_client_cleanup(sampler->http_client);
	sampler->sender_hdl = NULL;
	vTaskDelete(NULL);
}

/**
 * @ingroup	app_client_rtos_tasks
 * Get an output data from the PDM microphone
 */
void sound_recorder_task(void *arg) {
	sound_recorder_t *recorder = (sound_recorder_t *)arg;
	int32_t read_len = -1;
	for (;;) {
		if (recorder->state == SAMPLER_ACTIVE) {
			mp45dt02_take_samples(	(char *)recorder->rec_buf,
									sizeof recorder->rec_buf,
									(size_t *)&read_len,
									portMAX_DELAY);
			xQueueSendToBack(recorder->queue, recorder->rec_buf, 0);
			memset(recorder->rec_buf, 0, sizeof recorder->rec_buf);
		}
		vTaskDelay(1);
	}
}
