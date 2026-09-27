#include "config_handler.h"
#include "protocol.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"

static const char *NVS_NAMESPACE = "cirw";

typedef enum {
    TYPE_STRING,
    TYPE_INT
} config_type_t;

// Structure to define metadata for each config item
typedef struct {
    const char *nvs_key;      // NVS keys must be <= 15 characters
    config_type_t type;
    const char *def_str;
    int32_t def_int;
} config_metadata_t;

// Metadata table mapping keys cleanly to their configurations
static const config_metadata_t config_table[CONFIG_KEY_COUNT] = {
    [CONFIG_KEY_OPENAI_API_KEY]   = { "oa_key",    TYPE_STRING, DEFAULT_OPENAI_API_KEY,      0 },
    [CONFIG_KEY_OPENAI_API_URL]   = { "oa_url",    TYPE_STRING, DEFAULT_OPENAI_API_URL,      0 },
    [CONFIG_KEY_NETWORK_SSID]     = { "wf_ssid",   TYPE_STRING, DEFAULT_NETWORK_SSID,        0 },
    [CONFIG_KEY_NETWORK_PASSWORD] = { "wf_pass",   TYPE_STRING, DEFAULT_NETWORK_PASSWORD,    0 },
    [CONFIG_KEY_UART_PORT_NUM]    = { "u_port",    TYPE_INT,    NULL,                        DEFAULT_UART_PORT_NUM },
    [CONFIG_KEY_UART_BAUD_RATE]   = { "u_baud",    TYPE_INT,    NULL,                        DEFAULT_UART_BAUD_RATE },
    [CONFIG_KEY_UART_TX_PIN]      = { "u_tx",      TYPE_INT,    NULL,                        DEFAULT_UART_TX_PIN },
    [CONFIG_KEY_UART_TX_BUF_SIZE] = { "u_tx_buf",  TYPE_INT,    NULL,                        DEFAULT_UART_TX_BUF_SIZE },
    [CONFIG_KEY_UART_RX_PIN]      = { "u_rx",      TYPE_INT,    NULL,                        DEFAULT_UART_RX_PIN },
    [CONFIG_KEY_UART_RX_BUF_SIZE] = { "u_rx_buf",  TYPE_INT,    NULL,                        DEFAULT_UART_RX_BUF_SIZE }
};

void config_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

// Returns the raw value bytes for a config key, or NULL if the key is invalid.
// Caller must free the returned buffer.
uint8_t* get_config_value(const int key, uint16_t *out_len) {
    if (key < 0 || key >= CONFIG_KEY_COUNT) {
        return NULL;
    }

    const config_metadata_t *meta = &config_table[key];
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);

    if (meta->type == TYPE_STRING) {
        size_t required_size = 0;
        // Check if data exists and find buffer size
        if (err == ESP_OK && nvs_get_str(nvs_handle, meta->nvs_key, NULL, &required_size) == ESP_OK) {
            uint8_t *buf = malloc(required_size);
            if (buf && nvs_get_str(nvs_handle, meta->nvs_key, (char*)buf, &required_size) == ESP_OK) {
                nvs_close(nvs_handle);
                if (out_len) *out_len = (uint16_t)(required_size - 1); // exclude trailing \0
                return buf;
            }
            free(buf);
        }
        nvs_close(nvs_handle);
        // Fallback to default
        size_t dlen = strlen(meta->def_str);
        uint8_t *dbuf = malloc(dlen);
        if (dbuf) memcpy(dbuf, meta->def_str, dlen);
        if (out_len) *out_len = (uint16_t)dlen;
        return dbuf;
    }
    else { // TYPE_INT
        int32_t val = meta->def_int;
        if (err == ESP_OK) {
            nvs_get_i32(nvs_handle, meta->nvs_key, &val);
        }
        nvs_close(nvs_handle);

        // Return the integer as raw 4 bytes (little-endian)
        uint8_t *buf = malloc(4);
        if (buf) {
            buf[0] = (uint8_t)(val & 0xFF);
            buf[1] = (uint8_t)((val >> 8) & 0xFF);
            buf[2] = (uint8_t)((val >> 16) & 0xFF);
            buf[3] = (uint8_t)((val >> 24) & 0xFF);
        }
        if (out_len) *out_len = 4;
        return buf;
    }
}

// Returns RESP_OK on success, or an error status code.
resp_status_t set_config_value(const int key, const uint8_t *value, uint16_t len) {
    if (key < 0 || key >= CONFIG_KEY_COUNT || value == NULL) {
        return RESP_ERROR_INVALID_PARAMS;
    }

    const config_metadata_t *meta = &config_table[key];
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return RESP_ERROR_UNKNOWN;
    }

    if (meta->type == TYPE_STRING) {
        // NVS strings must be null-terminated; copy into a temp buffer
        char *str = malloc(len + 1);
        if (!str) {
            nvs_close(nvs_handle);
            return RESP_ERROR_UNKNOWN;
        }
        memcpy(str, value, len);
        str[len] = '\0';
        err = nvs_set_str(nvs_handle, meta->nvs_key, str);
        free(str);
    } else {
        // Interpret the raw 4 bytes as a little-endian int32
        int32_t val = 0;
        if (len >= 4) {
            val = (int32_t)(value[0] | (value[1] << 8) | (value[2] << 16) | ((uint32_t)value[3] << 24));
        } else {
            val = atoi((const char*)value); // fallback for short payloads
        }
        err = nvs_set_i32(nvs_handle, meta->nvs_key, val);
    }

    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        return RESP_OK;
    }

    nvs_close(nvs_handle);
    return RESP_ERROR_UNKNOWN;
}

response_t handle_config_command(uint8_t cmd, uint8_t *payload, uint16_t len) {
    if (!payload || len < 1) return response_status(RESP_ERROR_INVALID_PARAMS);

    if (cmd == CMD_CONFIG_GET) {
        // Payload: single byte key index
        int key = payload[0];
        uint16_t val_len = 0;
        uint8_t *val = get_config_value(key, &val_len);
        if (!val) return response_status(RESP_CONFIG_NOT_FOUND);

        response_t r = { .data = val, .len = val_len };
        return r;
    }

    if (cmd == CMD_CONFIG_SET) {
        // Payload: [key(1 byte)][value bytes]
        int key = payload[0];
        resp_status_t status = set_config_value(key, payload + 1, len - 1);
        return response_status(status);
    }

    return response_status(RESP_ERROR_UNKNOWN_COMMAND);
}

// Returns a null-terminated copy of the string config value, or NULL on failure.
// Caller must free the returned buffer.
char* get_config_string(const int key) {
    uint16_t len = 0;
    uint8_t *raw = get_config_value(key, &len);
    if (!raw) return NULL;

    char *str = malloc(len + 1);
    if (str) {
        memcpy(str, raw, len);
        str[len] = '\0';
    }
    free(raw);
    return str;
}

// Returns the integer config value (0 if the key is invalid).
int32_t get_config_int(const int key) {
    uint16_t len = 0;
    uint8_t *raw = get_config_value(key, &len);
    if (!raw) return 0;

    int32_t val = 0;
    if (len >= 4) {
        val = (int32_t)(raw[0] | (raw[1] << 8) | (raw[2] << 16) | ((uint32_t)raw[3] << 24));
    }
    free(raw);
    return val;
}