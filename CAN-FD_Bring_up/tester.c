#include <stdio.h>
#include <unistd.h>

#include "can_common.h"

int main()
{
    int socket_fd;
    int count = 0;

    can_packet_t packet;

    packet.id = TESTER_CAN_ID;
    packet.dlc = CANFD_DLC_64;

    for(int i = 0; i < packet.dlc; i++)
    {
        packet.data[i] = i;
    }

    socket_fd = can_socket_init();

    if(socket_fd == CAN_FAILURE)
        return -1;

    printf("Tester Started\n");

    while(1)
    {
        packet.data[0] = count;

        can_send_frame(socket_fd, &packet);

        printf("Frame %d Sent\n", count);

        count++;

        sleep(1);
    }

    can_socket_close(socket_fd);

    return 0;
}
