#include "wifi.h"
#include "config_handler.h"
#include "protocol.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI_HANDLER";

typedef enum {
    W_IDLE, W_CONNECTING, W_WRONG_PASSWORD, W_NO_AP_FOUND, W_CONNECT_FAIL, W_CONNECTED
} wifi_status_t;

static wifi_status_t current_wifi_status = W_IDLE;
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void sync_time(void) {
    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    time(&now);
    localtime_r(&now, &timeinfo);

    // Check if time is actually updated (year > 2020)
    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "Failed to sync time. HTTPS requests will fail.");
    } else {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Time successfully synchronized: %s", strftime_buf);
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        char *ssid = get_config_string(CONFIG_KEY_NETWORK_SSID);
        int has_ssid = (ssid != NULL && strlen(ssid) > 0);
        free(ssid);
        if (has_ssid) {
            current_wifi_status = W_CONNECTING;
            esp_wifi_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        if (event->reason == WIFI_REASON_NO_AP_FOUND) current_wifi_status = W_NO_AP_FOUND;
        else if (event->reason == WIFI_REASON_AUTH_EXPIRE || event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || event->reason == WIFI_REASON_802_1X_AUTH_FAILED || event->reason == WIFI_REASON_AUTH_FAIL) {
            current_wifi_status = W_WRONG_PASSWORD;
        } else {
            current_wifi_status = W_CONNECT_FAIL;
        }
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        current_wifi_status = W_CONNECTED;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);

        sync_time(); // Sync time immediately upon successful connection to ensure HTTPS works
    }
}

void wifi_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = { .sta = { .threshold = { .authmode = WIFI_AUTH_WPA2_PSK } } };
    char *ssid = get_config_string(CONFIG_KEY_NETWORK_SSID);
    char *password = get_config_string(CONFIG_KEY_NETWORK_PASSWORD);
    if (ssid && strlen(ssid) > 0) {
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        if (password) {
            strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        }
    }
    free(ssid);
    free(password);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

response_t handle_wifi_command(uint8_t cmd, uint8_t *payload, uint16_t len) {
    if (cmd == CMD_WIFI_SCAN) {
        esp_wifi_scan_start(NULL, true);
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        uint16_t max_aps = (ap_count > 8) ? 8 : ap_count;
        wifi_ap_record_t *ap_records = malloc(max_aps * sizeof(wifi_ap_record_t));
        esp_wifi_scan_get_ap_records(&max_aps, ap_records);

        // Raw format: [status][count] then per-AP: ssid\0, rssi(int8), authmode(uint8)
        size_t cap = 2;
        for (int i = 0; i < max_aps; i++) {
            cap += strlen((const char*)ap_records[i].ssid) + 1 + 2;
        }
        uint8_t *buf = malloc(cap);
        size_t off = 0;
        buf[off++] = RESP_OK;
        buf[off++] = (uint8_t)max_aps;
        for (int i = 0; i < max_aps; i++) {
            size_t slen = strlen((const char*)ap_records[i].ssid);
            memcpy(buf + off, ap_records[i].ssid, slen);
            off += slen;
            buf[off++] = '\0';
            buf[off++] = (uint8_t)ap_records[i].rssi;
            buf[off++] = (uint8_t)ap_records[i].authmode;
        }
        free(ap_records);

        response_t r = { .data = buf, .len = (uint16_t)off };
        return r;
    }

    if (cmd == CMD_WIFI_STATUS) {
        if (current_wifi_status == W_CONNECTED) {
            wifi_config_t cfg;
            esp_wifi_get_config(WIFI_IF_STA, &cfg);
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);

            // Raw format: [status][ssid\0][ip(4 bytes)]
            size_t slen = strlen((const char*)cfg.sta.ssid);
            uint8_t *buf = malloc(1 + slen + 1 + 4);
            size_t off = 0;
            buf[off++] = RESP_OK;
            memcpy(buf + off, cfg.sta.ssid, slen);
            off += slen;
            buf[off++] = '\0';
            memcpy(buf + off, &ip_info.ip.addr, 4);
            off += 4;

            response_t r = { .data = buf, .len = (uint16_t)off };
            return r;
        }

        switch (current_wifi_status) {
            case W_IDLE:            return response_status(RESP_WIFI_IDLE);
            case W_CONNECTING:      return response_status(RESP_WIFI_CONNECTING);
            case W_WRONG_PASSWORD:  return response_status(RESP_ERROR_AUTH_FAILED);
            case W_NO_AP_FOUND:     return response_status(RESP_ERROR_AP_NOT_FOUND);
            case W_CONNECT_FAIL:    return response_status(RESP_ERROR_CONNECT_FAILED);
            default:                return response_status(RESP_ERROR_UNKNOWN_COMMAND);
        }
    }

    if (cmd == CMD_WIFI_CONNECT) {
        if (!payload || len == 0) return response_status(RESP_ERROR_INVALID_PARAMS);

        // Payload: ssid\0password (raw bytes, no strlen on the whole buffer)
        uint8_t *sep = memchr(payload, '\0', len);
        if (!sep || (size_t)(sep - payload) + 1 >= len) return response_status(RESP_ERROR_INVALID_PARAMS);
        size_t ssid_len = (size_t)(sep - payload);
        char *password = (char*)(sep + 1);

        wifi_config_t wifi_config = {0};
        memcpy(wifi_config.sta.ssid, payload, ssid_len < sizeof(wifi_config.sta.ssid) ? ssid_len : sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password)-1);

        esp_wifi_disconnect();
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        current_wifi_status = W_CONNECTING;
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
        esp_wifi_connect();

        EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));

        if (bits & WIFI_CONNECTED_BIT) return response_status(RESP_OK);
        if (current_wifi_status == W_NO_AP_FOUND) return response_status(RESP_ERROR_AP_NOT_FOUND);
        if (current_wifi_status == W_WRONG_PASSWORD) return response_status(RESP_ERROR_AUTH_FAILED);
        if (bits & WIFI_FAIL_BIT) return response_status(RESP_ERROR_CONNECT_FAILED);
        return response_status(RESP_ERROR_CONNECT_TIMEOUT);
    }

    return response_status(RESP_ERROR_UNKNOWN_COMMAND);
}