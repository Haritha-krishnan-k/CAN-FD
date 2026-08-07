/******************************************************************************
 * File        : target.c
 * Description : Firmware Receiver (Target ECU)
 ******************************************************************************/

#include <stdio.h>

#include "packet.h"
#include "firmware_receiver.h"
#include "can_common.h"
#include "crc.h"

#define EXPECTED_FW_SIZE   (128 * 1024)   /* 131072 Bytes */

int main(void)
{
    packet_t packet;
    int socket_fd ;
    /*-------------------------------------------------------
     * Step 1 : Initialize CAN
     *------------------------------------------------------*/
    socket_fd = can_socket_init();

    if (socket_fd < 0 )
    {
        printf("CAN initialization failed\n");
        return -1;
    }

    printf("CAN Initialized\n");

    /*-------------------------------------------------------
     * Step 2 : Initialize Firmware Receiver
     *------------------------------------------------------*/

    if (firmware_receiver_init(EXPECTED_FW_SIZE)
            != FW_RECEIVER_SUCCESS)
    {
        printf("Receiver initialization failed\n");
        return -1;
    }

    printf("Firmware Receiver Initialized\n");
    printf("Waiting for firmware packets...\n\n");

    /*-------------------------------------------------------
     * Step 3 : Receive Packets
     *------------------------------------------------------*/

    while (1)
    {
        /*
         * Receive one packet from CAN.
         * This function will be implemented in can_common.c
         */
        if (can_receive_packet(socket_fd,&packet) != CAN_SUCCESS)
        {
            continue;
        }

        printf("---------------------------------\n");
        printf("Packet Number : %u\n", packet.sequence);
        printf("Packet Length : %u Bytes\n", packet.length);
        printf("First Byte    : %02X\n", packet.data[0]);
        printf("Last Byte     : %02X\n",
               packet.data[packet.length - 1]);

        /* Store packet */

        if (firmware_receiver_store(&packet)
                != FW_RECEIVER_SUCCESS)
        {
            printf("Failed to store packet %u\n",
                   packet.sequence);
            break;
        }

        /* Check whether firmware is complete */

        if (firmware_receiver_complete())
        {
            break;
        }

    }

    /*-------------------------------------------------------
     * Step 4 : Print Receiver Information
     *------------------------------------------------------*/

    printf("\n");
    printf("=================================\n");
    printf("Firmware Receiver Result\n");
    printf("=================================\n");

    printf("Expected Firmware Size : %zu Bytes\n",
           firmware_receiver_get_size());

    printf("Received Bytes         : %zu Bytes\n",
           firmware_receiver_get_received_bytes());

    if (firmware_receiver_complete())
    {
        printf("Firmware Reassembled Successfully\n");
    }
    else
    {
        printf("Firmware Reassembly Failed\n");
    }

    /*-------------------------------------------------------
     * Step 5 : CRC Verification (Next Step)
     *------------------------------------------------------*/

    /*
     * uint32_t crc = crc_calculate(
     *         firmware_receiver_get_buffer(),
     *         firmware_receiver_get_size());
     *
     * printf("CRC : %08X\n", crc);
     */

    /*-------------------------------------------------------
     * Step 6 : Cleanup
     *------------------------------------------------------*/

    firmware_receiver_deinit();

    can_socket_close(socket_fd);

    return 0;
}
