#include <stdio.h>

#include "can_common.h"

int main()
{
    int socket_fd;

    can_packet_t packet;
    int frame_count = 0;
    socket_fd = can_socket_init();

    if(socket_fd == CAN_FAILURE)
        return -1;

    printf("Waiting for CAN Frame...\n");

    while(1)
    {
        if(can_receive_frame(socket_fd, &packet) == CAN_SUCCESS)
        {
	    frame_count ++;

	    printf("\n frame number : %d\n" , frame_count);

	    printf("\nReceived Frame\n");

            printf("CAN ID : 0x%X\n", packet.id);

            printf("DLC    : %d\n", packet.dlc);

            printf("Data   : ");

            for(int i=0;i<packet.dlc;i++)
                printf("%02X ", packet.data[i]);

	   printf("\n");

	    can_packet_t response;

	    response.id = TARGET_CAN_ID;
            response.dlc = CAN_DLC_4;

	    response.data[0] = 0xAA;
            response.data[1] = 0xBB;
            response.data[2] = 0xCC;
	    response.data[3] = 0xDD;

	    can_send_frame(socket_fd , &response);

            printf("ACK sent");
        }
    }

    can_socket_close(socket_fd);

    return 0;
}
