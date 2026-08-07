/******************************************************************************
 * File        : packet.c
 * Description : Firmware Packetizer
 ******************************************************************************/

#include "packet.h"

#include <string.h>

/*-----------------------------------------------------------
 * Private Variables
 *----------------------------------------------------------*/

static uint8_t *fw_buffer = NULL;

static size_t fw_size = 0;

static size_t current_offset = 0;

static uint16_t current_sequence = 0;

/*-----------------------------------------------------------
 * Initialize Packetizer
 *----------------------------------------------------------*/

void packet_init(uint8_t *buffer,
                 size_t firmware_size)
{
    fw_buffer = buffer;

    fw_size = firmware_size;

    current_offset = 0;

    current_sequence = 0;
}

/*-----------------------------------------------------------
 * Check Whether More Packets Exist
 *----------------------------------------------------------*/

int packet_has_more(void)
{
    return (current_offset < fw_size);
}

/*-----------------------------------------------------------
 * Get Next Packet
 *----------------------------------------------------------*/

int packet_get_next(packet_t *packet)
{
    size_t remaining_bytes;

    size_t copy_length;

    if(packet == NULL)
        return PACKET_FAILURE;

    if(!packet_has_more())
        return PACKET_FAILURE;

    remaining_bytes = fw_size - current_offset;

    if(remaining_bytes >= PACKET_DATA_SIZE)
        copy_length = PACKET_DATA_SIZE;
    else
        copy_length = remaining_bytes;

    packet->sequence = current_sequence;

    packet->length = copy_length;

    memcpy(packet->data,
           &fw_buffer[current_offset],
           copy_length);

    current_offset += copy_length;

    current_sequence++;

    return PACKET_SUCCESS;
}

/*-----------------------------------------------------------
 * Reset Packetizer
 *----------------------------------------------------------*/

void packet_reset(void)
{
    current_offset = 0;

    current_sequence = 0;
}


