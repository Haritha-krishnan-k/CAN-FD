#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "can_common.h"
#include "isotp.h"
#include "uds.h"
#include "firmware_sender.h"
#include "crc.h"


#define RESPONSE_BUFFER_SIZE 4096

/*
 * Maximum firmware data carried by one UDS TransferData message.
 *
 * ISO-TP will take care of splitting the complete UDS message
 * into CAN-FD frames.
 *
 * For this POC we use 256 bytes of firmware per TransferData
 * request.
 */
#define TRANSFER_DATA_SIZE 256

int main(void)
{
    int socket_fd;

    uint8_t request[4096];
    uint8_t response[RESPONSE_BUFFER_SIZE];

    size_t response_length = 0;

    uint8_t *firmware;
    size_t firmware_size;

    size_t offset;
    uint8_t block_sequence = 1;

    /*-------------------------------------------------------
     * Initialize CAN
     *------------------------------------------------------*/

    socket_fd = can_socket_init();

    if (socket_fd < 0)
    {
        printf("CAN initialization failed\n");
        return -1;
    }

    printf("CAN Initialized Successfully\n");

    /*-------------------------------------------------------
     * Load firmware
     *------------------------------------------------------*/

    if (firmware_load("firmware.bin") != FW_SUCCESS)
    {
        printf("Firmware loading failed\n");

        can_socket_close(socket_fd);

        return -1;
    }

    firmware = firmware_get_buffer();
    firmware_size = firmware_get_size();

    printf("----------------------------------\n");
    printf("Firmware size: %zu bytes\n", firmware_size);
    printf("----------------------------------\n");

    /*-------------------------------------------------------
     * STEP 1
     *
     * Diagnostic Session Control
     *
     * Request:
     *
     *     10 02
     *
     * 02 = Programming Session
     *------------------------------------------------------*/

    request[0] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL;
    request[1] = UDS_SESSION_PROGRAMMING;

    printf("\n====================================\n");
    printf("STEP 1: Programming Session\n");
    printf("====================================\n");

    printf("Sending: 10 02\n");

    if (isotp_send_message(socket_fd,
                           request,
                           2) != ISOTP_SUCCESS)
    {
        printf("Failed to send session request\n");
        goto cleanup;
    }

    if (isotp_receive_message(socket_fd,
                              response,
                              sizeof(response),
                              &response_length) != ISOTP_SUCCESS)
    {
        printf("Failed to receive session response\n");
        goto cleanup;
    }

    printf("Response: ");

    for (size_t i = 0; i < response_length; i++)
    {
        printf("%02X ", response[i]);
    }

    printf("\n");

    if (response_length < 2 ||
        response[0] != 0x50 ||
        response[1] != UDS_SESSION_PROGRAMMING)
    {
        printf("Programming session activation failed\n");
        goto cleanup;
    }

    printf("Programming session activated\n");


    /*-------------------------------------------------------
     * STEP 2
     *
     * Request Download
     *
     * For this POC:
     *
     *     34
     *     00       Data Format Identifier
     *     44       Address/Length Format
     *     address  = 4 bytes
     *     size     = 4 bytes
     *
     * Total = 11 bytes
     *
     * 34 00 44 AA AA AA AA SS SS SS SS
     *------------------------------------------------------*/

    printf("\n====================================\n");
    printf("STEP 2: Request Download\n");
    printf("====================================\n");

    request[0] = UDS_SID_REQUEST_DOWNLOAD;

    /* Data Format Identifier */
    request[1] = 0x00;

    /*
     * AddressAndLengthFormatIdentifier
     *
     * 0x44:
     *     4 bytes address
     *     4 bytes memory size
     */
    request[2] = 0x44;

    /* Memory address = 0x00000000 */
    request[3] = 0x00;
    request[4] = 0x00;
    request[5] = 0x00;
    request[6] = 0x00;

    /* Firmware size */
    request[7]  = (uint8_t)((firmware_size >> 24) & 0xFF);
    request[8]  = (uint8_t)((firmware_size >> 16) & 0xFF);
    request[9]  = (uint8_t)((firmware_size >> 8) & 0xFF);
    request[10] = (uint8_t)(firmware_size & 0xFF);

    printf("Sending Request Download:\n");

    for (size_t i = 0; i < 11; i++)
    {
        printf("%02X ", request[i]);
    }

    printf("\n");

    if (isotp_send_message(socket_fd,
                           request,
                           11) != ISOTP_SUCCESS)
    {
        printf("Failed to send Request Download\n");
        goto cleanup;
    }

    response_length = 0;

    if (isotp_receive_message(socket_fd,
                              response,
                              sizeof(response),
                              &response_length) != ISOTP_SUCCESS)
    {
        printf("Failed to receive Request Download response\n");
        goto cleanup;
    }

    printf("Response: ");

    for (size_t i = 0; i < response_length; i++)
    {
        printf("%02X ", response[i]);
    }

    printf("\n");

    if (response_length < 1 ||
        response[0] != 0x74)
    {
        printf("Request Download rejected\n");
        goto cleanup;
    }

    printf("Request Download accepted\n");


    /*-------------------------------------------------------
     * STEP 3
     *
     * Transfer Data
     *
     * 36 <blockSequenceCounter> <firmware data>
     *------------------------------------------------------*/

    printf("\n====================================\n");
    printf("STEP 3: Transfer Firmware\n");
    printf("====================================\n");

    offset = 0;

    while (offset < firmware_size)
    {
        size_t remaining;
        size_t chunk_size;

        remaining = firmware_size - offset;

        if (remaining > TRANSFER_DATA_SIZE)
        {
            chunk_size = TRANSFER_DATA_SIZE;
        }
        else
        {
            chunk_size = remaining;
        }

        /*
         * UDS TransferData header:
         *
         * Byte 0 = 0x36
         * Byte 1 = block sequence counter
         */
        request[0] = UDS_SID_TRANSFER_DATA;
        request[1] = block_sequence;

        /*
         * Copy firmware data after the UDS header.
         */
        for (size_t i = 0; i < chunk_size; i++)
        {
            request[2 + i] = firmware[offset + i];
        }

        printf("\nSending block %u\n",
               block_sequence);

        printf("Offset      : %zu\n", offset);
        printf("Data length : %zu\n", chunk_size);

        if (isotp_send_message(socket_fd,
                               request,
                               chunk_size + 2) != ISOTP_SUCCESS)
        {
            printf("Failed to send TransferData block\n");
            goto cleanup;
        }

        response_length = 0;

        if (isotp_receive_message(socket_fd,
                                  response,
                                  sizeof(response),
                                  &response_length) != ISOTP_SUCCESS)
        {
            printf("Failed to receive TransferData response\n");
            goto cleanup;
        }

        /*
         * Positive response:
         *
         * 76 <blockSequenceCounter>
         */
        if (response_length < 2 ||
            response[0] != 0x76 ||
            response[1] != block_sequence)
        {
            printf("Invalid TransferData response\n");
            goto cleanup;
        }

        printf("Block %u acknowledged\n",
               block_sequence);

        offset += chunk_size;

        /*
         * UDS block sequence counter is one byte.
         * It wraps from FF -> 00.
         */
        block_sequence++;
    }

    printf("\nFirmware transfer completed\n");
    printf("Total transferred: %zu bytes\n",
           offset);


    /*-------------------------------------------------------
     * Calculate CRC of original firmware
     *------------------------------------------------------*/

    uint8_t *firmware_buffer;

    uint16_t expected_crc;

    firmware_buffer = firmware_get_buffer();
    firmware_size = firmware_get_size();

    expected_crc = crc16_calculate(firmware_buffer,
                               firmware_size);

    printf("\n");
    printf("====================================\n");
    printf("Tester Firmware CRC\n");
    printf("====================================\n");

    printf("Firmware Size : %zu Bytes\n",
       firmware_size);

    printf("Expected CRC  : 0x%04X\n",
       expected_crc);


/*-------------------------------------------------------
 * Request Transfer Exit
 *
 * Byte 0 = 0x37
 * Byte 1 = CRC MSB
 * Byte 2 = CRC LSB
 *------------------------------------------------------*/

    request[0] = UDS_SID_REQUEST_TRANSFER_EXIT;

    request[1] =
        (uint8_t)((expected_crc >> 8) & 0xFF);

    request[2] =
        (uint8_t)(expected_crc & 0xFF);

    printf("Sending: ");

    for (size_t i = 0; i < 3; i++)
    {
       printf("%02X ", request[i]);
    }

    printf("\n");

    if (isotp_send_message(socket_fd,
                       request,
                       3) != ISOTP_SUCCESS)
     {
        printf("Failed to send Transfer Exit\n");
        goto cleanup;
     }

    printf("Response: ");

    for (size_t i = 0; i < response_length; i++)
    {
        printf("%02X ", response[i]);
    }

    printf("\n");

    if (response_length >= 1 &&
        response[0] == 0x77)
    {
        printf("\n====================================\n");
        printf("FIRMWARE DOWNLOAD SUCCESS\n");
        printf("====================================\n");
    }
    else
    {
        printf("\nFirmware Transfer Exit failed\n");
    }


cleanup:

    firmware_unload();

    can_socket_close(socket_fd);

    return 0;
}
