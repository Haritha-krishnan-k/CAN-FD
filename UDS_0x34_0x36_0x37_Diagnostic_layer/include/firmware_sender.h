/******************************************************************************
 * File        : firmware_sender.h
 * Description : Firmware Loader Module
 ******************************************************************************/

#ifndef FIRMWARE_SENDER_H
#define FIRMWARE_SENDER_H

#include <stdint.h>
#include <stddef.h>

/*-----------------------------------------------------------
 * Return Status
 *----------------------------------------------------------*/
#define FW_SUCCESS      (0)
#define FW_FAILURE      (-1)

/*-----------------------------------------------------------
 * Function Prototypes
 *----------------------------------------------------------*/

/* Load firmware from file */
int firmware_load(const char *filename);

/* Get pointer to firmware buffer */
uint8_t *firmware_get_buffer(void);

/* Get firmware size */
size_t firmware_get_size(void);

/* Free allocated firmware buffer */
void firmware_unload(void);

/* firmware crc check */
uint16_t firmware_get_crc(void);

#endif
