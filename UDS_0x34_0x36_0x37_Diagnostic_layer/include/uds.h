#ifndef UDS_H
#define UDS_H

#include <stdint.h>
#include <stddef.h>

#include "firmware_receiver.h"

/*-----------------------------------------------------------
 * UDS Service Identifiers
 *----------------------------------------------------------*/

#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL   0x10
#define UDS_SID_ECU_RESET                    0x11

#define UDS_SID_REQUEST_DOWNLOAD             0x34
#define UDS_SID_TRANSFER_DATA                0x36
#define UDS_SID_REQUEST_TRANSFER_EXIT        0x37

#define UDS_SID_ROUTINE_CONTROL              0x31


/*-----------------------------------------------------------
 * Positive Response Offset
 *
 * Positive response SID = Request SID + 0x40
 *
 * 0x10 -> 0x50
 * 0x34 -> 0x74
 * 0x36 -> 0x76
 * 0x37 -> 0x77
 *----------------------------------------------------------*/

#define UDS_POSITIVE_RESPONSE_OFFSET         0x40


/*-----------------------------------------------------------
 * Negative Response
 *----------------------------------------------------------*/

#define UDS_NEGATIVE_RESPONSE                0x7F


/*-----------------------------------------------------------
 * Negative Response Codes
 *----------------------------------------------------------*/

#define UDS_RESPONSE_GENERAL_REJECT          0x10

#define UDS_RESPONSE_SERVICE_NOT_SUPPORTED   0x11

#define UDS_RESPONSE_INCORRECT_LENGTH        0x13

#define UDS_RESPONSE_CONDITIONS_NOT_CORRECT  0x22

#define UDS_RESPONSE_REQUEST_OUT_OF_RANGE    0x31

#define UDS_RESPONSE_WRONG_BLOCK_SEQUENCE    0x73

#define UDS_RESPONSE_TRANSFER_SUSPENDED      0x71


/*-----------------------------------------------------------
 * Diagnostic Sessions
 *----------------------------------------------------------*/

#define UDS_SESSION_DEFAULT                  0x01

#define UDS_SESSION_PROGRAMMING              0x02


/*-----------------------------------------------------------
 * UDS Return Status
 *----------------------------------------------------------*/

#define UDS_SUCCESS                          0

#define UDS_FAILURE                         -1


/*-----------------------------------------------------------
 * Download State
 *----------------------------------------------------------*/

typedef enum
{
    UDS_DOWNLOAD_IDLE = 0,

    UDS_DOWNLOAD_REQUESTED,

    UDS_DOWNLOAD_IN_PROGRESS,

    UDS_DOWNLOAD_COMPLETE

} uds_download_state_t;


/*-----------------------------------------------------------
 * UDS Download Context
 *----------------------------------------------------------*/

typedef struct
{
    /* Current diagnostic session */

    uint8_t session;


    /* Current download state */

    uds_download_state_t state;


    /* Firmware memory address */

    uint32_t memory_address;


    /* Firmware size requested by tester */

    size_t firmware_size;


    /* Number of firmware bytes received */

    size_t received_bytes;


    /* Expected Transfer Data sequence number */

    uint8_t expected_block_sequence;


    /* Indicates whether RequestDownload was accepted */

    uint8_t download_active;

} uds_download_context_t;


/*-----------------------------------------------------------
 * UDS Initialization
 *----------------------------------------------------------*/

/*
 * Initialize UDS state.
 */
void uds_init(void);


/*-----------------------------------------------------------
 * UDS Request Processing
 *----------------------------------------------------------*/

/*
 * Process one complete UDS request.
 *
 * request
 *      Complete UDS application message received
 *      through ISO-TP.
 *
 * request_length
 *      Number of bytes in request.
 *
 * response
 *      Buffer where UDS response will be written.
 *
 * response_length
 *      Number of bytes generated in response.
 */
int uds_process_request(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length);


/*-----------------------------------------------------------
 * UDS Context
 *----------------------------------------------------------*/

/*
 * Return current UDS download context.
 */
const uds_download_context_t *
uds_get_context(void);


/*-----------------------------------------------------------
 * UDS State Helpers
 *----------------------------------------------------------*/

/*
 * Check whether a firmware download is currently active.
 */
int uds_download_is_active(void);


/*
 * Check whether firmware download has completed.
 */
int uds_download_is_complete(void);

#endif /* UDS_H */
