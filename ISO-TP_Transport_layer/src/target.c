#include <stdio.h>
#include <stdint.h>

#include "isotp.h"
#include "firmware_receiver.h"
#include "can_common.h"
#include "crc.h"

#define EXPECTED_FW_SIZE   (128 * 1024)

int main(void)
{
    int socket_fd;

    uint8_t *firmware_buffer;

    size_t received_size = 0;

    /*-------------------------------------------------------
     * Step 1: Initialize CAN
     *------------------------------------------------------*/

    socket_fd = can_socket_init();

    if (socket_fd < 0)
    {
        printf("CAN initialization failed\n");
        return -1;
    }

    printf("CAN Initialized\n");

    /*-------------------------------------------------------
     * Step 2: Initialize firmware receiver
     *------------------------------------------------------*/

    if (firmware_receiver_init(EXPECTED_FW_SIZE)
            != FW_RECEIVER_SUCCESS)
    {
        printf("Firmware receiver initialization failed\n");

        can_socket_close(socket_fd);

        return -1;
    }

    printf("Waiting for ISO-TP firmware...\n");

    /*-------------------------------------------------------
     * Step 3: Get receiver buffer
     *------------------------------------------------------*/

    firmware_buffer =
        firmware_receiver_get_buffer();

    if (firmware_buffer == NULL)
    {
        printf("Failed to get firmware buffer\n");

        firmware_receiver_deinit();
        can_socket_close(socket_fd);

        return -1;
    }

    /*-------------------------------------------------------
     * Step 4: Receive complete ISO-TP firmware
     *------------------------------------------------------*/

    if (isotp_receive_firmware(socket_fd,
                               firmware_buffer,
                               EXPECTED_FW_SIZE,
                               &received_size)
            != ISOTP_SUCCESS)
    {
        printf("ISO-TP firmware reception failed\n");

        firmware_receiver_deinit();
        can_socket_close(socket_fd);

        return -1;
    }

    /*-------------------------------------------------------
     * Step 5: Tell firmware receiver how much was received
     *------------------------------------------------------*/

    if (firmware_receiver_store_data(
            firmware_buffer,
            received_size)
            != FW_RECEIVER_SUCCESS)
    {
        printf("Failed to store received firmware\n");

        firmware_receiver_deinit();
        can_socket_close(socket_fd);

        return -1;
    }

    /*-------------------------------------------------------
     * Step 6: Check complete
     *------------------------------------------------------*/

    printf("\n");
    printf("=================================\n");
    printf("Firmware Reception Result\n");
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
     * Step 7: CRC
     *------------------------------------------------------*/

    /*
    uint32_t crc =
        crc_calculate(
            firmware_receiver_get_buffer(),
            firmware_receiver_get_received_bytes());

    printf("CRC : %08X\n", crc);
    */

    /*-------------------------------------------------------
     * Step 8: Cleanup
     *------------------------------------------------------*/

    firmware_receiver_deinit();

    can_socket_close(socket_fd);

    return 0;
}
