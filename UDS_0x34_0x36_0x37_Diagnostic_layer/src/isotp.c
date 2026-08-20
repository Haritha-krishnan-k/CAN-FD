#include "isotp.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

/*-----------------------------------------------------------
 * ISO-TP CAN IDs
 *----------------------------------------------------------*/

#define ISOTP_TX_ID   0x700
#define ISOTP_RX_ID   0x708

/*-----------------------------------------------------------
 * ISO-TP parameters
 *----------------------------------------------------------*/

#define ISOTP_CANFD_PAYLOAD_SIZE 64

#define ISOTP_TIMEOUT_US         500000

#define ISOTP_MAX_FIRMWARE_SIZE  (1024 * 1024)

/*===========================================================
 * Helper: Send one CAN-FD frame
 *==========================================================*/

static int isotp_send_can_frame(int socket_fd,
                                uint32_t can_id,
                                const uint8_t *data,
                                size_t length)
{
    can_packet_t frame;

    if (data == NULL)
    {
        return ISOTP_FAILURE;
    }

    if (length == 0 ||
        length > ISOTP_CANFD_PAYLOAD_SIZE)
    {
        return ISOTP_FAILURE;
    }

    memset(&frame, 0, sizeof(frame));

    frame.id = can_id;
    frame.dlc = (uint8_t)length;

    memcpy(frame.data,
           data,
           length);

    if (can_send_frame(socket_fd,
                       &frame) != CAN_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    return ISOTP_SUCCESS;
}

/*===========================================================
 * Generic ISO-TP SEND
 *
 * Small message:
 *
 *      10 02
 *
 * becomes:
 *
 *      02 10 02
 *
 * Large message:
 *
 *      36 01 <firmware data...>
 *
 * becomes:
 *
 *      First Frame
 *      Flow Control
 *      Consecutive Frames
 *==========================================================*/

int isotp_send_message(int socket_fd,
                       const uint8_t *data,
                       size_t length)
{
    can_packet_t frame;

    if (data == NULL)
    {
        return ISOTP_FAILURE;
    }

    if (length == 0)
    {
        return ISOTP_FAILURE;
    }

    /*-------------------------------------------------------
     * SINGLE FRAME
     *
     * CAN-FD allows up to 63 bytes of payload because
     * byte 0 contains the ISO-TP PCI.
     *------------------------------------------------------*/

    if (length <= (ISOTP_CANFD_PAYLOAD_SIZE - 1))
    {
        memset(&frame, 0, sizeof(frame));

        frame.id = ISOTP_TX_ID;

        /*
         * Single Frame PCI
         *
         * Upper nibble = 0
         * Lower nibble = payload length
         */

        frame.data[0] =
            (uint8_t)(length & 0x0F);

        memcpy(&frame.data[1],
               data,
               length);

        frame.dlc =
            (uint8_t)(length + 1);

        if (can_send_frame(socket_fd,
                           &frame)
                != CAN_SUCCESS)
        {
            printf("Failed to send ISO-TP Single Frame\n");

            return ISOTP_FAILURE;
        }

        printf("ISO-TP Single Frame sent | %zu bytes\n",
               length);

        return ISOTP_SUCCESS;
    }

    /*-------------------------------------------------------
     * MULTI-FRAME
     *------------------------------------------------------*/

    size_t offset = 0;

    uint8_t sequence_number = 1;

    /*-------------------------------------------------------
     * First Frame
     *
     * For CAN-FD extended length:
     *
     * Byte 0 = 0x10
     * Byte 1 = 0x00
     * Byte 2-5 = 32-bit message length
     *
     * Bytes 6-63 = first data
     *------------------------------------------------------*/

    memset(&frame, 0, sizeof(frame));

    frame.id = ISOTP_TX_ID;

    frame.data[0] = 0x10;
    frame.data[1] = 0x00;

    frame.data[2] =
        (uint8_t)((length >> 24) & 0xFF);

    frame.data[3] =
        (uint8_t)((length >> 16) & 0xFF);

    frame.data[4] =
        (uint8_t)((length >> 8) & 0xFF);

    frame.data[5] =
        (uint8_t)(length & 0xFF);

    size_t first_payload =
        ISOTP_CANFD_PAYLOAD_SIZE - 6;

    if (first_payload > length)
    {
        first_payload = length;
    }

    memcpy(&frame.data[6],
           data,
           first_payload);

    frame.dlc =
        (uint8_t)(6 + first_payload);

    if (can_send_frame(socket_fd,
                       &frame)
            != CAN_SUCCESS)
    {
        printf("Failed to send ISO-TP First Frame\n");

        return ISOTP_FAILURE;
    }

    offset = first_payload;

    printf("ISO-TP First Frame sent | "
           "Total message = %zu bytes | "
           "First payload = %zu bytes\n",
           length,
           first_payload);

    /*-------------------------------------------------------
     * Wait for Flow Control
     *------------------------------------------------------*/

    while (1)
    {
        can_packet_t fc_frame;

        if (can_receive_frame(socket_fd,
                              &fc_frame)
                != CAN_SUCCESS)
        {
            printf("Failed waiting for Flow Control\n");

            return ISOTP_FAILURE;
        }

        /*
         * Target -> Tester
         */

        if (fc_frame.id != ISOTP_RX_ID)
        {
            continue;
        }

        /*
         * Flow Control PCI
         */

        if ((fc_frame.data[0] & 0xF0)
                != ISOTP_FLOW_CONTROL)
        {
            continue;
        }

        uint8_t flow_status =
            fc_frame.data[0] & 0x0F;

        uint8_t block_size =
            fc_frame.data[1];

        uint8_t stmin =
            fc_frame.data[2];

        /*---------------------------------------------------
         * Continue To Send
         *--------------------------------------------------*/

        if (flow_status == 0x00)
        {
            printf("Flow Control: CTS\n");
            printf("Block Size : %u\n",
                   block_size);
            printf("STmin      : %u\n",
                   stmin);

            break;
        }

        /*---------------------------------------------------
         * WAIT
         *--------------------------------------------------*/

        if (flow_status == 0x01)
        {
            printf("Flow Control: WAIT\n");

            continue;
        }

        /*---------------------------------------------------
         * OVERFLOW
         *--------------------------------------------------*/

        if (flow_status == 0x02)
        {
            printf("Flow Control: OVERFLOW\n");

            return ISOTP_FAILURE;
        }

        printf("Unknown Flow Status: 0x%02X\n",
               flow_status);

        return ISOTP_FAILURE;
    }

    /*-------------------------------------------------------
     * Consecutive Frames
     *------------------------------------------------------*/

    while (offset < length)
    {
        memset(&frame, 0, sizeof(frame));

        frame.id = ISOTP_TX_ID;

        /*
         * Consecutive Frame PCI
         *
         * Upper nibble = 2
         * Lower nibble = sequence number
         */

        frame.data[0] =
            ISOTP_CONSECUTIVE_FRAME |
            (sequence_number & 0x0F);

        size_t remaining =
            length - offset;

        size_t payload_length =
            ISOTP_CANFD_PAYLOAD_SIZE - 1;

        if (payload_length > remaining)
        {
            payload_length = remaining;
        }

        memcpy(&frame.data[1],
               &data[offset],
               payload_length);

        frame.dlc =
            (uint8_t)(payload_length + 1);

        if (can_send_frame(socket_fd,
                           &frame)
                != CAN_SUCCESS)
        {
            printf("Failed to send CF %u\n",
                   sequence_number);

            return ISOTP_FAILURE;
        }

        printf("CF %u sent | %zu bytes | "
               "Total %zu / %zu\n",
               sequence_number,
               payload_length,
               offset + payload_length,
               length);

        offset += payload_length;

        sequence_number++;

        sequence_number &= 0x0F;

        /*
         * Simple delay.
         *
         * Later this can be replaced with
         * proper STmin handling.
         */

        usleep(1000);
    }

    printf("ISO-TP multi-frame transmission complete\n");
    printf("Total bytes sent : %zu\n",
           length);

    return ISOTP_SUCCESS;
}

/*===========================================================
 * Generic ISO-TP RECEIVE
 *
 * Supports:
 *
 * 1. Single Frame
 * 2. First Frame
 * 3. Flow Control
 * 4. Consecutive Frames
 *==========================================================*/

int isotp_receive_message(int socket_fd,
                          uint8_t *buffer,
                          size_t buffer_size,
                          size_t *received_size)
{
    can_packet_t frame;

    if (buffer == NULL ||
        received_size == NULL)
    {
        return ISOTP_FAILURE;
    }

    *received_size = 0;

    /*-------------------------------------------------------
     * Receive first frame
     *------------------------------------------------------*/

    while (1)
    {
        if (can_receive_frame(socket_fd,
                              &frame)
                != CAN_SUCCESS)
        {
            return ISOTP_FAILURE;
        }

        /*
         * Target and tester currently use the same
         * receive path. Ignore unrelated CAN IDs.
         */

        if (frame.id != ISOTP_TX_ID)
        {
            continue;
        }

        break;
    }

    if (frame.dlc == 0)
    {
        return ISOTP_FAILURE;
    }

    uint8_t pci_type =
        frame.data[0] & 0xF0;

    /*=======================================================
     * SINGLE FRAME
     *======================================================*/

    if (pci_type == ISOTP_SINGLE_FRAME)
    {
        size_t payload_length =
            frame.data[0] & 0x0F;

        if (frame.dlc < 1)
        {
            return ISOTP_FAILURE;
        }

        if (payload_length >
            ((size_t)frame.dlc - 1))
        {
            printf("Invalid Single Frame length\n");

            return ISOTP_FAILURE;
        }

        if (payload_length >
            buffer_size)
        {
            printf("ISO-TP receive buffer too small\n");

            return ISOTP_FAILURE;
        }

        memcpy(buffer,
               &frame.data[1],
               payload_length);

        *received_size =
            payload_length;

        printf("ISO-TP Single Frame received | "
               "%zu bytes\n",
               payload_length);

        return ISOTP_SUCCESS;
    }

    /*=======================================================
     * FIRST FRAME
     *======================================================*/

    if (pci_type == ISOTP_FIRST_FRAME)
    {
        size_t total_length;

        /*
         * Extended 32-bit length format:
         *
         * 10 00 XX XX XX XX
         */

        if (frame.data[1] != 0x00)
        {
            printf("Unsupported First Frame format\n");

            return ISOTP_FAILURE;
        }

        total_length =
            ((size_t)frame.data[2] << 24) |
            ((size_t)frame.data[3] << 16) |
            ((size_t)frame.data[4] << 8)  |
            ((size_t)frame.data[5]);

        printf("ISO-TP First Frame received\n");
        printf("Expected message size : %zu bytes\n",
               total_length);

        if (total_length == 0)
        {
            return ISOTP_FAILURE;
        }

        if (total_length >
            buffer_size)
        {
            printf("ISO-TP buffer too small\n");

            return ISOTP_FAILURE;
        }

        if (frame.dlc < 6)
        {
            printf("Invalid First Frame DLC\n");

            return ISOTP_FAILURE;
        }

        /*
         * First Frame data begins at byte 6.
         */

        size_t payload_length =
            (size_t)frame.dlc - 6;

        if (payload_length > total_length)
        {
            payload_length = total_length;
        }

        memcpy(buffer,
               &frame.data[6],
               payload_length);

        size_t received =
            payload_length;

        /*---------------------------------------------------
         * Send Flow Control
         *--------------------------------------------------*/

        can_packet_t fc;

        memset(&fc, 0, sizeof(fc));

        fc.id = ISOTP_RX_ID;

        /*
         * 30 = Flow Control
         * 00 = Continue To Send
         * 00 = STmin
         */

        fc.data[0] = 0x30;
        fc.data[1] = 0x00;
        fc.data[2] = 0x00;

        fc.dlc = 3;

        if (can_send_frame(socket_fd,
                           &fc)
                != CAN_SUCCESS)
        {
            printf("Failed to send Flow Control\n");

            return ISOTP_FAILURE;
        }

        printf("Flow Control sent: 30 00 00\n");

        /*---------------------------------------------------
         * Receive Consecutive Frames
         *--------------------------------------------------*/

        uint8_t expected_sequence = 1;

        while (received < total_length)
        {
            can_packet_t cf;

            if (can_receive_frame(socket_fd,
                                  &cf)
                    != CAN_SUCCESS)
            {
                return ISOTP_FAILURE;
            }

            /*
             * Only accept tester frames.
             */

            if (cf.id != ISOTP_TX_ID)
            {
                continue;
            }

            /*
             * Must be Consecutive Frame.
             */

            if ((cf.data[0] & 0xF0)
                    != ISOTP_CONSECUTIVE_FRAME)
            {
                printf("Expected Consecutive Frame\n");

                return ISOTP_FAILURE;
            }

            uint8_t sequence =
                cf.data[0] & 0x0F;

            /*------------------------------------------------
             * Sequence check
             *------------------------------------------------*/

            if (sequence != expected_sequence)
            {
                printf("ISO-TP sequence error\n");

                printf("Expected : %u\n",
                       expected_sequence);

                printf("Received : %u\n",
                       sequence);

                return ISOTP_FAILURE;
            }

            if (cf.dlc < 1)
            {
                return ISOTP_FAILURE;
            }

            size_t cf_payload =
                (size_t)cf.dlc - 1;

            size_t remaining =
                total_length - received;

            if (cf_payload > remaining)
            {
                cf_payload = remaining;
            }

            memcpy(&buffer[received],
                   &cf.data[1],
                   cf_payload);

            received += cf_payload;

            printf("CF %u received | "
                   "%zu bytes | "
                   "Total %zu / %zu\n",
                   sequence,
                   cf_payload,
                   received,
                   total_length);

            expected_sequence++;

            expected_sequence &= 0x0F;
        }

        *received_size = total_length;

        printf("\nISO-TP multi-frame reception complete\n");
        printf("Total bytes received : %zu\n",
               total_length);

        return ISOTP_SUCCESS;
    }

    printf("Unsupported ISO-TP frame type: 0x%02X\n",
           pci_type);

    return ISOTP_FAILURE;
}

/*===========================================================
 * Firmware Transmission
 *
 * Kept as a convenience API.
 *
 * The actual firmware is sent as one ISO-TP message.
 *==========================================================*/

int isotp_send_firmware(int socket_fd,
                        const uint8_t *firmware,
                        size_t firmware_size)
{
    if (firmware == NULL)
    {
        return ISOTP_FAILURE;
    }

    if (firmware_size == 0)
    {
        return ISOTP_FAILURE;
    }

    printf("\n");
    printf("=================================\n");
    printf("ISO-TP Firmware Transmission\n");
    printf("=================================\n");

    printf("Firmware size : %zu bytes\n",
           firmware_size);

    /*
     * Send the complete firmware as one
     * ISO-TP multi-frame message.
     */

    return isotp_send_message(socket_fd,
                              firmware,
                              firmware_size);
}

/*===========================================================
 * Firmware Reception
 *
 * Receives one complete ISO-TP message into buffer.
 *==========================================================*/

int isotp_receive_firmware(int socket_fd,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *received_size)
{
    if (buffer == NULL ||
        received_size == NULL)
    {
        return ISOTP_FAILURE;
    }

    printf("\n");
    printf("=================================\n");
    printf("ISO-TP Firmware Reception\n");
    printf("=================================\n");

    if (isotp_receive_message(socket_fd,
                              buffer,
                              buffer_size,
                              received_size)
            != ISOTP_SUCCESS)
    {
        printf("ISO-TP firmware reception failed\n");

        return ISOTP_FAILURE;
    }

    printf("\n");
    printf("=================================\n");
    printf("ISO-TP Firmware Reception Complete\n");
    printf("=================================\n");

    printf("Total received : %zu Bytes\n",
           *received_size);

    return ISOTP_SUCCESS;
}
