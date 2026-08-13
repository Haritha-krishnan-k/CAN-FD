#ifndef FIRMWARE_RECEIVER_H
#define FIRMWARE_RECEIVER_H

#include <stdint.h>
#include <stddef.h>

#define FW_RECEIVER_SUCCESS  0
#define FW_RECEIVER_FAILURE -1

int firmware_receiver_init(size_t size);

int firmware_receiver_store_data(const uint8_t *data,
                                 size_t length);

uint8_t *firmware_receiver_get_buffer(void);

size_t firmware_receiver_get_size(void);

size_t firmware_receiver_get_received_bytes(void);

int firmware_receiver_complete(void);

void firmware_receiver_deinit(void);

#endif
