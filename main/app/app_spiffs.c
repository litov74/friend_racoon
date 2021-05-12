/**
 * *****************************************************************************
 * @file		app_spiffs.c
 * @author		S. Naumov
 * *****************************************************************************
 * @brief		Module for working with files
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/

/* STDLIB */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Framework */
#include <esp_err.h>
#include <esp_log.h>
#include <esp_spiffs.h>

/* User files */
#include "app_spiffs.h"

/* Private constants ---------------------------------------------------------*/

static const char *tag = "app_spiffs";
static const char *temp_file = "/spiffs/temp.csv";

/* Export functions ----------------------------------------------------------*/

/* Initialize SPIFFS */
esp_err_t app_spiffs_init(void) {
	ESP_LOGD(tag, "Initializing SPIFFS");
	esp_vfs_spiffs_conf_t conf = {
			.base_path = "/spiffs",
			.partition_label = NULL,
			.max_files = 5,	/* This decides the maximum number of files that can be created on the storage */
			.format_if_mount_failed = true,
	};
	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGD(tag, "Failed to mount or format SPIFFS file system");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGD(tag, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGD(tag, "Failed to initialize SPIFFS: %s", esp_err_to_name(ret));
		}
		return ESP_FAIL;
	}
	size_t total = 0, used = 0;
	ret = esp_spiffs_info(NULL, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGD(tag, "Failed to get SPIFFS partition information: %s", esp_err_to_name(ret));
		return ESP_FAIL;
	}
	ESP_LOGD(tag, "SPIFFS partition size: total = %d, used = %d", total, used);
	return ESP_OK;
}

/* Detach SPIFFS */
esp_err_t app_spiffs_deinit(const char *partition_label) {
	if (esp_vfs_spiffs_unregister(partition_label) != ESP_OK) {
		return ESP_FAIL;
	}
	ESP_LOGD(tag, "SPIFFS unmounted");
	return ESP_OK;
}

/* Create a file */
esp_err_t app_spiffs_create_file(const char *filename) {
	/* Check if destination file exists before reading */
	struct stat st;
	if (stat(filename, &st) == 0) {
		return ESP_ERR_INVALID_ARG;
	} else {
		FILE *f = fopen(filename, "w");
		if (f == NULL) {
			ESP_LOGD(tag, "Failed to create a file");
			return ESP_FAIL;
		}
		fclose(f);
		ESP_LOGD(tag, "File created");
	}
	return ESP_OK;
}

/* Clear the contents of a file */
esp_err_t app_spiffs_erase_file(const char *filename) {
	FILE *f = fopen(filename, "w");
	if (f == NULL) {
		ESP_LOGD(tag, "Failed to clear file");
		return ESP_FAIL;
	}
	fclose(f);
	ESP_LOGD(tag, "File cleared");
	return ESP_OK;
}

/* Count the number of lines in a document */
int app_spiffs_get_lines_num(const char *filename) {
	/* Check if destination file exists before reading */
	struct stat st;
	if (stat(filename, &st) == 0) {
		int cnt = 0;	/* Line counter (result) */
		int ch = 0;	/* To store a character read from file */
		/* Open requested file for reading */
		ESP_LOGD(tag, "Count lines");
		FILE *f = fopen(filename, "r");
		if (f == NULL) {
			ESP_LOGD(tag, "Failed to open file for reading");
			return ESP_FAIL;
		}
		/* Extract characters from file and store in character variable */
		for (ch = getc(f); ch != EOF; ch = getc(f)) {
			if (ch == '\n') {
				cnt += 1;	/* Increment count if this character is newline */
			}
		}
		/* Close the file */
		fclose(f);
		if (cnt > 0) {
			ESP_LOGD(tag, "Number of lines that file contains = %d", cnt);
			return cnt;
		}
	} else {
		return ESP_FAIL;
	}
	return ESP_OK;
}

/* Read existing records */
esp_err_t app_spiffs_read_records(const char *filename, int *ap_num, app_spiffs_ap_record_t *ap_records) {
	/* Check if destination file exists before reading */
	struct stat st;
	if (stat(filename, &st) == 0) {
		char *line_err;
		char line[SPIFFS_WIFI_SSID_LENGTH + SPIFFS_WIFI_PASSWORD_LENGTH + (4 * strlen("\"") + strlen(",") + strlen("\r\n"))];
		uint8_t line_ssid[SPIFFS_WIFI_SSID_LENGTH + 1];
		/* Open requested file for reading */
		ESP_LOGD(tag, "Reading records");
		FILE *f = fopen(filename, "r");
		if (f == NULL) {
			ESP_LOGD(tag, "Failed to open file for reading");
			return ESP_FAIL;
		}
		for (int i = 0; i < *ap_num; i++) {
			line_err = fgets(line, sizeof(line), f);
			if (line_err == NULL) {
				if (feof(f) != 0) {
					ESP_LOGD(tag, "File was read");
					return ESP_FAIL;
				} else {
					ESP_LOGD(tag, "File reading error");
					return ESP_FAIL;
				}
			}
			/* Parse */
			char *pos = strchr(line, '\"');
			if (pos++) {
				char ch = *pos++;
				memset(line_ssid, 0, (SPIFFS_WIFI_SSID_LENGTH + 1) * sizeof(uint8_t));
				while (ch != '\"') {
					strncat((char*)line_ssid, &ch, 1);
					ch = *pos++;
				}
				strncpy((char *)ap_records[i].ssid, (const char *)line_ssid, strlen((const char *)line_ssid));
			}
		}
		fclose(f);
	} else {
		return ESP_ERR_NOT_FOUND;
	}
	return ESP_OK;
}

/**
 * @brief		Write line to a file
 * @param[in]	file		Target file
 * @param[in]	ssid		SSID of target AP
 * @param[in]	password	Password of target AP
 * @return
 * 				- ESP_ERR_NO_MEM: Memory allocation failure
 * 				- ESP_OK: Success
 */
static int _write_line(FILE *file, const char *ssid, const char *password) {
	char *buf = calloc(SPIFFS_WIFI_SSID_LENGTH + SPIFFS_WIFI_PASSWORD_LENGTH + (4 * strlen("\"") + strlen(",") + strlen("\r\n")) + 1, sizeof(char));
	if (!buf) {
		return ESP_ERR_NO_MEM;
	}
	strcat(buf, "\"");
	strncat(buf, ssid, strlen(ssid));
	strcat(buf, "\",\"");
	strncat(buf, password, strlen(password));
	strcat(buf, "\"\r\n");
	fprintf(file, buf);
	ESP_LOGD(tag, "Written line: %s", buf);
	free(buf);
	return ESP_OK;
}

/* Insert new record */
esp_err_t app_spiffs_insert_record(	const char *filename,
									int *ap_num,
									const char *ssid,
									const char *pass) {
	/* Check if destination file exists before reading */
	struct stat st;
	if (stat(filename, &st) == 0) {
		char *line_err;
		char line[SPIFFS_WIFI_SSID_LENGTH + SPIFFS_WIFI_PASSWORD_LENGTH + (4 * strlen("\"") + strlen(",") + strlen("\r\n"))];
		uint8_t line_ssid[SPIFFS_WIFI_SSID_LENGTH + 1];
		/* Open requested file for reading */
		ESP_LOGD(tag, "Adding record");
		FILE *f = fopen(filename, "r");
		if (f == NULL) {
			ESP_LOGD(tag, "Failed to open file for reading");
			return ESP_FAIL;
		}
		/* Create a temporary file */
		ESP_LOGD(tag, "Creating temporary file");
		FILE *tmp = fopen(temp_file, "w");
		if (tmp == NULL) {
			ESP_LOGD(tag, "Failed to open file for writing");
			return ESP_FAIL;
		}
		bool exist = false;
		for (int i = 0; i < *ap_num; i++) {
			line_err = fgets(line, sizeof(line), f);
			if (line_err == NULL) {
				if (feof(f) != 0) {
					ESP_LOGD(tag, "File was read");
					return ESP_FAIL;
				} else {
					ESP_LOGD(tag, "File reading error");
					return ESP_FAIL;
				}
			}
			/* Parse */
			char *pos = strchr(line, '\"');
			if (pos++) {
				char ch = *pos++;
				memset(line_ssid, 0, (SPIFFS_WIFI_SSID_LENGTH + 1) * sizeof(uint8_t));
				while (ch != '\"') {
					strncat((char *)line_ssid, &ch, 1);
					ch = *pos++;
				}
				if (!strcmp((char *)line_ssid, ssid)) {
					exist = true;
					ESP_LOGD(tag, "Access point using specified SSID already exists!");
					if (_write_line(tmp, ssid, pass) != ESP_OK) {
						return ESP_ERR_NO_MEM;
					}
				} else {
					fprintf(tmp, line);
				}
			}
		}
		if (!exist) {
			ESP_LOGD(tag, "New access point");
			if (_write_line(tmp, ssid, pass) != ESP_OK) {
				return ESP_ERR_NO_MEM;
			}
		}
		fclose(tmp);
		fclose(f);
		if (remove(filename) != 0) {
			return ESP_FAIL;
		}
		if (rename(temp_file, filename) != 0) {
			return ESP_FAIL;
		}
	} else {
		return ESP_ERR_NOT_FOUND;
	}
	return ESP_OK;
}

/* Get a password by the specified line index */
esp_err_t app_spiffs_get_password(const char *filename, uint16_t *idx, app_spiffs_ap_record_t *ap_records) {
	/* Check if destination file exists before reading */
	struct stat st;
	if (stat(filename, &st) == 0) {
		char *line_err;
		char line[SPIFFS_WIFI_SSID_LENGTH + SPIFFS_WIFI_PASSWORD_LENGTH + (4 * strlen("\"") + strlen(",") + strlen("\r\n"))];
		uint8_t line_password[SPIFFS_WIFI_PASSWORD_LENGTH + 1];
		/* Open requested file for reading */
		ESP_LOGD(tag, "Extracting password");
		FILE *f = fopen(filename, "r");
		if (f == NULL) {
			ESP_LOGD(tag, "Failed to open file for reading");
			return ESP_FAIL;
		}
		if (*idx > 0) {
			for (int i = 0; i < *idx; ++i) {
				line_err = fgets(line, sizeof(line), f);
				if (line_err == NULL) {
					if (feof(f) == 0) {
						ESP_LOGD(tag, "File reading error");
						return ESP_FAIL;
					}
				}
			}
		}
		line_err = fgets(line, sizeof(line), f);
		if (line_err == NULL) {
			if (feof(f) == 0) {
				ESP_LOGD(tag, "File reading error");
				return ESP_FAIL;
			}
		}
		/* Strip newline */
		char *pos = strchr(line, '\n');
		if (pos) {
			*pos = '\0';
		}
		ESP_LOGD(tag, "Parse line: %s", line);
		/* Parse */
		pos = strchr(line, ',');
		pos++;
		if (pos++) {
			char ch = *pos++;
			memset(line_password, 0, (SPIFFS_WIFI_PASSWORD_LENGTH + 1) * sizeof(uint8_t));
			while (ch != '\"') {
				strncat((char*)line_password, &ch, 1);
				ch = *pos++;
			}
			strncpy((char *)ap_records[*idx].password, (const char *)line_password, strlen((const char *)line_password));
		}
		fclose(f);
	} else {
		return ESP_ERR_NOT_FOUND;
	}
	return ESP_OK;
}
