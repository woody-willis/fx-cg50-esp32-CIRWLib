#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "config.h"
#include "protocol.h"
#include "handlers/wifi.h"
#include "handlers/http.h"
#include "handlers/ai.h"

#define MAX_PACKET_SIZE 256

#define UART_TASK_STACK_SIZE 8192
#define UART_TASK_PRIORITY   5

static const char *TAG = "MAIN";

static void init_uart() {
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);

    gpio_pullup_en(UART_RX_PIN);
}

// Packages and sends a response back to the calculator using the TLV spec
void uart_send_packet(uint8_t cmd, const uint8_t *payload, uint16_t len) {
    if (len > 0 && payload == NULL) {
        ESP_LOGE(TAG, "Payload is NULL but len > 0");
        return;
    }

    if (len + 5 > MAX_PACKET_SIZE) {
        ESP_LOGE(TAG, "Payload too large: %d bytes", len);
        return;
    }

    uint8_t packet[MAX_PACKET_SIZE];

    packet[0] = PROTOCOL_SYNC_BYTE;
    packet[1] = cmd;
    packet[2] = (uint8_t)((len >> 8) & 0xFF);
    packet[3] = (uint8_t)(len & 0xFF);

    uint8_t checksum = packet[1] ^ packet[2] ^ packet[3];

    if (len > 0) {
        for (uint16_t i = 0; i < len; i++) {
            packet[4 + i] = payload[i];
            checksum ^= payload[i];
        }
    }

    packet[4 + len] = checksum;
    ESP_LOGI(TAG, "send_packet: cmd=0x%02X len=%d", cmd, len);
    ESP_LOG_BUFFER_HEX(TAG, packet, len + 5);
    uart_write_bytes(UART_PORT_NUM, packet, len + 5);
}

// Router to dispatch incoming binary payloads and collect string responses
void uart_process_packet(uint8_t cmd, uint8_t *payload, uint16_t len) {
    char *response = NULL;

    ESP_LOGI(TAG, "Processing Command ID: 0x%02X, Length: %d", cmd, len);

    switch (cmd) {
        case CMD_PING:
            response = malloc(1);
            if (response) {
                response[0] = 0x00; // Indicate success for ping
            }
            break;
        case CMD_WIFI_SCAN:
        case CMD_WIFI_STATUS:
        case CMD_WIFI_CONNECT:
            response = handle_wifi_command(cmd, (char*)payload, len);
            break;
        case CMD_HTTP_GET:
        case CMD_HTTP_POST:
            response = handle_http_command(cmd, (char*)payload, len);
            break;
        case CMD_AI_QUERY:
            response = handle_ai_command((char*)payload, len);
            break;
        default:
            response = malloc(1);
            if (response) {
                response[0] = 0xFF; // Indicate unknown command
            }
            break;
    }

    if (response) {
        uart_send_packet(cmd, (uint8_t*)response, strlen(response));
        free(response);
    } else {
        uart_send_packet(cmd, (uint8_t*)"OK", 2);
    }
}

static void uart_command_task(void *pvParameters) {
    // FSM State Variables
    rx_state_t state = STATE_WAIT_SYNC;
    uint8_t cmd = 0;
    uint16_t packet_len = 0;
    uint16_t payload_idx = 0;
    uint8_t checksum = 0;
    uint8_t *payload = NULL;
    TickType_t last_rx_time = xTaskGetTickCount();

    uint8_t rx_buf[128];

    while (1) {
        int bytes_read = uart_read_bytes(UART_PORT_NUM, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));

        if (bytes_read > 0) {
            last_rx_time = xTaskGetTickCount();

            for (int i = 0; i < bytes_read; i++) {
                uint8_t b = rx_buf[i];

                switch (state) {
                    case STATE_WAIT_SYNC:
                        if (b == PROTOCOL_SYNC_BYTE) {
                            checksum = 0;
                            state = STATE_READ_CMD;
                        }
                        break;

                    case STATE_READ_CMD:
                        cmd = b;
                        checksum ^= b;
                        state = STATE_READ_LEN_H;
                        break;

                    case STATE_READ_LEN_H:
                        packet_len = (b << 8);
                        checksum ^= b;
                        state = STATE_READ_LEN_L;
                        break;

                    case STATE_READ_LEN_L:
                        packet_len |= b;
                        checksum ^= b;

                        if (packet_len > 0) {
                            // Prevent heap overflow from malformed lengths
                            if (packet_len > 16384) {
                                ESP_LOGE(TAG, "Payload too large: %d", packet_len);
                                state = STATE_WAIT_SYNC;
                            } else {
                                payload = malloc(packet_len + 1); // +1 safety margin
                                if (!payload) {
                                    ESP_LOGE(TAG, "OOM Memory allocation failed!");
                                    state = STATE_WAIT_SYNC;
                                } else {
                                    payload_idx = 0;
                                    state = STATE_READ_PAYLOAD;
                                }
                            }
                        } else {
                            state = STATE_READ_CHECKSUM;
                        }
                        break;

                    case STATE_READ_PAYLOAD:
                        payload[payload_idx++] = b;
                        checksum ^= b;
                        if (payload_idx == packet_len) {
                            payload[packet_len] = '\0'; // Safety null-termination
                            state = STATE_READ_CHECKSUM;
                        }
                        break;

                    case STATE_READ_CHECKSUM:
                        if (checksum == b) {
                            uart_process_packet(cmd, payload, packet_len);
                        } else {
                            ESP_LOGE(TAG, "Checksum mismatch! Calc: 0x%02X, Recv: 0x%02X", checksum, b);
                        }

                        if (payload) {
                            free(payload);
                            payload = NULL;
                        }
                        state = STATE_WAIT_SYNC;
                        break;
                }
            }
        } else {
            // FSM Timeout recovery to prevent freezing on a clipped/partial packet
            if (state != STATE_WAIT_SYNC && (xTaskGetTickCount() - last_rx_time) > pdMS_TO_TICKS(1500)) {
                ESP_LOGW(TAG, "UART Rx Timeout. Resetting FSM.");
                if (payload) {
                    free(payload);
                    payload = NULL;
                }
                state = STATE_WAIT_SYNC;
            }
        }
    }

    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Initializing System...");
    wifi_init();
    init_uart();
    ESP_LOGI(TAG, "System Initialized. Waiting for UART commands.");

    BaseType_t task_created = xTaskCreate(
        uart_command_task,
        "uart_cmd_task",
        UART_TASK_STACK_SIZE,
        NULL,
        UART_TASK_PRIORITY,
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create uart_command_task");
    }
}