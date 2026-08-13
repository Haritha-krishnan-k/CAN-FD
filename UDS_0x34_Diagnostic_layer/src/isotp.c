#include "isotp.h"
#include "firmware_receiver.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*-----------------------------------------------------------
 * ISO-TP CAN ID
 *----------------------------------------------------------*/

#define ISOTP_TX_ID   0x700
#define ISOTP_RX_ID   0x708

/*-----------------------------------------------------------
 * ISO-TP timing
 *----------------------------------------------------------*/

#define ISOTP_TIMEOUT_US   500000

/*===========================================================
 * Generic ISO-TP Message Transport
 *
 * Used by UDS for messages such as:
 *
 * Request:
 *      10 01
 *
 * ISO-TP Single Frame:
 *      02 10 01
 *
 * Response:
 *      50 01
 *
 * ISO-TP Single Frame:
 *      02 50 01
 *==========================================================*/

/*-----------------------------------------------------------
 * Send Generic ISO-TP Message
 *----------------------------------------------------------*/

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

    /*
     * For now this generic function handles
     * ISO-TP Single Frames.
     *
     * CAN-FD allows a much larger payload, but
     * we first implement the small UDS messages.
     */

    if (length > (ISOTP_CANFD_MAX_DATA - 1))
    {
        printf("Message too large for Single Frame\n");
        return ISOTP_FAILURE;
    }

    memset(&frame, 0, sizeof(frame));

    frame.id = ISOTP_TX_ID;

    /*
     * Single Frame PCI
     *
     * Upper nibble = 0
     * Lower nibble = payload length
     *
     * Example:
     *
     * UDS request:
     *      10 01
     *
     * ISO-TP:
     *      02 10 01
     *
     * 02 = Single Frame
     * 02 = two bytes of UDS data
     */

    frame.data[0] = (uint8_t)(length & 0x0F);

    memcpy(&frame.data[1],
           data,
           length);

    frame.dlc = length + 1;

    if (can_send_frame(socket_fd,
                       &frame)
            != CAN_SUCCESS)
    {
        printf("Failed to send ISO-TP message\n");
        return ISOTP_FAILURE;
    }

    printf("ISO-TP message sent | %zu bytes\n",
           length);

    return ISOTP_SUCCESS;
}

/*-----------------------------------------------------------
 * Receive Generic ISO-TP Message
 *----------------------------------------------------------*/

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

    if (can_receive_frame(socket_fd,
                          &frame)
            != CAN_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    /*
     * We only process the expected ISO-TP
     * transmit CAN ID.
     */

    if (frame.id != ISOTP_TX_ID)
    {
        return ISOTP_FAILURE;
    }

    if (frame.dlc == 0)
    {
        return ISOTP_FAILURE;
    }

    /*
     * Extract PCI type.
     */

    uint8_t pci_type =
        frame.data[0] & 0xF0;

    /*-------------------------------------------------------
     * Single Frame
     *------------------------------------------------------*/

    if (pci_type == ISOTP_SINGLE_FRAME)
    {
        size_t payload_length =
            frame.data[0] & 0x0F;

        /*
         * Make sure the received frame actually
         * contains the advertised payload.
         */

        if (payload_length >
            (frame.dlc - 1))
        {
            printf("Invalid Single Frame length\n");
            return ISOTP_FAILURE;
        }

        /*
         * Make sure the destination buffer
         * is large enough.
         */

        if (payload_length > buffer_size)
        {
            printf("ISO-TP receive buffer too small\n");
            return ISOTP_FAILURE;
        }

        /*
         * Copy UDS payload.
         *
         * Example:
         *
         * CAN:
         *      02 10 01
         *
         * buffer:
         *      10 01
         */

        memcpy(buffer,
               &frame.data[1],
               payload_length);

        *received_size = payload_length;

        printf("ISO-TP Single Frame received | "
               "%zu bytes\n",
               payload_length);

        return ISOTP_SUCCESS;
    }

    /*
     * First Frame / Consecutive Frame support
     * for generic UDS messages will be added
     * when larger UDS requests are required.
     */

    printf("Unsupported ISO-TP frame type\n");

    return ISOTP_FAILURE;
}

/*===========================================================
 * Existing Firmware Transfer Implementation
 *==========================================================*/

/*-----------------------------------------------------------
 * Send First Frame
 *
 * For CAN-FD:
 *
 * Byte 0:
 * 0x10 = First Frame
 *
 * Byte 1:
 * upper length information
 *
 * For the large 131072-byte firmware, use
 * the extended 32-bit length format.
 *----------------------------------------------------------*/

static int isotp_send_first_frame(int socket_fd,
                                  const uint8_t *firmware,
                                  size_t firmware_size,
                                  size_t *offset)
{
    can_packet_t frame;

    memset(&frame, 0, sizeof(frame));

    frame.id = ISOTP_TX_ID;

    /*
     * Extended First Frame format:
     *
     * 0x10 0x00
     * followed by 32-bit length
     */

    frame.data[0] = 0x10;
    frame.data[1] = 0x00;

    frame.data[2] = (firmware_size >> 24) & 0xFF;
    frame.data[3] = (firmware_size >> 16) & 0xFF;
    frame.data[4] = (firmware_size >> 8)  & 0xFF;
    frame.data[5] = firmware_size & 0xFF;

    /*
     * Remaining bytes of the CAN-FD frame
     * contain the first firmware bytes.
     */

    size_t payload_length =
        ISOTP_CANFD_MAX_DATA - 6;

    if (payload_length > firmware_size)
    {
        payload_length = firmware_size;
    }

    memcpy(&frame.data[6],
           firmware,
           payload_length);

    frame.dlc = 6 + payload_length;

    if (can_send_frame(socket_fd,
                       &frame)
            != CAN_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    *offset = payload_length;

    printf("ISO-TP First Frame sent\n");

    printf("Firmware Size : %zu Bytes\n",
           firmware_size);

    printf("First Frame Data : %zu Bytes\n",
           payload_length);

    return ISOTP_SUCCESS;
}

/*-----------------------------------------------------------
 * Wait for Flow Control
 *----------------------------------------------------------*/

static int isotp_wait_for_flow_control(int socket_fd)
{
    can_packet_t frame;

    while (1)
    {
        if (can_receive_frame(socket_fd,
                              &frame)
                != CAN_SUCCESS)
        {
            return ISOTP_FAILURE;
        }

        /*
         * We only care about target's CAN ID.
         */

        if (frame.id != ISOTP_RX_ID)
        {
            continue;
        }

        /*
         * Check PCI type.
         */

        if ((frame.data[0] & 0xF0)
                != ISOTP_FLOW_CONTROL)
        {
            continue;
        }

        /*
         * Flow Status
         *
         * 0x30 = Continue To Send
         * 0x31 = Wait
         * 0x32 = Overflow
         */

        uint8_t flow_status =
            frame.data[0] & 0x0F;

        if (flow_status == 0x00)
        {
            printf("Flow Control: Continue To Send\n");

            return ISOTP_SUCCESS;
        }

        if (flow_status == 0x01)
        {
            printf("Flow Control: WAIT\n");

            continue;
        }

        if (flow_status == 0x02)
        {
            printf("Flow Control: OVERFLOW\n");

            return ISOTP_FAILURE;
        }

        return ISOTP_FAILURE;
    }
}

/*-----------------------------------------------------------
 * Send firmware using ISO-TP
 *----------------------------------------------------------*/

int isotp_send_firmware(int socket_fd,
                        const uint8_t *firmware,
                        size_t firmware_size)
{
    size_t offset = 0;

    uint8_t sequence_number = 1;

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

    /*
     * STEP 1
     *
     * Send First Frame
     */

    if (isotp_send_first_frame(socket_fd,
                               firmware,
                               firmware_size,
                               &offset)
            != ISOTP_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    /*
     * STEP 2
     *
     * Wait for Flow Control
     */

    if (isotp_wait_for_flow_control(socket_fd)
            != ISOTP_SUCCESS)
    {
        printf("Flow Control failed\n");

        return ISOTP_FAILURE;
    }

    /*
     * STEP 3
     *
     * Send Consecutive Frames
     */

    while (offset < firmware_size)
    {
        can_packet_t frame;

        size_t remaining;
        size_t payload_length;

        memset(&frame, 0, sizeof(frame));

        frame.id = ISOTP_TX_ID;

        /*
         * PCI byte
         *
         * Upper nibble = 2
         * Lower nibble = sequence number
         */

        frame.data[0] =
            ISOTP_CONSECUTIVE_FRAME |
            (sequence_number & 0x0F);

        remaining = firmware_size - offset;

        /*
         * First byte is PCI.
         *
         * Remaining 63 bytes carry firmware.
         */

        payload_length =
            ISOTP_CANFD_MAX_DATA - 1;

        if (payload_length > remaining)
        {
            payload_length = remaining;
        }

        memcpy(&frame.data[1],
               &firmware[offset],
               payload_length);

        frame.dlc = payload_length + 1;

        if (can_send_frame(socket_fd,
                           &frame)
                != CAN_SUCCESS)
        {
            printf("Failed to send CF %u\n",
                   sequence_number);

            return ISOTP_FAILURE;
        }

        printf("CF %u sent | %zu bytes\n",
               sequence_number,
               payload_length);

        offset += payload_length;

        /*
         * ISO-TP sequence number:
         *
         * 1,2,3,...,15,0,1,2...
         */

        sequence_number++;

        sequence_number &= 0x0F;

        /*
         * Simple separation time.
         *
         * This can later be replaced
         * by STmin handling.
         */

        usleep(1000);
    }

    printf("\nISO-TP transmission complete\n");

    printf("Total firmware bytes sent : %zu\n",
           firmware_size);

    return ISOTP_SUCCESS;
}

/*-----------------------------------------------------------
 * Receive First Frame
 *----------------------------------------------------------*/

static int isotp_receive_first_frame(int socket_fd,
                                     uint8_t *buffer,
                                     size_t buffer_size,
                                     size_t *total_size,
                                     size_t *received)
{
    can_packet_t frame;

    if (can_receive_frame(socket_fd,
                          &frame)
            != CAN_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    if (frame.id != ISOTP_TX_ID)
    {
        return ISOTP_FAILURE;
    }

    /*
     * Check First Frame
     */

    if ((frame.data[0] & 0xF0)
            != ISOTP_FIRST_FRAME)
    {
        printf("Expected First Frame\n");

        return ISOTP_FAILURE;
    }

    /*
     * Extended 32-bit firmware length.
     */

    if (frame.data[1] != 0x00)
    {
        printf("Unsupported First Frame format\n");

        return ISOTP_FAILURE;
    }

    *total_size =
        ((size_t)frame.data[2] << 24) |
        ((size_t)frame.data[3] << 16) |
        ((size_t)frame.data[4] << 8)  |
        ((size_t)frame.data[5]);

    printf("ISO-TP First Frame received\n");

    printf("Expected firmware size : %zu Bytes\n",
           *total_size);

    if (*total_size > buffer_size)
    {
        printf("Firmware is larger than receive buffer\n");

        return ISOTP_FAILURE;
    }

    /*
     * First Frame contains firmware bytes.
     */

    size_t payload_length = frame.dlc - 6;

    if (payload_length > *total_size)
    {
        payload_length = *total_size;
    }

    memcpy(buffer,
           &frame.data[6],
           payload_length);

    *received = payload_length;

    return ISOTP_SUCCESS;
}

/*-----------------------------------------------------------
 * Send Flow Control
 *----------------------------------------------------------*/

static int isotp_send_flow_control(int socket_fd)
{
    can_packet_t frame;

    memset(&frame, 0, sizeof(frame));

    frame.id = ISOTP_RX_ID;

    /*
     * 0x30 = Flow Control
     *
     * FS = 0
     * Continue To Send
     */

    frame.data[0] = 0x30;

    /*
     * Block Size = 0
     */

    frame.data[1] = 0x00;

    /*
     * STmin = 0
     */

    frame.data[2] = 0x00;

    frame.dlc = 3;

    if (can_send_frame(socket_fd,
                       &frame)
            != CAN_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    printf("Flow Control sent: 30 00 00\n");

    return ISOTP_SUCCESS;
}

/*-----------------------------------------------------------
 * Receive Consecutive Frames
 *----------------------------------------------------------*/

static int isotp_receive_consecutive_frames(int socket_fd,
                                            uint8_t *buffer,
                                            size_t total_size,
                                            size_t received)
{
    uint8_t expected_sequence = 1;

    while (received < total_size)
    {
        can_packet_t frame;

        if (can_receive_frame(socket_fd,
                              &frame)
                != CAN_SUCCESS)
        {
            return ISOTP_FAILURE;
        }

        if (frame.id != ISOTP_TX_ID)
        {
            continue;
        }

        /*
         * Must be Consecutive Frame
         */

        if ((frame.data[0] & 0xF0)
                != ISOTP_CONSECUTIVE_FRAME)
        {
            continue;
        }

        uint8_t sequence =
            frame.data[0] & 0x0F;

        /*
         * Check sequence number
         */

        if (sequence != expected_sequence)
        {
            printf("Sequence error\n");

            printf("Expected : %u\n",
                   expected_sequence);

            printf("Received : %u\n",
                   sequence);

            return ISOTP_FAILURE;
        }

        /*
         * One byte PCI.
         */

        size_t payload_length =
            frame.dlc - 1;

        /*
         * Don't copy more than the
         * remaining firmware size.
         */

        if (payload_length >
            (total_size - received))
        {
            payload_length =
                total_size - received;
        }

        memcpy(&buffer[received],
               &frame.data[1],
               payload_length);

        received += payload_length;

        printf("CF %u received | %zu bytes | "
               "Total %zu / %zu\n",
               sequence,
               payload_length,
               received,
               total_size);

        expected_sequence++;

        expected_sequence &= 0x0F;
    }

    return ISOTP_SUCCESS;
}

/*-----------------------------------------------------------
 * Receive complete firmware
 *----------------------------------------------------------*/

int isotp_receive_firmware(int socket_fd,
                           uint8_t *buffer,
                           size_t buffer_size,
                           size_t *received_size)
{
    size_t total_size = 0;

    size_t received = 0;

    if (buffer == NULL ||
        received_size == NULL)
    {
        return ISOTP_FAILURE;
    }

    /*
     * STEP 1
     *
     * Receive First Frame
     */

    if (isotp_receive_first_frame(socket_fd,
                                  buffer,
                                  buffer_size,
                                  &total_size,
                                  &received)
            != ISOTP_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    /*
     * STEP 2
     *
     * Send Flow Control
     */

    if (isotp_send_flow_control(socket_fd)
            != ISOTP_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    /*
     * STEP 3
     *
     * Receive all Consecutive Frames
     */

    if (isotp_receive_consecutive_frames(socket_fd,
                                         buffer,
                                         total_size,
                                         received)
            != ISOTP_SUCCESS)
    {
        return ISOTP_FAILURE;
    }

    *received_size = total_size;

    printf("\n");
    printf("=================================\n");
    printf("ISO-TP Reception Complete\n");
    printf("=================================\n");

    printf("Total received : %zu Bytes\n",
           total_size);

    return ISOTP_SUCCESS;
}

