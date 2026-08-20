#include "uds.h"

#include "firmware_receiver.h"

#include <stdio.h>
#include <string.h>


/*-----------------------------------------------------------
 * Current Diagnostic Session
 *----------------------------------------------------------*/

static uint8_t current_session = UDS_SESSION_DEFAULT;


/*-----------------------------------------------------------
 * Download State
 *----------------------------------------------------------*/

static uds_download_state_t download_state =
        UDS_DOWNLOAD_IDLE;


/*
 * Expected firmware size.
 *
 * This value comes from the RequestDownload request.
 */

static size_t expected_firmware_size = 0;


/*
 * Number of firmware bytes received so far.
 */

static size_t received_firmware_size = 0;


/*
 * Next expected TransferData block counter.
 */

static uint8_t expected_block_counter = 1;


/*-----------------------------------------------------------
 * Helper: Generate Negative Response
 *----------------------------------------------------------*/

static void uds_negative_response(
        uint8_t sid,
        uint8_t nrc,
        uint8_t *response,
        size_t *response_length)
{
    response[0] = UDS_NEGATIVE_RESPONSE;
    response[1] = sid;
    response[2] = nrc;

    *response_length = 3;
}


/*-----------------------------------------------------------
 * Diagnostic Session Control
 *
 * Request:
 *
 *     10 01
 *
 *     Default Session
 *
 * or
 *
 *     10 02
 *
 *     Programming Session
 *
 * Positive response:
 *
 *     50 01
 *
 *     50 02
 *
 *----------------------------------------------------------*/

static int uds_handle_session_control(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    if (request_length != 2)
    {
        printf("UDS: Session Control - incorrect length\n");

        uds_negative_response(
                UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    if (request[1] != UDS_SESSION_DEFAULT &&
        request[1] != UDS_SESSION_PROGRAMMING)
    {
        printf("UDS: Unsupported session 0x%02X\n",
               request[1]);

        uds_negative_response(
                UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                UDS_RESPONSE_REQUEST_OUT_OF_RANGE,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    current_session = request[1];


    printf("\nUDS: Diagnostic Session Control\n");
    printf("UDS: Session = 0x%02X\n",
           current_session);


    response[0] =
        UDS_SID_DIAGNOSTIC_SESSION_CONTROL +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] = current_session;

    *response_length = 2;


    printf("UDS: Response = 50 %02X\n",
           current_session);


    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * ECU Reset
 *
 * Request:
 *
 *     11 <resetType>
 *
 * Positive response:
 *
 *     51 <resetType>
 *
 *----------------------------------------------------------*/

static int uds_handle_ecu_reset(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    if (request_length != 2)
    {
        uds_negative_response(
                UDS_SID_ECU_RESET,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    printf("UDS: ECU Reset requested\n");
    printf("UDS: Reset type = 0x%02X\n",
           request[1]);


    /*
     * This POC only acknowledges the request.
     *
     * Actual system reboot can be connected later.
     */

    response[0] =
        UDS_SID_ECU_RESET +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] = request[1];

    *response_length = 2;


    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * Request Download
 *
 * Request format used by this POC:
 *
 *     34
 *     <dataFormatIdentifier>
 *     <addressLengthFormatIdentifier>
 *     <address...>
 *     <size...>
 *
 *
 * Example:
 *
 *     34 00 44 00 00 00 00 00 00 02 00 00
 *
 * means:
 *
 *     Data format        = 00
 *     Address/Length     = 44
 *     Address            = 0x00000000
 *     Firmware size      = 0x00020000
 *
 *
 * Positive response:
 *
 *     74 ...
 *
 *----------------------------------------------------------*/

static int uds_handle_request_download(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    uint8_t address_length_format;

    uint8_t address_bytes;
    uint8_t size_bytes;

    size_t index;

    size_t firmware_size = 0;


    /*
     * RequestDownload is only allowed in
     * Programming Session for this POC.
     */

    if (current_session != UDS_SESSION_PROGRAMMING)
    {
        printf("UDS: RequestDownload rejected\n");
        printf("UDS: Not in Programming Session\n");

        uds_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_CONDITIONS_NOT_CORRECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Minimum:
     *
     * SID
     * DataFormatIdentifier
     * AddressAndLengthFormatIdentifier
     */

    if (request_length < 3)
    {
        printf("UDS: RequestDownload - incorrect length\n");

        uds_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    address_length_format = request[2];


    /*
     * Upper nibble = number of address bytes.
     *
     * Lower nibble = number of size bytes.
     */

    address_bytes =
        (address_length_format >> 4) & 0x0F;

    size_bytes =
        address_length_format & 0x0F;


    /*
     * We need at least:
     *
     * SID
     * DataFormat
     * AddressLengthFormat
     * Address
     * Size
     */

    if (request_length !=
        (size_t)(3 + address_bytes + size_bytes))
    {
        printf("UDS: RequestDownload - invalid format\n");

        uds_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Skip:
     *
     * request[0] = SID
     * request[1] = DataFormatIdentifier
     * request[2] = AddressAndLengthFormatIdentifier
     *
     * Address begins at byte 3.
     */

    index = 3;

    index += address_bytes;


    /*
     * Read firmware size.
     *
     * Big-endian.
     */

    for (uint8_t i = 0; i < size_bytes; i++)
    {
        firmware_size <<= 8;
        firmware_size |= request[index++];
    }


    printf("\n");
    printf("----------------------------------\n");
    printf("UDS: Request Download\n");
    printf("----------------------------------\n");

    printf("Address bytes : %u\n",
           address_bytes);

    printf("Size bytes    : %u\n",
           size_bytes);

    printf("Firmware size : %zu Bytes\n",
           firmware_size);


    /*
     * Check firmware size.
     */

    if (firmware_size == 0 ||
        firmware_size > UDS_MAX_FIRMWARE_SIZE)
    {
        printf("UDS: Firmware size out of range\n");

        uds_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_REQUEST_OUT_OF_RANGE,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Initialize firmware receiver.
     *
     * This allocates the destination firmware buffer.
     */

    if (firmware_receiver_init(firmware_size)
            != FW_RECEIVER_SUCCESS)
    {
        printf("UDS: Firmware receiver initialization failed\n");

        uds_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_GENERAL_REJECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Save download information.
     */

    expected_firmware_size = firmware_size;

    received_firmware_size = 0;

    expected_block_counter = 1;

    download_state = UDS_DOWNLOAD_REQUESTED;


    /*
     * Positive response.
     *
     * 0x34 + 0x40 = 0x74
     *
     * For this POC:
     *
     * response[1..2] = maximum number of
     * bytes accepted in one TransferData
     * request.
     *
     * 0x1000 = 4096 bytes.
     */

    response[0] =
        UDS_SID_REQUEST_DOWNLOAD +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] = 0x20;
    response[2] = 0x10;
    response[3] = 0x00;

    *response_length = 4;


    printf("UDS: Download accepted\n");
    printf("UDS: Response = 74 20 10 00\n");


    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * Transfer Data
 *
 * Request:
 *
 *     36 <blockCounter> <data...>
 *
 * Positive response:
 *
 *     76 <blockCounter>
 *
 *----------------------------------------------------------*/

static int uds_handle_transfer_data(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    uint8_t block_counter;

    const uint8_t *firmware_data;

    size_t data_length;


    /*
     * TransferData must contain:
     *
     * SID
     * Block counter
     * At least one data byte
     */

    if (request_length < 3)
    {
        printf("UDS: TransferData - incorrect length\n");

        uds_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Download must have been requested first.
     */

    if (download_state != UDS_DOWNLOAD_REQUESTED &&
        download_state != UDS_DOWNLOAD_IN_PROGRESS)
    {
        printf("UDS: TransferData received without RequestDownload\n");

        uds_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_SEQUENCE_ERROR,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Byte 1 = block sequence counter.
     */

    block_counter = request[1];


    /*
     * Check expected sequence.
     */

    if (block_counter != expected_block_counter)
    {
        printf("UDS: Unexpected block counter\n");
        printf("UDS: Expected = 0x%02X\n",
               expected_block_counter);

        printf("UDS: Received = 0x%02X\n",
               block_counter);


        uds_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_SEQUENCE_ERROR,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Remaining bytes are firmware.
     */

    firmware_data = &request[2];

    data_length = request_length - 2;


    /*
     * Make sure we do not exceed the
     * expected firmware size.
     */

    if ((received_firmware_size + data_length) >
        expected_firmware_size)
    {
        printf("UDS: Firmware data exceeds expected size\n");

        uds_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_REQUEST_OUT_OF_RANGE,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Store the firmware data.
     */

    if (firmware_receiver_store_data(
            firmware_data,
            data_length)
            != FW_RECEIVER_SUCCESS)
    {
        printf("UDS: Failed to store firmware data\n");

        uds_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_GENERAL_REJECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    received_firmware_size += data_length;

    download_state = UDS_DOWNLOAD_IN_PROGRESS;


    printf("\n");
    printf("UDS: TransferData\n");
    printf("UDS: Block       = 0x%02X\n",
           block_counter);

    printf("UDS: Data        = %zu Bytes\n",
           data_length);

    printf("UDS: Progress    = %zu / %zu Bytes\n",
           received_firmware_size,
           expected_firmware_size);


    /*
     * Increment expected block.
     *
     * Block counter is one byte and wraps
     * from 0xFF -> 0x00.
     */

    expected_block_counter++;


    /*
     * Positive response:
     *
     * 0x36 + 0x40 = 0x76
     */

    response[0] =
        UDS_SID_TRANSFER_DATA +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] = block_counter;

    *response_length = 2;


    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * Request Transfer Exit
 *
 * Request:
 *
 *     37
 *
 * Positive response:
 *
 *     77
 *
 *----------------------------------------------------------*/

static int uds_handle_transfer_exit(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    if (request_length != 1)
    {
        uds_negative_response(
                UDS_SID_REQUEST_TRANSFER_EXIT,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * TransferExit must happen after
     * RequestDownload + TransferData.
     */

    if (download_state != UDS_DOWNLOAD_IN_PROGRESS)
    {
        printf("UDS: TransferExit received in invalid state\n");

        uds_negative_response(
                UDS_SID_REQUEST_TRANSFER_EXIT,
                UDS_RESPONSE_SEQUENCE_ERROR,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*
     * Make sure complete firmware was received.
     */

    if (!firmware_receiver_complete())
    {
        printf("UDS: Firmware reception incomplete\n");

        printf("UDS: Received = %zu\n",
               received_firmware_size);

        printf("UDS: Expected = %zu\n",
               expected_firmware_size);


        uds_negative_response(
                UDS_SID_REQUEST_TRANSFER_EXIT,
                UDS_RESPONSE_CONDITIONS_NOT_CORRECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    printf("\n");
    printf("----------------------------------\n");
    printf("UDS: Firmware Download Complete\n");
    printf("----------------------------------\n");

    printf("Firmware size : %zu Bytes\n",
           expected_firmware_size);


    /*
     * Mark download complete.
     */

    download_state = UDS_DOWNLOAD_COMPLETE;


    /*
     * Positive response:
     *
     * 0x37 + 0x40 = 0x77
     */

    response[0] =
        UDS_SID_REQUEST_TRANSFER_EXIT +
        UDS_POSITIVE_RESPONSE_OFFSET;

    *response_length = 1;


    printf("UDS: Response = 77\n");


    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * Routine Control
 *----------------------------------------------------------*/

static int uds_handle_routine_control(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    if (request_length < 4)
    {
        uds_negative_response(
                UDS_SID_ROUTINE_CONTROL,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    printf("UDS: Routine Control received\n");


    /*
     * This remains a POC placeholder.
     */

    response[0] =
        UDS_SID_ROUTINE_CONTROL +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] = request[1];
    response[2] = request[2];
    response[3] = request[3];

    *response_length = 4;


    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * Process UDS Request
 *----------------------------------------------------------*/

int uds_process_request(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    if (request == NULL ||
        response == NULL ||
        response_length == NULL)
    {
        return UDS_FAILURE;
    }


    if (request_length == 0)
    {
        return UDS_FAILURE;
    }


    printf("\n");
    printf("----------------------------------\n");
    printf("UDS Request\n");
    printf("----------------------------------\n");

    printf("SID = 0x%02X\n",
           request[0]);


    /*
     * Route request according to SID.
     */

    switch (request[0])
    {
        case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:

            return uds_handle_session_control(
                    request,
                    request_length,
                    response,
                    response_length);


        case UDS_SID_ECU_RESET:

            return uds_handle_ecu_reset(
                    request,
                    request_length,
                    response,
                    response_length);


        case UDS_SID_REQUEST_DOWNLOAD:

            return uds_handle_request_download(
                    request,
                    request_length,
                    response,
                    response_length);


        case UDS_SID_TRANSFER_DATA:

            return uds_handle_transfer_data(
                    request,
                    request_length,
                    response,
                    response_length);


        case UDS_SID_REQUEST_TRANSFER_EXIT:

            return uds_handle_transfer_exit(
                    request,
                    request_length,
                    response,
                    response_length);


        case UDS_SID_ROUTINE_CONTROL:

            return uds_handle_routine_control(
                    request,
                    request_length,
                    response,
                    response_length);


        default:

            printf("UDS: Service 0x%02X not supported\n",
                   request[0]);


            uds_negative_response(
                    request[0],
                    UDS_RESPONSE_SERVICE_NOT_SUPPORTED,
                    response,
                    response_length);

            return UDS_SUCCESS;
    }
}


/*-----------------------------------------------------------
 * Get Download State
 *----------------------------------------------------------*/

uds_download_state_t uds_get_download_state(void)
{
    return download_state;
}


/*-----------------------------------------------------------
 * Get Expected Firmware Size
 *----------------------------------------------------------*/

size_t uds_get_expected_firmware_size(void)
{
    return expected_firmware_size;
}


/*-----------------------------------------------------------
 * Get Received Firmware Size
 *----------------------------------------------------------*/

size_t uds_get_received_firmware_size(void)
{
    return received_firmware_size;
}


/*-----------------------------------------------------------
 * Reset Download State
 *----------------------------------------------------------*/

void uds_download_reset(void)
{
    /*
     * Release the firmware receiver buffer.
     */

    firmware_receiver_deinit();


    expected_firmware_size = 0;

    received_firmware_size = 0;

    expected_block_counter = 1;

    download_state = UDS_DOWNLOAD_IDLE;
}
