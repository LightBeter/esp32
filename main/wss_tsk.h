#ifndef _WSS_TSK
#define _WSS_TSK

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "websocket_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool valid;
    int  lifetime;
} wss_client_type;

typedef struct {
    xTaskHandle     serverTsk;
    xTaskHandle     handleTsk;
    QueueHandle_t   clientQueue;
    wss_client_type clients[WEBSOCKET_SERVER_MAX_CLIENTS];
} wss_tsk_type;

void wss_tsk_init();
void wss_tsk_deinit();
void wss_tsk_start();
void wss_tsk_stop();

#ifdef __cplusplus
}
#endif
#endif