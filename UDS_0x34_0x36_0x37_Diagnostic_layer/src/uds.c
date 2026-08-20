#include "uds.h"

#include <stdio.h>
#include <string.h>
#include "crc.h"

/*-----------------------------------------------------------
 * Private UDS Context
 *----------------------------------------------------------*/

static uds_download_context_t uds_context;


/*-----------------------------------------------------------
 * Helper: Negative Response
 *----------------------------------------------------------*/

static void uds_make_negative_response(
        uint8_t service,
        uint8_t error_code,
        uint8_t *response,
        size_t *response_length)
{
    response[0] = UDS_NEGATIVE_RESPONSE;
    response[1] = service;
    response[2] = error_code;

    *response_length = 3;
}


/*-----------------------------------------------------------
 * Helper: Positive Response
 *----------------------------------------------------------*/

static void uds_make_positive_response(
        uint8_t service,
        uint8_t *response,
        size_t *response_length)
{
    response[0] =
        service + UDS_POSITIVE_RESPONSE_OFFSET;

    *response_length = 1;
}


/*-----------------------------------------------------------
 * Initialize UDS
 *----------------------------------------------------------*/

void uds_init(void)
{
    /*
     * Make sure any previous firmware receiver
     * allocation is released.
     */
    firmware_receiver_deinit();

    memset(&uds_context,
           0,
           sizeof(uds_context));

    uds_context.session =
        UDS_SESSION_DEFAULT;

    uds_context.state =
        UDS_DOWNLOAD_IDLE;

    uds_context.expected_block_sequence =
        1;

    uds_context.download_active =
        0;

    printf("UDS initialized\n");
}


/*-----------------------------------------------------------
 * Diagnostic Session Control - 0x10
 *----------------------------------------------------------*/

static int uds_handle_session_control(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    uint8_t requested_session;


    /*-------------------------------------------------------
     * Validate request length
     *------------------------------------------------------*/

    if (request_length != 2)
    {
        printf("UDS 0x10: Incorrect message length\n");

        uds_make_negative_response(
                UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    requested_session = request[1];


    /*-------------------------------------------------------
     * Check requested session
     *------------------------------------------------------*/

    if (requested_session != UDS_SESSION_DEFAULT &&
        requested_session != UDS_SESSION_PROGRAMMING)
    {
        printf("UDS 0x10: Unsupported session 0x%02X\n",
               requested_session);

        uds_make_negative_response(
                UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                UDS_RESPONSE_SERVICE_NOT_SUPPORTED,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Change session
     *------------------------------------------------------*/

    uds_context.session =
        requested_session;

    printf("UDS 0x10: Session changed to 0x%02X\n",
           requested_session);


    /*-------------------------------------------------------
     * Positive Response
     *------------------------------------------------------*/

    response[0] =
        UDS_SID_DIAGNOSTIC_SESSION_CONTROL +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] =
        requested_session;

    *response_length = 2;

    printf("UDS 0x10 Response: %02X %02X\n",
           response[0],
           response[1]);

    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * Request Download - 0x34
 *
 * Request:
 *
 *     34 00 44
 *        + address
 *        + size
 *
 * Example:
 *
 *     34 00 44
 *        00 00 00 00
 *        00 02 00 00
 *
 * Meaning:
 *
 *     Address = 0x00000000
 *     Size    = 131072 bytes
 *
 * Positive Response:
 *
 *     74 20 00
 *----------------------------------------------------------*/

static int uds_handle_request_download(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    uint8_t data_format_identifier;

    uint8_t address_length_format_identifier;

    uint8_t address_length;

    uint8_t size_length;

    uint32_t address = 0;

    size_t firmware_size = 0;

    size_t index;

    size_t expected_request_length;


    /*-------------------------------------------------------
     * RequestDownload requires at least:
     *
     * Byte 0 = SID
     * Byte 1 = DFI
     * Byte 2 = ALFI
     *------------------------------------------------------*/

    if (request_length < 3)
    {
        printf("UDS 0x34: Incorrect message length\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Download requires Programming Session
     *------------------------------------------------------*/

    if (uds_context.session !=
        UDS_SESSION_PROGRAMMING)
    {
        printf("UDS 0x34: Programming session required\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_CONDITIONS_NOT_CORRECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    data_format_identifier =
        request[1];

    address_length_format_identifier =
        request[2];


    /*
     * This POC does not currently use the Data Format
     * Identifier, but we still decode it.
     */
    (void)data_format_identifier;


    /*-------------------------------------------------------
     * Decode ALFI
     *
     * Upper nibble = address length
     * Lower nibble = memory size length
     *
     * Example:
     *
     *     0x44
     *
     *     Address = 4 bytes
     *     Size    = 4 bytes
     *------------------------------------------------------*/

    address_length =
        (address_length_format_identifier >> 4) & 0x0F;

    size_length =
        address_length_format_identifier & 0x0F;


    if (address_length == 0 ||
        size_length == 0)
    {
        printf("UDS 0x34: Invalid ALFI\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_REQUEST_OUT_OF_RANGE,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * This POC supports:
     *
     * Address <= 4 bytes
     * Size <= sizeof(size_t)
     *------------------------------------------------------*/

    if (address_length > 4 ||
        size_length > sizeof(size_t))
    {
        printf("UDS 0x34: Unsupported address/size length\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_REQUEST_OUT_OF_RANGE,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Calculate expected complete request length
     *------------------------------------------------------*/

    expected_request_length =
        3 + address_length + size_length;


    if (request_length != expected_request_length)
    {
        printf("UDS 0x34: Incorrect request length\n");

        printf("Expected : %zu Bytes\n",
               expected_request_length);

        printf("Received : %zu Bytes\n",
               request_length);

        uds_make_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Decode Memory Address
     *
     * Big Endian
     *------------------------------------------------------*/

    index = 3;

    for (uint8_t i = 0;
         i < address_length;
         i++)
    {
        address <<= 8;

        address |= request[index++];
    }


    /*-------------------------------------------------------
     * Decode Firmware Size
     *
     * Big Endian
     *------------------------------------------------------*/

    firmware_size = 0;

    for (uint8_t i = 0;
         i < size_length;
         i++)
    {
        firmware_size <<= 8;

        firmware_size |= request[index++];
    }


    /*-------------------------------------------------------
     * Validate Firmware Size
     *------------------------------------------------------*/

    if (firmware_size == 0)
    {
        printf("UDS 0x34: Firmware size is zero\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_REQUEST_OUT_OF_RANGE,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    printf("\n");
    printf("=================================\n");
    printf("UDS Request Download\n");
    printf("=================================\n");

    printf("Memory Address : 0x%08X\n",
           address);

    printf("Firmware Size  : %zu Bytes\n",
           firmware_size);


    /*-------------------------------------------------------
     * IMPORTANT FIX
     *
     * Allocate the firmware receiver buffer now.
     *
     * Previously uds.c only stored:
     *
     *     uds_context.firmware_size = firmware_size;
     *
     * but firmware_receiver_init() was never called.
     *
     * Therefore receive_buffer remained NULL and
     * firmware_receiver_store_data() failed during 0x36.
     *------------------------------------------------------*/

    /*
     * If another download was active, release the old
     * receiver buffer before starting a new one.
     */
    firmware_receiver_deinit();


    if (firmware_receiver_init(firmware_size)
            != FW_RECEIVER_SUCCESS)
    {
        printf("UDS 0x34: Failed to initialize "
               "firmware receiver\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_DOWNLOAD,
                UDS_RESPONSE_GENERAL_REJECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Initialize Download Context
     *------------------------------------------------------*/

    uds_context.memory_address =
        address;

    uds_context.firmware_size =
        firmware_size;

    uds_context.received_bytes =
        0;

    uds_context.expected_block_sequence =
        1;

    uds_context.state =
        UDS_DOWNLOAD_REQUESTED;

    uds_context.download_active =
        1;


    /*-------------------------------------------------------
     * Positive Response
     *
     * 0x34 + 0x40 = 0x74
     *
     * POC response:
     *
     *     74 20 00
     *------------------------------------------------------*/

    response[0] =
        UDS_SID_REQUEST_DOWNLOAD +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] =
        0x20;

    response[2] =
        0x00;

    *response_length = 3;


    printf("Firmware receiver initialized successfully\n");

    printf("UDS 0x34 Response: ");

    for (size_t i = 0;
         i < *response_length;
         i++)
    {
        printf("%02X ",
               response[i]);
    }

    printf("\n");

    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * Transfer Data - 0x36
 *
 * Request:
 *
 *     36 <block sequence> <data...>
 *
 * Example:
 *
 *     36 01 AA BB CC DD ...
 *
 * Positive Response:
 *
 *     76 <block sequence>
 *----------------------------------------------------------*/

static int uds_handle_transfer_data(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    uint8_t block_sequence;

    const uint8_t *firmware_data;

    size_t firmware_data_length;


    /*-------------------------------------------------------
     * Minimum:
     *
     * Byte 0 = 0x36
     * Byte 1 = block sequence
     * Byte 2 = at least one data byte
     *------------------------------------------------------*/

    if (request_length < 3)
    {
        printf("UDS 0x36: Incorrect message length\n");

        uds_make_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Check download state
     *------------------------------------------------------*/

    if (!uds_context.download_active)
    {
        printf("UDS 0x36: Download not active\n");

        uds_make_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_CONDITIONS_NOT_CORRECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Check firmware receiver
     *------------------------------------------------------*/

    if (firmware_receiver_get_buffer() == NULL)
    {
        printf("UDS 0x36: Firmware receiver not initialized\n");

        uds_make_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_GENERAL_REJECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    block_sequence =
        request[1];


    /*-------------------------------------------------------
     * Check block sequence
     *------------------------------------------------------*/

    if (block_sequence !=
        uds_context.expected_block_sequence)
    {
        printf("UDS 0x36: Wrong block sequence\n");

        printf("Expected : %u\n",
               uds_context.expected_block_sequence);

        printf("Received : %u\n",
               block_sequence);


        uds_make_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_WRONG_BLOCK_SEQUENCE,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Firmware data begins after:
     *
     * Byte 0 = SID
     * Byte 1 = Block Sequence
     *------------------------------------------------------*/

    firmware_data =
        &request[2];

    firmware_data_length =
        request_length - 2;


    /*-------------------------------------------------------
     * Check firmware size
     *------------------------------------------------------*/

    if ((uds_context.received_bytes +
         firmware_data_length) >
        uds_context.firmware_size)
    {
        printf("UDS 0x36: Firmware size exceeded\n");

        printf("Current : %zu Bytes\n",
               uds_context.received_bytes);

        printf("Incoming: %zu Bytes\n",
               firmware_data_length);

        printf("Maximum : %zu Bytes\n",
               uds_context.firmware_size);


        uds_make_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_REQUEST_OUT_OF_RANGE,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Store firmware data
     *------------------------------------------------------*/

    if (firmware_receiver_store_data(
            firmware_data,
            firmware_data_length)
            != FW_RECEIVER_SUCCESS)
    {
        printf("UDS 0x36: Failed to store firmware data\n");

        uds_make_negative_response(
                UDS_SID_TRANSFER_DATA,
                UDS_RESPONSE_GENERAL_REJECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Update UDS context
     *------------------------------------------------------*/

    uds_context.received_bytes +=
        firmware_data_length;

    uds_context.state =
        UDS_DOWNLOAD_IN_PROGRESS;


    /*-------------------------------------------------------
     * Increment expected block sequence
     *
     * uint8_t naturally wraps:
     *
     * FF -> 00
     *------------------------------------------------------*/

    uds_context.expected_block_sequence++;


    /*-------------------------------------------------------
     * Print transfer information
     *------------------------------------------------------*/

    printf("\n");
    printf("UDS 0x36: Block %u received\n",
           block_sequence);

    printf("Data Bytes     : %zu\n",
           firmware_data_length);

    printf("Total Received : %zu / %zu\n",
           uds_context.received_bytes,
           uds_context.firmware_size);


    /*-------------------------------------------------------
     * Check whether entire firmware has been received
     *------------------------------------------------------*/

    if (uds_context.received_bytes ==
        uds_context.firmware_size)
    {
        printf("Firmware data reception complete\n");
    }


    /*-------------------------------------------------------
     * Positive Response
     *
     * 0x36 + 0x40 = 0x76
     *
     * Response:
     *
     *     76 <block sequence>
     *------------------------------------------------------*/

    response[0] =
        UDS_SID_TRANSFER_DATA +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] =
        block_sequence;

    *response_length = 2;


    printf("UDS 0x36 Response: %02X %02X\n",
           response[0],
           response[1]);


    return UDS_SUCCESS;
}


/*-----------------------------------------------------------
 * Request Transfer Exit - 0x37
 *
 * Request:
 *
 *     37
 *
 * Positive Response:
 *
 *     77
 *----------------------------------------------------------*/

static int uds_handle_transfer_exit(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    /*-------------------------------------------------------
     * Validate request
     *------------------------------------------------------*/

    if (request_length != 3)
    {
        printf("UDS 0x37: Incorrect message length\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_TRANSFER_EXIT,
                UDS_RESPONSE_INCORRECT_LENGTH,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Check download state
     *------------------------------------------------------*/

    if (!uds_context.download_active)
    {
        printf("UDS 0x37: No active download\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_TRANSFER_EXIT,
                UDS_RESPONSE_CONDITIONS_NOT_CORRECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Verify complete firmware was received
     *------------------------------------------------------*/

    if (uds_context.received_bytes !=
        uds_context.firmware_size)
    {
        printf("UDS 0x37: Firmware transfer incomplete\n");

        printf("Expected : %zu Bytes\n",
               uds_context.firmware_size);

        printf("Received : %zu Bytes\n",
               uds_context.received_bytes);


        uds_make_negative_response(
                UDS_SID_REQUEST_TRANSFER_EXIT,
                UDS_RESPONSE_CONDITIONS_NOT_CORRECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * Also verify firmware receiver
     *------------------------------------------------------*/

    if (!firmware_receiver_complete())
    {
        printf("UDS 0x37: Firmware receiver reports "
               "incomplete data\n");

        uds_make_negative_response(
                UDS_SID_REQUEST_TRANSFER_EXIT,
                UDS_RESPONSE_CONDITIONS_NOT_CORRECT,
                response,
                response_length);

        return UDS_SUCCESS;
    }


    /*-------------------------------------------------------
     * CRC Verification
     *------------------------------------------------------*/

     uint8_t *firmware_buffer;

     size_t firmware_length;

     uint16_t calculated_crc;


     /* Get received firmware */

     firmware_buffer =
         firmware_receiver_get_buffer();

     firmware_length =
         firmware_receiver_get_received_bytes();


     /* Calculate CRC */

     calculated_crc =
             crc16_calculate(
                     firmware_buffer,
                            firmware_length);


     /* Print CRC result */

     printf("\n");
     printf("=================================\n");
     printf("Firmware CRC Verification\n");
     printf("=================================\n");

     printf("Firmware Size   : %zu Bytes\n",
       firmware_length);

     printf("Calculated CRC  : 0x%04X\n",
       calculated_crc);

     printf("CRC CHECK       : CALCULATED\n");


    /*-------------------------------------------------------
     * Transfer Complete
     *------------------------------------------------------*/

    uds_context.state =
        UDS_DOWNLOAD_COMPLETE;

    uds_context.download_active =
        0;


    printf("\n");
    printf("=================================\n");
    printf("UDS Transfer Exit\n");
    printf("=================================\n");

    printf("Firmware Transfer Complete\n");

    printf("Total Firmware Bytes : %zu\n",
           uds_context.received_bytes);


    /*-------------------------------------------------------
     * Positive Response
     *
     * 0x37 + 0x40 = 0x77
     *------------------------------------------------------*/

    uds_make_positive_response(
            UDS_SID_REQUEST_TRANSFER_EXIT,
            response,
            response_length);


    printf("UDS 0x37 Response: 77\n");


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
    uint8_t service_id;


    /*-------------------------------------------------------
     * Validate parameters
     *------------------------------------------------------*/

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


    service_id =
        request[0];


    /*-------------------------------------------------------
     * Identify UDS Service
     *------------------------------------------------------*/

    switch (service_id)
    {
        case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:

            return uds_handle_session_control(
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


        default:

            printf("UDS: Service 0x%02X not supported\n",
                   service_id);


            uds_make_negative_response(
                    service_id,
                    UDS_RESPONSE_SERVICE_NOT_SUPPORTED,
                    response,
                    response_length);

            return UDS_SUCCESS;
    }
}


/*-----------------------------------------------------------
 * Get UDS Context
 *----------------------------------------------------------*/

const uds_download_context_t *
uds_get_context(void)
{
    return &uds_context;
}


/*-----------------------------------------------------------
 * Check Download Active
 *----------------------------------------------------------*/

int uds_download_is_active(void)
{
    return uds_context.download_active != 0;
}


/*-----------------------------------------------------------
 * Check Download Complete
 *----------------------------------------------------------*/

int uds_download_is_complete(void)
{
    return uds_context.state ==
           UDS_DOWNLOAD_COMPLETE;
}
