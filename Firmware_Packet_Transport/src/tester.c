/******************************************************************************
 * File        : tester.c
 * Description : Firmware Sender (Tester ECU)
 ******************************************************************************/

#include <stdio.h>
#include <string.h>

#include "firmware_sender.h"
#include "packet.h"
#include "can_common.h"

int main(void)
{
    int socket_fd;

    packet_t packet;

    /*-------------------------------------------------------
     * Step 1 : Initialize CAN
     *------------------------------------------------------*/

    socket_fd = can_socket_init();

    if(socket_fd < 0)
    {
        printf("CAN initialization failed\n");

        return -1;
    }

    printf("CAN Initialized Successfully\n\n");

    /*-------------------------------------------------------
     * Step 2 : Load Firmware
     *------------------------------------------------------*/

    if(firmware_load("firmware.bin") != FW_SUCCESS)
    {
        can_socket_close(socket_fd);

        return -1;
    }

    printf("Firmware Size : %zu Bytes\n\n",
            firmware_get_size());

    /*-------------------------------------------------------
     * Step 3 : Initialize Packetizer
     *------------------------------------------------------*/

    packet_init(firmware_get_buffer(),
                firmware_get_size());

    /*-------------------------------------------------------
     * Step 4 : Generate and Send Packets
     *------------------------------------------------------*/

    while(packet_has_more())
    {
        if(packet_get_next(&packet) == PACKET_SUCCESS)
        {
            printf("---------------------------------\n");

            printf("Packet Number : %u\n",
                    packet.sequence);

            printf("Packet Length : %u Bytes\n",
                    packet.length);

            printf("First Byte    : %02X\n",
                    packet.data[0]);

            printf("Last Byte     : %02X\n",
                    packet.data[packet.length - 1]);

            /* Send packet over CAN */

            if(can_send_packet(socket_fd, &packet)
                    != CAN_SUCCESS)
            {
                printf("Packet %u send failed\n",
                        packet.sequence);

                break;
            }

            printf("Packet %u Sent Successfully\n\n",
                    packet.sequence);
        }
    }

    /*-------------------------------------------------------
     * Step 5 : Cleanup
     *------------------------------------------------------*/

    firmware_unload();

    can_socket_close(socket_fd);

    return 0;
}
