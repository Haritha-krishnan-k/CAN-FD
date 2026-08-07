/******************************************************************************
 * File        : firmware_sender.c
 * Description : Firmware Loader Module
 ******************************************************************************/

#include "firmware_sender.h"

#include <stdio.h>
#include <stdlib.h>

/*-----------------------------------------------------------
 * Private Variables
 *----------------------------------------------------------*/

static uint8_t *firmware_buffer = NULL;

static size_t firmware_size = 0;

/*-----------------------------------------------------------
 * Load Firmware
 *----------------------------------------------------------*/

int firmware_load(const char *filename)
{
    FILE *fp;

    /* Open firmware file */

    fp = fopen(filename, "rb");

    if(fp == NULL)
    {
        perror("fopen");
        return FW_FAILURE;
    }

    /* Move to end of file */

    if(fseek(fp, 0, SEEK_END) != 0)
    {
        perror("fseek");

        fclose(fp);

        return FW_FAILURE;
    }

    /* Find file size */

    firmware_size = (size_t)ftell(fp);

    if(firmware_size == 0)
    {
        printf("Firmware file is empty\n");

        fclose(fp);

        return FW_FAILURE;
    }

    /* Return file pointer to beginning */

    rewind(fp);

    /* Allocate RAM */

    firmware_buffer = (uint8_t *)malloc(firmware_size);

    if(firmware_buffer == NULL)
    {
        perror("malloc");

        fclose(fp);

        return FW_FAILURE;
    }

    /* Read firmware */

    if(fread(firmware_buffer,
             1,
             firmware_size,
             fp) != firmware_size)
    {
        perror("fread");

        free(firmware_buffer);

        firmware_buffer = NULL;

        firmware_size = 0;

        fclose(fp);

        return FW_FAILURE;
    }

    fclose(fp);

    printf("----------------------------------\n");
    printf("Firmware Loaded Successfully\n");
    printf("Firmware Size : %zu Bytes\n", firmware_size);
    printf("----------------------------------\n");

    return FW_SUCCESS;
}

/*-----------------------------------------------------------
 * Return Firmware Buffer
 *----------------------------------------------------------*/

uint8_t *firmware_get_buffer(void)
{
    return firmware_buffer;
}

/*-----------------------------------------------------------
 * Return Firmware Size
 *----------------------------------------------------------*/

size_t firmware_get_size(void)
{
    return firmware_size;
}

/*-----------------------------------------------------------
 * Free Firmware Buffer
 *----------------------------------------------------------*/

void firmware_unload(void)
{
    if(firmware_buffer != NULL)
    {
        free(firmware_buffer);

        firmware_buffer = NULL;
    }

    firmware_size = 0;
}
