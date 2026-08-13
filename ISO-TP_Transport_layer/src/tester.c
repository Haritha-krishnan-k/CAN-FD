/******************************************************************************
 * File        : tester.c
 * Description : ISO-TP Firmware Tester / Sender
 ******************************************************************************/

#include <stdio.h>

#include "firmware_sender.h"
#include "isotp.h"
#include "can_common.h"


int main(void)
{
    int socket_fd;

    /*-------------------------------------------------------
     * Step 1 : Initialize CAN-FD
     *------------------------------------------------------*/

    socket_fd = can_socket_init();

    if (socket_fd < 0)
    {
        printf("CAN initialization failed\n");
        return -1;
    }

    printf("CAN Initialized Successfully\n\n");


    /*-------------------------------------------------------
     * Step 2 : Load Firmware
     *------------------------------------------------------*/

    if (firmware_load("firmware.bin") != FW_SUCCESS)
    {
        printf("Firmware loading failed\n");

        can_socket_close(socket_fd);

        return -1;
    }

    printf("Firmware Loaded Successfully\n");

    printf("Firmware Size : %zu Bytes\n\n",
           firmware_get_size());


    /*-------------------------------------------------------
     * Step 3 : Start ISO-TP Transfer
     *------------------------------------------------------*/

    printf("Starting ISO-TP Firmware Transfer...\n\n");

    if (isotp_send_firmware(socket_fd,
                            firmware_get_buffer(),
                            firmware_get_size())
            != ISOTP_SUCCESS)
    {
        printf("\nISO-TP Firmware Transfer Failed\n");

        firmware_unload();

        can_socket_close(socket_fd);

        return -1;
    }


    /*-------------------------------------------------------
     * Step 4 : Transfer Successful
     *------------------------------------------------------*/

    printf("\n=================================\n");
    printf("ISO-TP Transfer Successful\n");
    printf("=================================\n");

    printf("Firmware Size : %zu Bytes\n",
           firmware_get_size());


    /*-------------------------------------------------------
     * Step 5 : Cleanup
     *------------------------------------------------------*/

    firmware_unload();

    can_socket_close(socket_fd);

    return 0;
}

