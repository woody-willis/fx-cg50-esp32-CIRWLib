#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H
#include <stdint.h>

// Generic HTTP request helper (used by both HTTP and AI handlers)
char* perform_http_request(const char *url, const char *method, const char *post_data, const char *headers[][2], int header_count);
char* handle_http_command(uint8_t cmd, char *payload, uint16_t len);

#endif