#include "http.h"
#include "protocol.h"
#include <string.h>
#include <stdlib.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

typedef struct { char *buffer; int len; } http_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_buffer_t *buf = (http_buffer_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (!esp_http_client_is_chunked_response(evt->client) || evt->data_len > 0) {
            char *new_buf = realloc(buf->buffer, buf->len + evt->data_len + 1);
            if (new_buf) {
                buf->buffer = new_buf;
                memcpy(buf->buffer + buf->len, evt->data, evt->data_len);
                buf->len += evt->data_len;
                buf->buffer[buf->len] = '\0';
            }
        }
    }
    return ESP_OK;
}

char* perform_http_request(const char *url, const char *method, const char *post_data, const char *headers[][2], int header_count) {
    http_buffer_t response_buf = { .buffer = calloc(1, 1), .len = 0 };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &response_buf,
        .crt_bundle_attach = esp_crt_bundle_attach, // Ensure HTTPS works
        .timeout_ms = 15000
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { free(response_buf.buffer); return NULL; }

    esp_http_client_set_method(client, strcmp(method, "POST") == 0 ? HTTP_METHOD_POST : HTTP_METHOD_GET);
    
    for (int i = 0; i < header_count; i++) {
        esp_http_client_set_header(client, headers[i][0], headers[i][1]);
    }

    if (post_data) {
        esp_http_client_set_post_field(client, post_data, strlen(post_data));
    }

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    if (err == ESP_OK && response_buf.len > 0) {
        return response_buf.buffer; // Caller must free
    }
    
    free(response_buf.buffer);
    return NULL;
}

char* handle_http_command(uint8_t cmd, char *payload, uint16_t len) {
    if (cmd == CMD_HTTP_GET) {
        if (!payload || len == 0) return strdup("ERROR_INVALID_PARAMS");
        char *resp = perform_http_request(payload, "GET", NULL, NULL, 0);
        return resp ? resp : strdup("ERROR_HTTP_REQUEST_FAILED");
    }

    if (cmd == CMD_HTTP_POST) {
        if (!payload || len == 0) return strdup("ERROR_INVALID_PARAMS");
        
        char *url = payload;
        size_t url_len = strlen(url);
        // Verify payload extends past the url and \0
        if (url_len + 1 >= len) return strdup("ERROR_INVALID_PARAMS");
        char *data = payload + url_len + 1;

        char *resp = perform_http_request(url, "POST", data, NULL, 0);
        return resp ? resp : strdup("ERROR_HTTP_REQUEST_FAILED");
    }

    return strdup("ERROR_UNKNOWN_COMMAND");
}