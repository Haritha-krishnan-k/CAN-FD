#include "can_common.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>


#include <linux/can.h>
#include <linux/can/raw.h>

static struct sockaddr_can addr;
static struct ifreq ifr;

int can_socket_init(void)
{
    int s;
    int enable_canfd = 1;
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    if(s < 0)
    {
        perror("Socket");
        return CAN_FAILURE;
    }

         /* Enable CAN-FD support */
    if(setsockopt(s, SOL_CAN_RAW,CAN_RAW_FD_FRAMES,&enable_canfd,sizeof(enable_canfd)) < 0)
    {
        perror("setsockopt CAN_RAW_FD_FRAMES");
        close(s);
        return CAN_FAILURE;
    }

    strcpy(ifr.ifr_name, CAN_INTERFACE);

    if(ioctl(s, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("ioctl");
        close(s);
        return CAN_FAILURE;
    }

   // ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind");
        close(s);
        return CAN_FAILURE;
    }

    return s;
}

int can_send_frame(int socket_fd, const can_packet_t *packet)
{
    struct canfd_frame frame; 
    int ret;

   memset(&frame , 0 , sizeof(frame));

    frame.can_id = packet->id;
  //frame.can_dlc = packet->dlc;
    frame.len = packet->dlc;

    memcpy(frame.data, packet->data, packet->dlc);

    ret =  write(socket_fd, &frame, sizeof(frame));
    if(ret<0)
    {
        perror("CAN send");
	return CAN_FAILURE;
    }
    
     return CAN_SUCCESS;
}

int can_receive_frame(int socket_fd, can_packet_t *packet)
{
    struct canfd_frame frame;

    int nbytes;

    memset(&frame , 0 , sizeof(frame));

    nbytes = read(socket_fd, &frame, sizeof(frame));

    if(nbytes <= 0)
        return CAN_FAILURE;

    packet->id = frame.can_id;
    packet->dlc = frame.len;

    memcpy(packet->data, frame.data, frame.len);

    return CAN_SUCCESS;
}

/*int can_send_packet(int socket_fd,
                    const packet_t *packet)
{
    can_packet_t frame;

    packet_encode(packet, &frame);

    return can_send_frame(socket_fd, &frame);
}*/

/*int can_receive_packet(int socket_fd,
                       packet_t *packet)
{
    can_packet_t frame;

    if(can_receive_frame(socket_fd, &frame)
            != CAN_SUCCESS)
    {
        return CAN_FAILURE;
    }

    packet_decode(&frame, packet);

    return CAN_SUCCESS;
}*/


void can_socket_close(int socket_fd)
{
    close(socket_fd);
}


/*void packet_encode(const packet_t *packet,
                   can_packet_t *frame)
{
    frame->id = TESTER_CAN_ID;

    frame->dlc = packet->length + 2;

    frame->data[0] = packet->sequence & 0xFF;
    frame->data[1] = (packet->sequence >> 8) & 0xFF;

    memcpy(&frame->data[2],
           packet->data,
           packet->length);
}*/



/*void packet_decode(const can_packet_t *frame,
                   packet_t *packet)
{
    packet->sequence =
        frame->data[0] |
        (frame->data[1] << 8);

    packet->length = frame->dlc - 2;

    memcpy(packet->data,
           &frame->data[2],
           packet->length);
}*/
