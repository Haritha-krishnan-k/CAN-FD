#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>
#include <stddef.h>

#include "can_common.h"

#define ISOTP_SUCCESS  0
#define ISOTP_FAILURE -1

/* ISO-TP PCI Types */

#define ISOTP_SINGLE_FRAME       0x00
#define ISOTP_FIRST_FRAME        0x10
#define ISOTP_CONSECUTIVE_FRAME  0x20
#define ISOTP_FLOW_CONTROL       0x30

/* CAN-FD maximum payload */
#define ISOTP_CANFD_MAX_DATA     64
#define ISOTP_TIMEOUT_US         500000
/* Flow Control */

#define ISOTP_FC_CTS             0x30
#define ISOTP_FC_WAIT            0x31
#define ISOTP_FC_OVERFLOW        0x32

/* Block Size
 *
 * 0 = sender can continue until transfer completes
 */
#define ISOTP_BLOCK_SIZE         0

/* Separation Time
 *
 * 0 = no additional delay
 */
#define ISOTP_STMIN               0

/*-----------------------------------------------------------
 * ISO-TP CAN IDs
 *----------------------------------------------------------*/

#define ISOTP_TX_ID        0x700
#define ISOTP_RX_ID        0x708



/*-----------------------------------------------------------
 * Generic ISO-TP APIs
 *
 * These APIs transport arbitrary application messages.
 * UDS will use these APIs.
 *----------------------------------------------------------*/

/*
 * Send one complete application-layer message
 * using ISO-TP.
 */
int isotp_send_message(int socket_fd,
                       const uint8_t *data,
                       size_t length);


/*
 * Receive one complete application-layer message
 * using ISO-TP.
 *
 * The caller provides the receive buffer and its size.
 */
int isotp_receive_message(int socket_fd,
                          uint8_t *buffer,
                          size_t buffer_size,
                          size_t *received_size);


/*
 * Sender
 *
 * firmware -> firmware buffer
 * firmware_size -> total firmware size
 */
int isotp_send_firmware(int socket_fd,
                        const uint8_t *firmware,
                        size_t firmware_size);


/*
 * Receiver
 *
 * buffer -> destination firmware buffer
 * buffer_size -> maximum buffer capacity
 * received_size -> actual firmware size received
 */
int isotp_receive_firmware(int socket_fd,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *received_size);



int isotp_send_message(int socket_fd,
                       const uint8_t *data,
                       size_t length);

int isotp_receive_message(int socket_fd,
                          uint8_t *buffer,
                          size_t buffer_size,
                          size_t *received_size);


#endif
