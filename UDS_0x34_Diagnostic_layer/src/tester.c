#include <stdio.h>
#include <stdint.h>

#include "isotp.h"
#include "can_common.h"

int main(void)
{
    int socket_fd;

    uint8_t request[] = {
        0x10,   /* Diagnostic Session Control */
        0x01    /* Default Session */
    };

    uint8_t response[64];
    size_t response_length = 0;

    /*-------------------------------------------------------
     * Step 1: Initialize CAN-FD
     *------------------------------------------------------*/

    socket_fd = can_socket_init();

    if (socket_fd < 0)
    {
        printf("CAN initialization failed\n");
        return -1;
    }

    printf("CAN Initialized Successfully\n\n");

    /*-------------------------------------------------------
     * Step 2: Send UDS Request
     *------------------------------------------------------*/

    printf("Sending UDS Request...\n");

    printf("Request: ");

    for (size_t i = 0; i < sizeof(request); i++)
    {
        printf("%02X ", request[i]);
    }

    printf("\n");

    if (isotp_send_message(socket_fd,
                           request,
                           sizeof(request))
            != ISOTP_SUCCESS)
    {
        printf("Failed to send UDS request\n");

        can_socket_close(socket_fd);

        return -1;
    }

    printf("UDS Request Sent Successfully\n");

    /*-------------------------------------------------------
     * Step 3: Receive UDS Response
     *------------------------------------------------------*/

    printf("\nWaiting for UDS Response...\n");

    if (isotp_receive_message(socket_fd,
                              response,
                              sizeof(response),
                              &response_length)
            != ISOTP_SUCCESS)
    {
        printf("Failed to receive UDS response\n");

        can_socket_close(socket_fd);

        return -1;
    }

    /*-------------------------------------------------------
     * Step 4: Print Response
     *------------------------------------------------------*/

    printf("UDS Response Received\n");

    printf("Response: ");

    for (size_t i = 0; i < response_length; i++)
    {
        printf("%02X ", response[i]);
    }

    printf("\n");

    /*-------------------------------------------------------
     * Step 5: Validate Response
     *------------------------------------------------------*/

    if (response_length >= 2 &&
        response[0] == 0x50 &&
        response[1] == 0x01)
    {
        printf("\n=================================\n");
        printf("UDS SESSION CONTROL SUCCESS\n");
        printf("Default Session Activated\n");
        printf("=================================\n");
    }
    else if (response_length >= 3 &&
             response[0] == 0x7F)
    {
        printf("\nUDS Negative Response\n");
        printf("Service      : 0x%02X\n", response[1]);
        printf("NRC          : 0x%02X\n", response[2]);
    }
    else
    {
        printf("\nUnexpected UDS response\n");
    }

    /*-------------------------------------------------------
     * Step 6: Cleanup
     *------------------------------------------------------*/

    can_socket_close(socket_fd);

    return 0;
}

