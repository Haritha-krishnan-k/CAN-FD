#include "uds.h"

#include <stdio.h>

/*-----------------------------------------------------------
 * Diagnostic Session Control Handler
 *
 * Request:
 *     10 01
 *
 * Positive Response:
 *     50 01
 *----------------------------------------------------------*/

static int uds_handle_session_control(
        const uint8_t *request,
        size_t request_length,
        uint8_t *response,
        size_t *response_length)
{
    /* Check request length */

    if (request_length != 2)
    {
        printf("UDS: Incorrect message length\n");

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL;
        response[2] = UDS_RESPONSE_INCORRECT_LENGTH;

        *response_length = 3;

        return UDS_SUCCESS;
    }

    /* Check requested session */

    if (request[1] != UDS_SESSION_DEFAULT)
    {
        printf("UDS: Unsupported session: 0x%02X\n",
               request[1]);

        response[0] = UDS_NEGATIVE_RESPONSE;
        response[1] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL;
        response[2] = UDS_RESPONSE_SERVICE_NOT_SUPPORTED;

        *response_length = 3;

        return UDS_SUCCESS;
    }

    /* Default Session */

    printf("UDS: Default Session requested\n");

    /* Positive response:
     *
     * 0x10 + 0x40 = 0x50
     */

    response[0] =
        UDS_SID_DIAGNOSTIC_SESSION_CONTROL +
        UDS_POSITIVE_RESPONSE_OFFSET;

    response[1] = UDS_SESSION_DEFAULT;

    *response_length = 2;

    printf("UDS: Response: 50 01\n");

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
    /* Validate parameters */

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

    /* Identify UDS service */

    switch (request[0])
    {
        case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:

            return uds_handle_session_control(
                    request,
                    request_length,
                    response,
                    response_length);

        default:

            printf("UDS: Service 0x%02X not supported\n",
                   request[0]);

            response[0] = UDS_NEGATIVE_RESPONSE;
            response[1] = request[0];
            response[2] = UDS_RESPONSE_SERVICE_NOT_SUPPORTED;

            *response_length = 3;

            return UDS_SUCCESS;
    }
}
