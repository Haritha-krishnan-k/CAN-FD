/******************************************************************************
 * File        : can_common.h
 * Description : Common CAN-FD APIs
 ******************************************************************************/

#ifndef CAN_COMMON_H
#define CAN_COMMON_H

#include <stdint.h>
#include <linux/can.h>

/*-----------------------------------------------------------
 * CAN Interface
 *----------------------------------------------------------*/

#define CAN_INTERFACE      "vcan0"

/*-----------------------------------------------------------
 * CAN Identifiers
 *----------------------------------------------------------*/

#define TESTER_CAN_ID      0x700
#define TARGET_CAN_ID      0x701

/*-----------------------------------------------------------
 * Return Status
 *----------------------------------------------------------*/

#define CAN_SUCCESS         0
#define CAN_FAILURE        -1

/*-----------------------------------------------------------
 * Low-Level CAN Frame
 *----------------------------------------------------------*/

typedef struct
{
    uint32_t id;
    uint8_t dlc;
    uint8_t data[64];

} can_packet_t;

/*-----------------------------------------------------------
 * Socket APIs
 *----------------------------------------------------------*/

/* Open CAN Socket */
int can_socket_init(void);

/* Close CAN Socket */
void can_socket_close(int socket_fd);

/*-----------------------------------------------------------
 * Low-Level CAN APIs
 *----------------------------------------------------------*/

/* Send one CAN-FD frame */
int can_send_frame(int socket_fd,
                   const can_packet_t *frame);

/* Receive one CAN-FD frame */
int can_receive_frame(int socket_fd,
                      can_packet_t *frame);

/*-----------------------------------------------------------
 * High-Level Firmware Packet APIs
 *----------------------------------------------------------*/

/*
 * Convert packet_t -> CAN Frame
 * Send over SocketCAN
 */


/*
 * Receive CAN Frame
 * Convert CAN Frame -> packet_t
 */
//int can_receive_packet(int socket_fd, packet_t *packet);


//void packet_encode(const packet_t *packet,can_packet_t *frame);

//void packet_decode(const can_packet_t *frame,packet_t *packet);

#endif
