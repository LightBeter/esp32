#ifndef _TSK_UDP_MULTICASE_H
#define _TSK_UDP_MULTICASE_H

#define UDP_MULTICAST_PACKET_MAX_SIZE       384
#define UDP_MULTICAST_PACKET_POOL_SIZE      6

#ifdef __cplusplus
extern "C" {
#endif

typedef struct udp_multicast_packet_tag {
    unsigned char data[UDP_MULTICAST_PACKET_MAX_SIZE];
    ssize_t len;
} udp_multicast_packet_t;

void tsk_udp_multicast_init();
void tsk_udp_multicast_deinit();
void tsk_udp_multicast_start();
void tsk_udp_multicast_stop();

#ifdef __cplusplus
}
#endif
#endif