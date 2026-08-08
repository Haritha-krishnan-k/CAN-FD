#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>

#include "packet.h"
#include "can_common.h"

#define ISOTP_SUCCESS      0
#define ISOTP_FAILURE     -1

/* ISO-TP PCI Types */

#define ISOTP_SINGLE_FRAME      0x0
#define ISOTP_FIRST_FRAME       0x1
#define ISOTP_CONSECUTIVE_FRAME 0x2
#define ISOTP_FLOW_CONTROL      0x3

int isotp_send(int socket_fd,
               const packet_t *packet);

int isotp_receive(int socket_fd,
                  packet_t *packet);

#endif
