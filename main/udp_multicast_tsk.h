#ifndef _UDP_MULTICASE_TSK
#define _UDP_MULTICASE_TSK

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#define UDP_MULTICAST_PACKET_MAX_SIZE       384
#define UDP_MULTICAST_PACKET_POOL_SIZE      10

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char data[UDP_MULTICAST_PACKET_MAX_SIZE];
    int  len;
} udp_multicast_packet;

typedef struct {
    xTaskHandle     recvTsk;
    xTaskHandle     handleTsk;
    int             sock;
    QueueHandle_t   freeQueue;
    QueueHandle_t   handleQueue;
    udp_multicast_packet pool[UDP_MULTICAST_PACKET_POOL_SIZE];
} udp_multicast_tsk_type;

void udp_multicast_tsk_init();
void udp_multicast_tsk_deinit();
void udp_multicast_tsk_start();
void udp_multicast_tsk_stop();

#ifdef __cplusplus
}
#endif
#endif