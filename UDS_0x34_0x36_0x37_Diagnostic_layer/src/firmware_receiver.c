#include "firmware_receiver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------
 * Private variables
 *----------------------------------------------------------*/

static uint8_t *receive_buffer = NULL;

static size_t firmware_size = 0;

static size_t received_bytes = 0;


/*-----------------------------------------------------------
 * Initialize receiver
 *----------------------------------------------------------*/

int firmware_receiver_init(size_t size)
{
    if (size == 0)
    {
        return FW_RECEIVER_FAILURE;
    }

    /* Allocate memory for complete firmware */

    receive_buffer = malloc(size);

    if (receive_buffer == NULL)
    {
        perror("malloc");
        return FW_RECEIVER_FAILURE;
    }

    firmware_size = size;

    received_bytes = 0;

    memset(receive_buffer, 0, size);

    printf("Firmware Receiver Initialized\n");
    printf("Expected Firmware Size : %zu Bytes\n",
           firmware_size);

    return FW_RECEIVER_SUCCESS;
}


/*-----------------------------------------------------------
 * Store reassembled ISO-TP firmware data
 *----------------------------------------------------------*/

int firmware_receiver_store_data(const uint8_t *data,
                                 size_t length)
{
    if (data == NULL)
    {
        return FW_RECEIVER_FAILURE;
    }

    if (receive_buffer == NULL)
    {
        return FW_RECEIVER_FAILURE;
    }

    if (length == 0)
    {
        return FW_RECEIVER_SUCCESS;
    }

    /*
     * Make sure the ISO-TP layer does not
     * write beyond the firmware buffer.
     */

    if ((received_bytes + length) > firmware_size)
    {
        printf("Firmware buffer overflow\n");
        return FW_RECEIVER_FAILURE;
    }

    /*
     * Copy reassembled firmware data
     * into receiver buffer.
     */

    memcpy(&receive_buffer[received_bytes],
           data,
           length);

    received_bytes += length;

    printf("Stored %zu bytes | "
           "Total received = %zu / %zu\n",
           length,
           received_bytes,
           firmware_size);

    return FW_RECEIVER_SUCCESS;
}


/*-----------------------------------------------------------
 * Get firmware buffer
 *----------------------------------------------------------*/

uint8_t *firmware_receiver_get_buffer(void)
{
    return receive_buffer;
}


/*-----------------------------------------------------------
 * Get expected firmware size
 *----------------------------------------------------------*/

size_t firmware_receiver_get_size(void)
{
    return firmware_size;
}


/*-----------------------------------------------------------
 * Get received bytes
 *----------------------------------------------------------*/

size_t firmware_receiver_get_received_bytes(void)
{
    return received_bytes;
}


/*-----------------------------------------------------------
 * Check whether firmware reception is complete
 *----------------------------------------------------------*/

int firmware_receiver_complete(void)
{
    return (received_bytes == firmware_size);
}


/*-----------------------------------------------------------
 * Deinitialize receiver
 *----------------------------------------------------------*/

void firmware_receiver_deinit(void)
{
    if (receive_buffer != NULL)
    {
        free(receive_buffer);
    }

    receive_buffer = NULL;

    firmware_size = 0;

    received_bytes = 0;
}
