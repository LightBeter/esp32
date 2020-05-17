#ifndef _TEMP_CTRL_TSK_H
#define _TEMP_CTRL_TSK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TEMP_SENSOR_1 = 0,
    TEMP_SENSOR_2,
    TEMP_SENSOR_COUNT
};

typedef struct {
    xTaskHandle         handle;
    ringbuffer_type     sensor[TEMP_SENSOR_COUNT];
} temp_ctrl_tsk_type;

void temp_ctrl_tsk_init();
void temp_ctrl_tsk_deinit();
void temp_ctrl_tsk_start();
void temp_ctrl_tsk_stop();

#ifdef __cplusplus
}
#endif
#endif