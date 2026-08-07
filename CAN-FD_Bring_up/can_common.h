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
 * CAN DLC (Data Length Code)
 *----------------------------------------------------------*/
#define CAN_DLC_0          0
#define CAN_DLC_1          1
#define CAN_DLC_2          2
#define CAN_DLC_3          3
#define CAN_DLC_4          4
#define CAN_DLC_5          5
#define CAN_DLC_6          6
#define CAN_DLC_7          7
#define CAN_DLC_8          8


/*-----------------------------------------------------------
 * CAN-FD payload length (Data Length Code)
 *----------------------------------------------------------*/
#define CANFD_DLC_0        0
#define CANFD_DLC_8        8
#define CANFD_DLC_12       12
#define CANFD_DLC_16       16
#define CANFD_DLC_20       20
#define CANFD_DLC_24       24
#define CANFD_DLC_32       32
#define CANFD_DLC_48       48
#define CANFD_DLC_64       64


/*-----------------------------------------------------------
 * Return Status
 *----------------------------------------------------------*/
#define CAN_SUCCESS         0
#define CAN_FAILURE        -1

/*-----------------------------------------------------------
 * Packet Structure
 * (Used in Phase-1)
 *----------------------------------------------------------*/
typedef struct
{
    uint32_t id;
    uint8_t dlc;
    uint8_t data[64];

} can_packet_t;

/*-----------------------------------------------------------
 * Function Prototypes
 *----------------------------------------------------------*/

/* Open CAN Socket */
int can_socket_init(void);

/* Send CAN Frame */
int can_send_frame(int socket_fd, can_packet_t *packet);

/* Receive CAN Frame */
int can_receive_frame(int socket_fd, can_packet_t *packet);

/* Close Socket */
void can_socket_close(int socket_fd);

#endif

