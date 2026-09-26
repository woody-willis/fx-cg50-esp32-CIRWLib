#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <stdint.h>
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

// Robust Default Values
#define DEFAULT_OPENAI_API_KEY        ""
#define DEFAULT_OPENAI_API_URL        "https://api.openai.com/v1/chat/completions"
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
char* get_config_value(const int key);
char* set_config_value(const int key, const char *value);
char* handle_config_command(char *payload, uint16_t len);

#endif // CONFIG_HANDLER_H
