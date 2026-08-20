#ifndef UDS_H
#define UDS_H

#include <stdint.h>
#include <stddef.h>

/*-----------------------------------------------------------
 * UDS Service Identifiers
 *----------------------------------------------------------*/

#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL   0x10
#define UDS_SID_ECU_RESET                    0x11
#define UDS_SID_ROUTINE_CONTROL              0x31
#define UDS_SID_REQUEST_DOWNLOAD             0x34
#define UDS_SID_TRANSFER_DATA                0x36
#define UDS_SID_REQUEST_TRANSFER_EXIT        0x37


/*-----------------------------------------------------------
 * Diagnostic Sessions
 *----------------------------------------------------------*/

#define UDS_SESSION_DEFAULT                  0x01
#define UDS_SESSION_PROGRAMMING              0x02


/*-----------------------------------------------------------
 * Positive Response
 *----------------------------------------------------------*/

#define UDS_POSITIVE_RESPONSE_OFFSET         0x40


/*-----------------------------------------------------------
 * Negative Response
 *----------------------------------------------------------*/

#define UDS_NEGATIVE_RESPONSE                0x7F

#define UDS_RESPONSE_GENERAL_REJECT          0x10
#define UDS_RESPONSE_SERVICE_NOT_SUPPORTED   0x11
#define UDS_RESPONSE_INCORRECT_LENGTH        0x13
#define UDS_RESPONSE_CONDITIONS_NOT_CORRECT  0x22
#define UDS_RESPONSE_REQUEST_SEQUENCE_ERROR  0x24
#define UDS_RESPONSE_REQUEST_OUT_OF_RANGE    0x31


/*-----------------------------------------------------------
 * UDS Return Status
 *----------------------------------------------------------*/

#define UDS_SUCCESS                           0
#define UDS_FAILURE                          -1


/*-----------------------------------------------------------
 * UDS Download Parameters
 *----------------------------------------------------------*/

/*
 * Maximum firmware buffer size supported by this POC.
 *
 * This is a host-side POC, so we keep the limit
 * reasonably small.
 */

#define UDS_MAX_FIRMWARE_SIZE                (1024 * 1024)


/*
 * Maximum firmware data carried by one
 * TransferData request after ISO-TP reassembly.
 *
 * This is an application-level value.
 *
 * ISO-TP itself handles the CAN-FD segmentation.
 */

#define UDS_MAX_TRANSFER_DATA                4096


/*-----------------------------------------------------------
 * UDS Download State
 *----------------------------------------------------------*/

typedef enum
{
    UDS_DOWNLOAD_IDLE = 0,

    UDS_DOWNLOAD_REQUESTED,

    UDS_DOWNLOAD_IN_PROGRESS,

    UDS_DOWNLOAD_COMPLETE

} uds_download_state_t;


/*-----------------------------------------------------------
 * UDS API
 *----------------------------------------------------------*/

/*
 * Process one complete UDS request.
 *
 * The request has already been reassembled by ISO-TP.
 *
 * Example:
 *
 *     request = { 0x10, 0x02 }
 *
 * or:
 *
 *     request = { 0x36, block_counter, firmware_data... }
 */
int uds_process_request(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length);


/*-----------------------------------------------------------
 * Download State API
 *----------------------------------------------------------*/

/*
 * Return current download state.
 */
uds_download_state_t uds_get_download_state(void);


/*
 * Return expected firmware size.
 */
size_t uds_get_expected_firmware_size(void);


/*
 * Return number of firmware bytes received.
 */
size_t uds_get_received_firmware_size(void);


/*
 * Reset the UDS download state.
 */
void uds_download_reset(void);

#endif /* UDS_H */
