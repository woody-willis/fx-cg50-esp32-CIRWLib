#ifndef WIFI_H
#define WIFI_H
#include <stdint.h>

void wifi_init(void);
char* handle_wifi_command(uint8_t cmd, char *payload, uint16_t len);

#endif