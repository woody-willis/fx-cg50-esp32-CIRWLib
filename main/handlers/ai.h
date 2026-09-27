#ifndef AI_HANDLER_H
#define AI_HANDLER_H
#include <stdint.h>
#include "protocol.h"

response_t handle_ai_command(uint8_t *payload, uint16_t len);

#endif