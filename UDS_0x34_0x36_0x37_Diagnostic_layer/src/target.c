#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "isotp.h"
#include "uds.h"
#include "can_common.h"

#define UDS_BUFFER_SIZE 4096

int main(void)
{
    int socket_fd;

    uint8_t request[UDS_BUFFER_SIZE];
    uint8_t response[UDS_BUFFER_SIZE];

    size_t request_length;
    size_t response_length;

    printf("====================================\n");
    printf("UDS Firmware Download Target\n");
    printf("====================================\n");

    /*
     * Initialize CAN-FD / SocketCAN
     */
    socket_fd = can_socket_init();

    if (socket_fd < 0)
    {
        printf("CAN initialization failed\n");
        return -1;
    }

    printf("CAN Initialized Successfully\n");
    printf("Waiting for UDS request...\n");

    while (1)
    {
        request_length = 0;
        response_length = 0;

        /*
         * Receive one complete ISO-TP message.
         */
        if (isotp_receive_message(socket_fd,
                                  request,
                                  sizeof(request),
                                  &request_length) != ISOTP_SUCCESS)
        {
            printf("ISO-TP request reception failed\n");
            continue;
        }

        printf("\nUDS Request received\n");
        printf("Request length : %zu Bytes\n", request_length);

        /*
         * Process the UDS request.
         */
        if (uds_process_request(request,
                                request_length,
                                response,
                                &response_length) != UDS_SUCCESS)
        {
            printf("UDS request processing failed\n");
            continue;
        }

        /*
         * Send UDS response.
         */
        if (isotp_send_message(socket_fd,
                               response,
                               response_length) != ISOTP_SUCCESS)
        {
            printf("ISO-TP response transmission failed\n");
            continue;
        }

        printf("UDS response sent\n");
    }

    can_socket_close(socket_fd);

    return 0;
}
