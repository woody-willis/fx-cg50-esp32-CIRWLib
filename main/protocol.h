#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdlib.h>

// Specification: Sync Byte
#define PROTOCOL_SYNC_BYTE 0xAA

// Specification: Command ID Registry
typedef enum {
    CMD_PING         = 0x01,
    CMD_WIFI_SCAN    = 0x02,
    CMD_WIFI_STATUS  = 0x03,
    CMD_WIFI_CONNECT = 0x04,
    CMD_HTTP_GET     = 0x05,
    CMD_HTTP_POST    = 0x06,
    CMD_AI_QUERY     = 0x07,
    CMD_CONFIG_GET   = 0x08,
    CMD_CONFIG_SET   = 0x09
} cmd_id_t;

// Specification: FSM States for Receiver
typedef enum {
    STATE_WAIT_SYNC = 0,
    STATE_READ_CMD,
    STATE_READ_LEN_H,
    STATE_READ_LEN_L,
    STATE_READ_PAYLOAD,
    STATE_READ_CHECKSUM
} rx_state_t;

// Raw byte response buffer returned by handlers.
// `data` is heap-allocated (or NULL when len == 0) and must be freed by the caller.
typedef struct {
    uint8_t *data;
    uint16_t len;
} response_t;

// Specification: Raw status codes (single byte, no string encoding)
typedef enum {
    RESP_OK                      = 0x00,
    RESP_ERROR_INVALID_PARAMS    = 0x01,
    RESP_ERROR_UNKNOWN_COMMAND   = 0x02,
    RESP_ERROR_AP_NOT_FOUND      = 0x03,
    RESP_ERROR_AUTH_FAILED       = 0x04,
    RESP_ERROR_CONNECT_FAILED    = 0x05,
    RESP_ERROR_CONNECT_TIMEOUT   = 0x06,
    RESP_ERROR_HTTP_FAILED       = 0x07,
    RESP_ERROR_AI_REQUEST_FAILED = 0x08,
    RESP_ERROR_AI_PARSE_FAILED   = 0x09,
    RESP_WIFI_IDLE               = 0x10,
    RESP_WIFI_CONNECTING         = 0x11,
    RESP_CONFIG_NOT_FOUND        = 0x12,
    RESP_ERROR_UNKNOWN           = 0xFF
} resp_status_t;

// Helper: build a response_t wrapping a single raw status byte.
static inline response_t response_status(resp_status_t status) {
    response_t r;
    r.data = malloc(1);
    if (r.data) r.data[0] = (uint8_t)status;
    r.len = 1;
    return r;
}

#endif // PROTOCOL_H