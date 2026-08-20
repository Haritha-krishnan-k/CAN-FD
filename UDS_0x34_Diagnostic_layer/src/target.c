#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "isotp.h"
#include "uds.h"
#include "can_common.h"

#define UDS_BUFFER_SIZE 4096


/*-----------------------------------------------------------
 * Print UDS message
 *----------------------------------------------------------*/

static void print_message(const char *name,
                          const uint8_t *data,
                          size_t length)
{
    printf("%s (%zu bytes): ", name, length);

    for (size_t i = 0; i < length; i++)
    {
        printf("%02X ", data[i]);
    }

    printf("\n");
}


/*-----------------------------------------------------------
 * Print Download Status
 *----------------------------------------------------------*/

static void print_download_status(void)
{
    uds_download_state_t state =
        uds_get_download_state();

    switch (state)
    {
        case UDS_DOWNLOAD_IDLE:

            printf("Download State : IDLE\n");

            break;


        case UDS_DOWNLOAD_REQUESTED:

            printf("Download State : REQUESTED\n");

            break;


        case UDS_DOWNLOAD_IN_PROGRESS:

            printf("Download State : IN PROGRESS\n");

            printf("Download Progress : %zu / %zu bytes\n",
                   uds_get_received_firmware_size(),
                   uds_get_expected_firmware_size());

            break;


        case UDS_DOWNLOAD_COMPLETE:

            printf("Download State : COMPLETE\n");

            printf("Firmware Size : %zu bytes\n",
                   uds_get_expected_firmware_size());

            break;


        default:

            printf("Download State : UNKNOWN\n");

            break;
    }
}


/*-----------------------------------------------------------
 * Main
 *----------------------------------------------------------*/

int main(void)
{
    int socket_fd;

    uint8_t request[UDS_BUFFER_SIZE];
    uint8_t response[UDS_BUFFER_SIZE];

    size_t request_length;
    size_t response_length;


    /*-------------------------------------------------------
     * Step 1:
     *
     * Initialize CAN-FD
     *------------------------------------------------------*/

    printf("\n");
    printf("============================================\n");
    printf(" UDS Firmware Download Target\n");
    printf("============================================\n");


    socket_fd = can_socket_init();


    if (socket_fd < 0)
    {
        printf("CAN initialization failed\n");

        return -1;
    }


    printf("CAN Initialized Successfully\n");


    /*-------------------------------------------------------
     * Step 2:
     *
     * Target is now waiting for a tester request.
     *------------------------------------------------------*/

    printf("\n");
    printf("Target is ready.\n");
    printf("Waiting for UDS requests...\n");


    /*-------------------------------------------------------
     * Step 3:
     *
     * Main UDS processing loop
     *------------------------------------------------------*/

    while (1)
    {
        request_length = 0;
        response_length = 0;


        /*---------------------------------------------------
         * Receive complete UDS message.
         *
         * ISO-TP handles:
         *
         * Single Frame
         * First Frame
         * Consecutive Frames
         * Flow Control
         *
         * The result here is one complete UDS message.
         *--------------------------------------------------*/

        printf("\n");
        printf("Waiting for ISO-TP message...\n");


        if (isotp_receive_message(
                socket_fd,
                request,
                sizeof(request),
                &request_length)
                != ISOTP_SUCCESS)
        {
            printf("ISO-TP request reception failed\n");

            continue;
        }


        /*---------------------------------------------------
         * We now have a complete UDS request.
         *--------------------------------------------------*/

        print_message(
            "UDS Request",
            request,
            request_length);


        /*---------------------------------------------------
         * Process UDS request.
         *
         * uds.c determines what service was requested:
         *
         * 10 = Diagnostic Session Control
         * 11 = ECU Reset
         * 31 = Routine Control
         * 34 = Request Download
         * 36 = Transfer Data
         * 37 = Request Transfer Exit
         *--------------------------------------------------*/

        if (uds_process_request(
                request,
                request_length,
                response,
                &response_length)
                != UDS_SUCCESS)
        {
            printf("UDS request processing failed\n");

            continue;
        }


        /*---------------------------------------------------
         * Print download state after processing.
         *--------------------------------------------------*/

        print_download_status();


        /*---------------------------------------------------
         * Send UDS response through ISO-TP.
         *--------------------------------------------------*/

        print_message(
            "UDS Response",
            response,
            response_length);


        if (isotp_send_message(
                socket_fd,
                response,
                response_length)
                != ISOTP_SUCCESS)
        {
            printf("ISO-TP response transmission failed\n");

            continue;
        }


        printf("UDS response sent successfully\n");


        /*---------------------------------------------------
         * If firmware download is complete, report it.
         *
         * We do NOT exit here.
         *
         * A real ECU would normally continue running and
         * could subsequently validate/program/reset.
         *--------------------------------------------------*/

        if (uds_get_download_state() ==
            UDS_DOWNLOAD_COMPLETE)
        {
            printf("\n");
            printf("============================================\n");
            printf(" FIRMWARE DOWNLOAD COMPLETE\n");
            printf("============================================\n");

            printf("Firmware size : %zu bytes\n",
                   uds_get_expected_firmware_size());

            printf("Received      : %zu bytes\n",
                   uds_get_received_firmware_size());

            printf("============================================\n");
        }
    }


    /*-------------------------------------------------------
     * Cleanup
     *
     * Normally unreachable because the target remains
     * active waiting for diagnostic requests.
     *------------------------------------------------------*/

    uds_download_reset();

    can_socket_close(socket_fd);

    return 0;
}
