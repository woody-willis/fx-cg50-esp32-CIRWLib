#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <stdint.h>
#include "protocol.h"
#include "driver/uart.h" // Needed for UART_NUM_1

// Configuration Keys
#define CONFIG_KEY_OPENAI_API_KEY     0
#define CONFIG_KEY_OPENAI_API_URL     1
#define CONFIG_KEY_NETWORK_SSID       2
#define CONFIG_KEY_NETWORK_PASSWORD   3
#define CONFIG_KEY_UART_PORT_NUM      4
#define CONFIG_KEY_UART_BAUD_RATE     5
#define CONFIG_KEY_UART_TX_PIN        6
#define CONFIG_KEY_UART_TX_BUF_SIZE   7
#define CONFIG_KEY_UART_RX_PIN        8
#define CONFIG_KEY_UART_RX_BUF_SIZE   9

#define CONFIG_KEY_COUNT              10 // Total number of keys

// Default Values
#define DEFAULT_OPENAI_API_KEY        ""
#define DEFAULT_OPENAI_API_URL        "https://generativelanguage.googleapis.com/v1beta/openai"
#define DEFAULT_NETWORK_SSID          ""
#define DEFAULT_NETWORK_PASSWORD      ""
#define DEFAULT_UART_PORT_NUM         UART_NUM_1
#define DEFAULT_UART_BAUD_RATE        38400
#define DEFAULT_UART_TX_PIN           1
#define DEFAULT_UART_TX_BUF_SIZE      4096
#define DEFAULT_UART_RX_PIN           3
#define DEFAULT_UART_RX_BUF_SIZE      4096

// Core Functions
void config_init(void);
uint8_t* get_config_value(const int key, uint16_t *out_len);
resp_status_t set_config_value(const int key, const uint8_t *value, uint16_t len);
response_t handle_config_command(uint8_t cmd, uint8_t *payload, uint16_t len);

// Convenience accessors (caller must free the string returned by get_config_string)
char* get_config_string(const int key);
int32_t get_config_int(const int key);

#endif // CONFIG_HANDLER_H
