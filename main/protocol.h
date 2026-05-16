#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

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
    CMD_AI_QUERY     = 0x07
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

#endif // PROTOCOL_H