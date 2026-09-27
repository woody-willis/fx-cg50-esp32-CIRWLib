#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H
#include <stdint.h>
#include "protocol.h"

// Generic HTTP request helper (used by both HTTP and AI handlers).
// Returns the raw response body (heap-allocated) or NULL on failure.
// On success, *out_len is set to the body length. Caller must free the buffer.
uint8_t* perform_http_request(const char *url, const char *method, const char *post_data, const char *headers[][2], int header_count, uint16_t *out_len);
response_t handle_http_command(uint8_t cmd, uint8_t *payload, uint16_t len);

#endif