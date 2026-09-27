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

// Performs an HTTP request and returns the raw response body.
// Returns NULL on failure. Caller must free the returned buffer.
uint8_t* perform_http_request(const char *url, const char *method, const char *post_data, const char *headers[][2], int header_count, uint16_t *out_len) {
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
        if (out_len) *out_len = (uint16_t)response_buf.len;
        return (uint8_t*)response_buf.buffer; // Caller must free
    }
    
    free(response_buf.buffer);
    return NULL;
}

response_t handle_http_command(uint8_t cmd, uint8_t *payload, uint16_t len) {
    if (cmd == CMD_HTTP_GET) {
        if (!payload || len == 0) return response_status(RESP_ERROR_INVALID_PARAMS);
        // URL is null-terminated by the receiver FSM
        uint16_t resp_len = 0;
        uint8_t *resp = perform_http_request((const char*)payload, "GET", NULL, NULL, 0, &resp_len);
        if (!resp) return response_status(RESP_ERROR_HTTP_FAILED);
        response_t r = { .data = resp, .len = resp_len };
        return r;
    }

    if (cmd == CMD_HTTP_POST) {
        if (!payload || len == 0) return response_status(RESP_ERROR_INVALID_PARAMS);
        
        // Payload: url\0data (raw bytes)
        uint8_t *sep = memchr(payload, '\0', len);
        if (!sep || (size_t)(sep - payload) + 1 >= len) return response_status(RESP_ERROR_INVALID_PARAMS);
        char *url = (char*)payload;
        char *data = (char*)(sep + 1);

        uint16_t resp_len = 0;
        uint8_t *resp = perform_http_request(url, "POST", data, NULL, 0, &resp_len);
        if (!resp) return response_status(RESP_ERROR_HTTP_FAILED);
        response_t r = { .data = resp, .len = resp_len };
        return r;
    }

    return response_status(RESP_ERROR_UNKNOWN_COMMAND);
}