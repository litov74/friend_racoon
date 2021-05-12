/**
 * *****************************************************************************
 * @file		app_client_utils.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Auxiliary functions that are used to perform certain actions based on
 * 				the values ​​of the device profile parameters
 *
 * *****************************************************************************
 */

#define LOG_LOCAL_LEVEL		ESP_LOG_DEBUG

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <string.h>
#include <sys/param.h>

/* Framework */
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <cJSON.h>

/* User files */
#include "app.h"
#include "app_client.h"
#include "board_def.h"
#include "uuid.h"
#include "vs1053b.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_client";
static bool firstShowProfile = true;

/* Export functions ----------------------------------------------------------*/

/**
 * @ingroup	app_client_utils
 * Get JSON string that contains current device profile keys values
 */
esp_err_t app_client_get_device_profile(esp_http_client_handle_t cli_hdl, app_client_profile_t *profile) {
	int32_t ret = -1, data_len = -1, status = -1, read_len = -1;
	ret = esp_http_client_open(cli_hdl, 0);
	if (ret != ESP_OK) {
		return ESP_FAIL;
	}
	data_len = esp_http_client_fetch_headers(cli_hdl);
	status = esp_http_client_get_status_code(cli_hdl);
	if (status == HTTP_200) {
		if (data_len <= 0) {
			esp_http_client_close(cli_hdl);
			return ESP_FAIL;
		} else if (data_len > 0) {
			char *buf = malloc(MAX_HTTP_RECV_BUF + 1);
			while (data_len > 0) {
				if ((read_len = esp_http_client_read(	cli_hdl,
														buf,
														MIN(data_len, MAX_HTTP_RECV_BUF))) <= 0) {
					break;
				}
				data_len -= read_len;
			}
			if (app_client_parse_profile(profile, (const char * const)buf) == ESP_FAIL) {
				esp_http_client_close(cli_hdl);
				free(buf);
				return ESP_ERR_NOT_FOUND;
			}
			esp_http_client_close(cli_hdl);
			free(buf);
		}
	} else {
		ESP_LOGD(tag, "HTTP response status code is invalid = %d", status);
		esp_http_client_close(cli_hdl);
		if (status == HTTP_401) {
			return ESP_ERR_INVALID_STATE;
		} else {
			return ESP_FAIL;
		}
	}
	return ESP_OK;
}

/**
 * @ingroup	app_client_utils
 * Parse JSON string that contains current device profile keys values
 */
esp_err_t app_client_parse_profile(app_client_profile_t *profile, const char * const content) {
	BaseType_t does_trid_exist = pdFALSE;
	cJSON *root = cJSON_Parse(content);
	char *key1 = NULL, *key2 = NULL, *key3 = NULL;
	cJSON *obj1 = cJSON_GetObjectItem(root, "id");
	if (obj1) {
		key1 = obj1->valuestring;
		if (key1) {
			strlcpy(profile->id, key1, sizeof profile->id);
		} else {
			return ESP_FAIL;
		}
	} else {
		return ESP_FAIL;
	}
	cJSON *obj2 = cJSON_GetObjectItem(root, "name");
	if (obj2) {
		key2 = obj2->valuestring;
		if (key2) {
			strlcpy(profile->name, key2, sizeof profile->name);
		} else {
			return ESP_FAIL;
		}
	} else {
		return ESP_FAIL;
	}
	cJSON *obj3 = cJSON_GetObjectItem(root, "currentVoiceCommandId");
	if (obj3) {
		key3 = obj3->valuestring;
		if (key3) {
			if (uuid_parse((const char *)key3, &profile->track_id) == ESP_OK) {
				does_trid_exist = pdTRUE;
			}
		}
	}
	profile->is_muted = cJSON_GetObjectItem(root, "mute")->valueint;
	profile->is_player = cJSON_GetObjectItem(root, "playerActive")->valueint;
	profile->is_recorder = cJSON_GetObjectItem(root, "radioActive")->valueint;
	profile->track_cnt = cJSON_GetObjectItem(root, "soundCnt")->valuedouble;
	profile->vol = cJSON_GetObjectItem(root, "volume")->valuedouble;
	cJSON_Delete(root);
	char *tmpstr = malloc(UUID_NULL_TERM_STRING_LEN);
	if (does_trid_exist == pdTRUE) {
		if (uuid_to_string(&profile->track_id, tmpstr, UUID_NULL_TERM_STRING_LEN) != ESP_OK) {
			return ESP_FAIL;
		}
	} else {
		memset(&profile->track_id.b, 0, sizeof profile->track_id.b);
	}

	if (firstShowProfile)
	{
		firstShowProfile = false;
		ESP_LOGD(tag,	"current device profile:\n"
						"device_id=%s;\n"
						"device_name=%s;\n"
						"mute_state=%d;\n"
						"player_state=%d;\n"
						"vol_level=%.0f;\n"
						"sampler_state=%d;\n"
						"track_cnt=%.0f;\n"
						"track_id=%s;\n",
						profile->id,
						profile->name,
						profile->is_muted,
						profile->is_player,
						profile->vol,
						profile->is_recorder,
						profile->track_cnt,
						does_trid_exist == pdFALSE ? "" : (const char *)tmpstr);
	}
	else
	{
		ESP_LOGD(tag,	"profile: m=%d p=%d v=%.0f s=%d tcnt=%.0f tid=%s\n",
						profile->is_muted,
						profile->is_player,
						profile->vol,
						profile->is_recorder,
						profile->track_cnt,
						does_trid_exist == pdFALSE ? "" : (const char *)tmpstr);
	}
	
	free(tmpstr);
	return ESP_OK;
}

/**
 * @ingroup	app_client_utils
 * Change the state of the player in accordance with the current
 * values ​​of the profile keys
 */
void app_client_set_player_state(sound_player_t *player, app_client_profile_t *profile) {
	/* Sound player mute-mode control node */
	if (profile->is_muted != player->is_muted) {
		if (profile->is_muted) {
			gpio_set_level(PIN_NUM_AMP_XMUTE, 0);
		} else {
			gpio_set_level(PIN_NUM_AMP_XMUTE, 1);
		}
		player->is_muted = profile->is_muted;
	}
	/* Sound player volume control node */
	if (profile->vol != player->vol) {
		vs1053b_set_volume(profile->vol);
		player->vol = profile->vol;
	}
	/* Sound player state control node */
	if (memcmp(player->pend_tr_id.b, profile->track_id.b, UUID_SIZE) != 0) {
		if (	player->state != GETTER_IDLE &&
				player->state != GETTER_HALT) {
			player->state = GETTER_HALT;
		}
	} else {
		if (profile->is_player && profile->track_cnt) {
			if (player->state == GETTER_IDLE) {
				player->state = GETTER_STARTING;
			} else if (player->state == GETTER_PAUSE) {
				player->state = GETTER_ACTIVE;
			}
		} else if (!profile->is_player && profile->track_cnt) {
			if (	player->state == GETTER_BUFFERING ||
					player->state == GETTER_ACTIVE ||
					player->state == GETTER_STOP_AT_THE_END) {
				player->state = GETTER_PAUSE;
			}
		}
	}
	player->pend_tr_cnt = profile->track_cnt;
	memset(player->pend_tr_id.b, 0, sizeof player->pend_tr_id.b);
	memcpy(player->pend_tr_id.b, profile->track_id.b, UUID_SIZE);
}

/**
 * @ingroup	app_client_utils
 * Change the state of the recorder in accordance with the current
 * values ​​of the profile keys
 */
void app_client_set_sampler_state(sound_recorder_t *sampler, app_client_profile_t *profile) {
	if (profile->is_recorder) {
		if (sampler->state == SAMPLER_IDLE) {
			sampler->state = SAMPLER_STARTING;
		}
	} else {
		if (	sampler->state != SAMPLER_IDLE &&
				sampler->state != SAMPLER_HALT) {
			sampler->state = SAMPLER_HALT;
		}
	}
}

/**
 * @ingroup	app_client_utils
 * Suspend execution of all current media tasks
 */
void app_client_halt_media_tasks(void *arg) {
	app_client_func_t *client = (app_client_func_t *)arg;
	/* Stop the player */
	xSemaphoreTake(client->player.semphr, portMAX_DELAY);
	if (	client->player.state != GETTER_IDLE &&
			client->player.state != GETTER_HALT) {
		client->player.state = GETTER_HALT;
	}
	xSemaphoreGive(client->player.semphr);
	while (client->player.state != GETTER_IDLE) {
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	/* Stop the sampler */
	xSemaphoreTake(client->sampler.semphr, portMAX_DELAY);
	if (	client->sampler.state != SAMPLER_IDLE &&
			client->sampler.state != SAMPLER_HALT) {
		client->sampler.state = SAMPLER_HALT;
	}
	xSemaphoreGive(client->sampler.semphr);
	while (client->sampler.state != SAMPLER_IDLE) {
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	/* Turn the LED off */
	gpio_set_level(PIN_NUM_USER_LED, 0);
	client->led_tracker = pdFALSE;
}
