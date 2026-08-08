/******************************************************************************
 * File        : firmware_receiver.c
 * Description : Firmware Receiver Module
 ******************************************************************************/

#include "firmware_receiver.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*-----------------------------------------------------------
 * Private Variables
 *----------------------------------------------------------*/

static uint8_t *receive_buffer = NULL;

static size_t expected_size = 0;

static size_t received_bytes = 0;

/*-----------------------------------------------------------
 * Initialize Receiver
 *----------------------------------------------------------*/

int firmware_receiver_init(size_t firmware_size)
{
    receive_buffer = (uint8_t *)malloc(firmware_size);

    if(receive_buffer == NULL)
    {
        perror("malloc");

        return FW_RECEIVER_FAILURE;
    }

    expected_size = firmware_size;

    received_bytes = 0;

    printf("----------------------------------\n");
    printf("Receiver Initialized\n");
    printf("Expected Firmware Size : %zu Bytes\n", expected_size);
    printf("----------------------------------\n");

    return FW_RECEIVER_SUCCESS;
}

/*-----------------------------------------------------------
 * Store One Packet
 *----------------------------------------------------------*/

int firmware_receiver_store(packet_t *packet)
{
    size_t offset;

    if(packet == NULL)
        return FW_RECEIVER_FAILURE;

    offset = packet->sequence * PACKET_DATA_SIZE;

    if(offset + packet->length > expected_size)
    {
        printf("Packet exceeds firmware size\n");

        return FW_RECEIVER_FAILURE;
    }

    memcpy(&receive_buffer[offset],
           packet->data,
           packet->length);

    received_bytes += packet->length;

    printf("Stored Packet %-5u Offset %-6zu Length %-3u\n",
            packet->sequence,
            offset,
            packet->length);

    return FW_RECEIVER_SUCCESS;
}

/*-----------------------------------------------------------
 * Get Receive Buffer
 *----------------------------------------------------------*/

uint8_t *firmware_receiver_get_buffer(void)
{
    return receive_buffer;
}

/*-----------------------------------------------------------
 * Get Expected Size
 *----------------------------------------------------------*/

size_t firmware_receiver_get_size(void)
{
    return expected_size;
}

/*-----------------------------------------------------------
 * Get Received Bytes
 *----------------------------------------------------------*/

size_t firmware_receiver_get_received_bytes(void)
{
    return received_bytes;
}

/*-----------------------------------------------------------
 * Check Completion
 *----------------------------------------------------------*/

int firmware_receiver_complete(void)
{
    return (received_bytes == expected_size);
}

/*-----------------------------------------------------------
 * Free Receiver Buffer
 *----------------------------------------------------------*/

void firmware_receiver_deinit(void)
{
    if(receive_buffer != NULL)
    {
        free(receive_buffer);

        receive_buffer = NULL;
    }

    expected_size = 0;

    received_bytes = 0;
}
