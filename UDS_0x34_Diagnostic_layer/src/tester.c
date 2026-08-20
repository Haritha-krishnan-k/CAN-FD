#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "isotp.h"
#include "uds.h"
#include "can_common.h"
#include "firmware_sender.h"
#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------
 * Configuration
 *----------------------------------------------------------*/

#define FIRMWARE_FILE "firmware.bin"

#define RESPONSE_BUFFER_SIZE 256

/*
 * Maximum firmware data carried inside one UDS TransferData
 * message.
 *
 * ISO-TP will take care of splitting this UDS message into
 * CAN-FD frames.
 *
 * 1 byte  = SID
 * 1 byte  = block sequence counter
 * remaining bytes = firmware data
 */
#define TRANSFER_DATA_SIZE 1024


/*-----------------------------------------------------------
 * Print buffer
 *----------------------------------------------------------*/

static void print_hex(const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        printf("%02X ", data[i]);
    }

    printf("\n");
}


/*-----------------------------------------------------------
 * Send UDS request and receive response
 *----------------------------------------------------------*/

static int uds_send_request(
        int socket_fd,
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t response_buffer_size,
        size_t *response_length)
{
    printf("\n----------------------------------------\n");

    printf("UDS Request: ");
    print_hex(request, request_length);

    /*
     * Send complete UDS message.
     *
     * ISO-TP performs CAN-FD transport.
     */
    if (isotp_send_message(
                socket_fd,
                request,
                request_length) != ISOTP_SUCCESS)
    {
        printf("ERROR: Failed to send UDS request\n");
        return -1;
    }

    /*
     * Wait for complete UDS response.
     */
    if (isotp_receive_message(
                socket_fd,
                response,
                response_buffer_size,
                response_length) != ISOTP_SUCCESS)
    {
        printf("ERROR: Failed to receive UDS response\n");
        return -1;
    }

    printf("UDS Response: ");
    print_hex(response, *response_length);

    /*
     * Check for UDS negative response.
     *
     * Format:
     *
     * 7F <original SID> <NRC>
     */
    if (*response_length >= 3 &&
        response[0] == UDS_NEGATIVE_RESPONSE)
    {
        printf("UDS Negative Response\n");
        printf("Service : 0x%02X\n", response[1]);
        printf("NRC     : 0x%02X\n", response[2]);

        return -1;
    }

    return 0;
}


/*-----------------------------------------------------------
 * Main
 *----------------------------------------------------------*/

int main(void)
{
    int socket_fd;

    uint8_t response[RESPONSE_BUFFER_SIZE];

    size_t response_length = 0;

    uint8_t *firmware;

    size_t firmware_size;

    size_t offset = 0;

    uint8_t block_sequence = 1;


    /*-------------------------------------------------------
     * STEP 1
     *
     * Load firmware
     *------------------------------------------------------*/

    printf("\n========================================\n");
    printf("        UDS FIRMWARE DOWNLOAD TESTER\n");
    printf("========================================\n");

    printf("\nLoading firmware: %s\n",
           FIRMWARE_FILE);

    if (firmware_load(FIRMWARE_FILE) != FW_SUCCESS)
    {
        printf("ERROR: Firmware loading failed\n");
        return -1;
    }

    firmware = firmware_get_buffer();
    firmware_size = firmware_get_size();

    printf("Firmware size: %zu bytes\n",
           firmware_size);


    /*-------------------------------------------------------
     * STEP 2
     *
     * Initialize CAN-FD
     *------------------------------------------------------*/

    socket_fd = can_socket_init();

    if (socket_fd < 0)
    {
        printf("CAN initialization failed\n");

        firmware_unload();

        return -1;
    }

    printf("CAN Initialized Successfully\n");


    /*-------------------------------------------------------
     * STEP 3
     *
     * Diagnostic Session Control
     *
     * Request:
     *
     *     10 02
     *
     * 02 = Programming Session
     *
     * Expected response:
     *
     *     50 02
     *------------------------------------------------------*/

    {
        uint8_t request[] =
        {
            UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
            UDS_SESSION_PROGRAMMING
        };

        response_length = 0;

        printf("\n");
        printf("========================================\n");
        printf("STEP 1: PROGRAMMING SESSION\n");
        printf("========================================\n");

        if (uds_send_request(
                    socket_fd,
                    request,
                    sizeof(request),
                    response,
                    sizeof(response),
                    &response_length) != 0)
        {
            printf("ERROR: Programming session failed\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }

        /*
         * Expected:
         *
         * 50 02
         */

        if (response_length < 2 ||
            response[0] !=
                (UDS_SID_DIAGNOSTIC_SESSION_CONTROL +
                 UDS_POSITIVE_RESPONSE_OFFSET) ||
            response[1] != UDS_SESSION_PROGRAMMING)
        {
            printf("ERROR: Invalid programming-session response\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }

        printf("Programming Session Activated\n");
    }


    /*-------------------------------------------------------
     * STEP 4
     *
     * Request Download
     *
     * For this POC:
     *
     * 34
     * 00                  Data Format Identifier
     * 44                  Address/Length format
     * 00 00 00 00         Memory address
     * <4-byte size>       Firmware size
     *
     *------------------------------------------------------*/

    {
        uint8_t request[11];

        uint32_t firmware_size_32 =
            (uint32_t)firmware_size;

        request[0] =
            UDS_SID_REQUEST_DOWNLOAD;

        /*
         * Data Format Identifier
         *
         * 0x00 = no compression,
         * no encryption.
         */

        request[1] = 0x00;

        /*
         * AddressAndLengthFormatIdentifier
         *
         * 0x44:
         *
         * high nibble = 4 bytes address
         * low nibble  = 4 bytes size
         */

        request[2] = 0x44;

        /*
         * Memory address.
         *
         * For our host POC we use address 0.
         */

        request[3] = 0x00;
        request[4] = 0x00;
        request[5] = 0x00;
        request[6] = 0x00;

        /*
         * Firmware size.
         *
         * Big-endian.
         */

        request[7] =
            (firmware_size_32 >> 24) & 0xFF;

        request[8] =
            (firmware_size_32 >> 16) & 0xFF;

        request[9] =
            (firmware_size_32 >> 8) & 0xFF;

        request[10] =
            firmware_size_32 & 0xFF;


        response_length = 0;

        printf("\n");
        printf("========================================\n");
        printf("STEP 2: REQUEST DOWNLOAD\n");
        printf("========================================\n");

        if (uds_send_request(
                    socket_fd,
                    request,
                    sizeof(request),
                    response,
                    sizeof(response),
                    &response_length) != 0)
        {
            printf("ERROR: Request Download failed\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }

        /*
         * Positive response:
         *
         * 74 ...
         */

        if (response_length < 1 ||
            response[0] !=
                (UDS_SID_REQUEST_DOWNLOAD +
                 UDS_POSITIVE_RESPONSE_OFFSET))
        {
            printf("ERROR: Invalid Request Download response\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }

        printf("Download Accepted by Target\n");
    }


    /*-------------------------------------------------------
     * STEP 5
     *
     * Transfer Data
     *
     * 36 <sequence> <firmware data>
     *
     * Example:
     *
     * 36 01 AA BB CC ...
     * 36 02 ...
     * 36 03 ...
     *
     *------------------------------------------------------*/

    printf("\n");
    printf("========================================\n");
    printf("STEP 3: TRANSFER DATA\n");
    printf("========================================\n");

    printf("Starting firmware transfer...\n");
    printf("Total firmware: %zu bytes\n",
           firmware_size);


    while (offset < firmware_size)
    {
        size_t remaining =
            firmware_size - offset;

        size_t chunk_size =
            remaining;

        /*
         * Limit one TransferData UDS message.
         */

        if (chunk_size > TRANSFER_DATA_SIZE)
        {
            chunk_size = TRANSFER_DATA_SIZE;
        }


        /*
         * UDS request:
         *
         * Byte 0 = 0x36
         * Byte 1 = sequence counter
         * Byte 2... = firmware
         */

        size_t request_length =
            2 + chunk_size;

        uint8_t *request =
            malloc(request_length);

        if (request == NULL)
        {
            printf("ERROR: Memory allocation failed\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }


        request[0] =
            UDS_SID_TRANSFER_DATA;

        request[1] =
            block_sequence;


        /*
         * Copy firmware chunk.
         */

        memcpy(
            &request[2],
            &firmware[offset],
            chunk_size);


        printf("\nTransfer block %u\n",
               block_sequence);

        printf("Offset : %zu\n",
               offset);

        printf("Size   : %zu bytes\n",
               chunk_size);


        response_length = 0;


        /*
         * Send 0x36 request.
         */

        if (uds_send_request(
                    socket_fd,
                    request,
                    request_length,
                    response,
                    sizeof(response),
                    &response_length) != 0)
        {
            free(request);

            printf("ERROR: Transfer Data failed\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }


        /*
         * Expected:
         *
         * 76 <sequence>
         */

        if (response_length < 2 ||
            response[0] !=
                (UDS_SID_TRANSFER_DATA +
                 UDS_POSITIVE_RESPONSE_OFFSET) ||
            response[1] != block_sequence)
        {
            free(request);

            printf("ERROR: Invalid Transfer Data response\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }


        free(request);


        /*
         * Advance firmware pointer.
         */

        offset += chunk_size;


        /*
         * Sequence counter is one byte.
         *
         * After 0xFF it wraps to 0x00.
         */

        block_sequence++;


        printf("Progress: %zu / %zu bytes\n",
               offset,
               firmware_size);
    }


    printf("\nFirmware transfer completed\n");


    /*-------------------------------------------------------
     * STEP 6
     *
     * Request Transfer Exit
     *
     * Request:
     *
     *     37
     *
     * Expected:
     *
     *     77
     *------------------------------------------------------*/

    {
        uint8_t request[] =
        {
            UDS_SID_REQUEST_TRANSFER_EXIT
        };

        response_length = 0;

        printf("\n");
        printf("========================================\n");
        printf("STEP 4: REQUEST TRANSFER EXIT\n");
        printf("========================================\n");

        if (uds_send_request(
                    socket_fd,
                    request,
                    sizeof(request),
                    response,
                    sizeof(response),
                    &response_length) != 0)
        {
            printf("ERROR: Transfer Exit failed\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }


        /*
         * Expected:
         *
         * 77
         */

        if (response_length < 1 ||
            response[0] !=
                (UDS_SID_REQUEST_TRANSFER_EXIT +
                 UDS_POSITIVE_RESPONSE_OFFSET))
        {
            printf("ERROR: Invalid Transfer Exit response\n");

            can_socket_close(socket_fd);
            firmware_unload();

            return -1;
        }

        printf("Transfer Exit Accepted\n");
    }


    /*-------------------------------------------------------
     * STEP 7
     *
     * Download completed
     *------------------------------------------------------*/

    printf("\n");
    printf("========================================\n");
    printf("       FIRMWARE DOWNLOAD SUCCESS\n");
    printf("========================================\n");

    printf("Firmware size : %zu bytes\n",
           firmware_size);

    printf("Transferred   : %zu bytes\n",
           offset);

    printf("Status        : SUCCESS\n");

    printf("========================================\n");


    /*-------------------------------------------------------
     * Cleanup
     *------------------------------------------------------*/

    can_socket_close(socket_fd);

    firmware_unload();

    return 0;
}
