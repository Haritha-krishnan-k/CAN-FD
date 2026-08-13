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

/*-----------------------------------------------------------
 * Positive Response
 *----------------------------------------------------------*/

#define UDS_POSITIVE_RESPONSE_OFFSET         0x40

/*-----------------------------------------------------------
 * Negative Response
 *----------------------------------------------------------*/

#define UDS_NEGATIVE_RESPONSE               0x7F

#define UDS_RESPONSE_GENERAL_REJECT          0x10
#define UDS_RESPONSE_SERVICE_NOT_SUPPORTED   0x11
#define UDS_RESPONSE_INCORRECT_LENGTH        0x13
#define UDS_RESPONSE_CONDITIONS_NOT_CORRECT  0x22

/*-----------------------------------------------------------
 * UDS Return Status
 *----------------------------------------------------------*/

#define UDS_SUCCESS                          0
#define UDS_FAILURE                         -1

/*-----------------------------------------------------------
 * UDS API
 *----------------------------------------------------------*/

int uds_process_request(const uint8_t *request,
                        size_t request_length,
                        uint8_t *response,
                        size_t *response_length);

#endif /* UDS_H */
