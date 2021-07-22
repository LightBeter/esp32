#ifndef _UDP_MULTICASE_TSK
#define _UDP_MULTICASE_TSK

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define UDP_MULTICAST_PACKET_MAX_SIZE       384
#define UDP_MULTICAST_PACKET_POOL_SIZE      6

#ifdef __cplusplus
extern "C" {
#endif

typedef struct udp_multicast_packet_tag {
    unsigned char data[384];
    ssize_t len;
} udp_multicast_packet;

typedef struct {
    xTaskHandle     recvTsk;
    xTaskHandle     handleTsk;
    int             sock;
    QueueHandle_t   freeQueue;
    QueueHandle_t   handleQueue;
} udp_multicast_tsk_type;

void udp_multicast_tsk_init();
void udp_multicast_tsk_deinit();
void udp_multicast_tsk_start();
void udp_multicast_tsk_stop();

#ifdef __cplusplus
}
#endif
#endif