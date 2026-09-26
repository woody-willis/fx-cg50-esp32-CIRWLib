#include "config_handler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "CONFIG_HANDLER";
static const char *NVS_NAMESPACE = "sys_config";

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

char* get_config_value(const int key) {
    if (key < 0 || key >= CONFIG_KEY_COUNT) {
        return strdup("ERROR_INVALID_KEY");
    }

    const config_metadata_t *meta = &config_table[key];
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);

    if (meta->type == TYPE_STRING) {
        size_t required_size = 0;
        // Check if data exists and find buffer size
        if (err == ESP_OK && nvs_get_str(nvs_handle, meta->nvs_key, NULL, &required_size) == ESP_OK) {
            char *buf = malloc(required_size);
            if (buf && nvs_get_str(nvs_handle, meta->nvs_key, buf, &required_size) == ESP_OK) {
                nvs_close(nvs_handle);
                return buf;
            }
            free(buf);
        }
        nvs_close(nvs_handle);
        return strdup(meta->def_str); // Fallback to default
    } 
    else { // TYPE_INT
        int32_t val = meta->def_int;
        if (err == ESP_OK) {
            nvs_get_i32(nvs_handle, meta->nvs_key, &val);
        }
        nvs_close(nvs_handle);

        // Convert Integer to string to match function signature
        char *buf = malloc(16);
        if (buf) {
            snprintf(buf, 16, "%ld", (long)val);
        }
        return buf;
    }
}

char* set_config_value(const int key, const char *value) {
    if (key < 0 || key >= CONFIG_KEY_COUNT || value == NULL) {
        return strdup("ERROR_INVALID_PARAMS");
    }

    const config_metadata_t *meta = &config_table[key];
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return strdup("ERROR_NVS_OPEN_FAIL");
    }

    if (meta->type == TYPE_STRING) {
        err = nvs_set_str(nvs_handle, meta->nvs_key, value);
    } else {
        int32_t val = atoi(value);
        err = nvs_set_i32(nvs_handle, meta->nvs_key, val);
    }

    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        return strdup("OK");
    }

    nvs_close(nvs_handle);
    return strdup("ERROR_WRITE_FAIL");
}

char* handle_config_command(char *payload, uint16_t len) {
    if (!payload || len < 5) return strdup("ERROR_BAD_REQUEST");

    if (strncmp(payload, "GET:", 4) == 0) {
        int key = atoi(payload + 4);
        char *val = get_config_value(key);
        
        char *response = malloc(strlen(val) + 16);
        sprintf(response, "VALUE:%s", val);
        free(val);
        return response;
    } 
    else if (strncmp(payload, "SET:", 4) == 0) {
        char *key_str = payload + 4;
        char *colon = strchr(key_str, ':');
        if (!colon) return strdup("ERROR_SYNTAX");

        *colon = '\0'; // Split the string
        int key = atoi(key_str);
        char *value_str = colon + 1;

        return set_config_value(key, value_str);
    }

    return strdup("ERROR_UNKNOWN_CONFIG_CMD");
}