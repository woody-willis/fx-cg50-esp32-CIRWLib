#ifndef WIFI_H
#define WIFI_H
#include <stdint.h>
#include "protocol.h"

void wifi_init(void);
response_t handle_wifi_command(uint8_t cmd, uint8_t *payload, uint16_t len);

#endif