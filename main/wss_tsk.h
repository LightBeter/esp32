#ifndef _WSS_TSK
#define _WSS_TSK

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct {
    xTaskHandle     serverTsk;
    xTaskHandle     handleTsk;
    QueueHandle_t   clientQueue;
} wss_tsk_type;

void wss_tsk_init();
void wss_tsk_deinit();
void wss_tsk_start();
void wss_tsk_stop();

#endif