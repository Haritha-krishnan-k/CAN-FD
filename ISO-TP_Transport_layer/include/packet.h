/******************************************************************************
 * File        : packet.h
 * Description : Firmware Packetizer
 ******************************************************************************/

#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stddef.h>

/*-----------------------------------------------------------
 * Packet Payload Size
 *----------------------------------------------------------*/
#define PACKET_DATA_SIZE      62

/*-----------------------------------------------------------
 * Return Status
 *----------------------------------------------------------*/
#define PACKET_SUCCESS         0
#define PACKET_FAILURE        -1

/*-----------------------------------------------------------
 * Packet Structure
 *----------------------------------------------------------*/
typedef struct
{
    uint16_t sequence;

    uint16_t length;

    uint8_t data[PACKET_DATA_SIZE];

} packet_t;

/*-----------------------------------------------------------
 * Function Prototypes
 *----------------------------------------------------------*/

/* Initialize packetizer */
void packet_init(uint8_t *buffer,
                 size_t firmware_size);

/* Get next packet */
int packet_get_next(packet_t *packet);

/* Check whether packets remain */
int packet_has_more(void);

/* Reset packetizer */
void packet_reset(void);

#endif
