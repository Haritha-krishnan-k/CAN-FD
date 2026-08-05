#include <stdio.h>

#include "firmware_sender.h"
#include "packet.h"

int main(void)
{
    packet_t packet;

    /* Load firmware into RAM */
    if (firmware_load("firmware.bin") != FW_SUCCESS)
    {
        return -1;
    }

    printf("Firmware Size : %zu Bytes\n\n",
            firmware_get_size());

    /* Initialize packetizer */
    packet_init(firmware_get_buffer(),
                firmware_get_size());

    /* Generate packets one by one */
    while(packet_has_more())
    {
        if(packet_get_next(&packet) == PACKET_SUCCESS)
        {
            printf("---------------------------------\n");

            printf("Packet Number : %u\n", packet.sequence);

            printf("Packet Length : %u Bytes\n", packet.length);

            printf("First Byte    : %02X\n", packet.data[0]);

            printf("Last Byte     : %02X\n",
                    packet.data[packet.length - 1]);
        }
    }

    firmware_unload();

    return 0;
}
