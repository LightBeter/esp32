#ifndef _TEMP_CTRL_TSK_H
#define _TEMP_CTRL_TSK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>
#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TEMP_SENSOR_REF = 0,
    TEMP_SENSOR_NTC,
    TEMP_SENSOR_COUNT
};

typedef struct {
    xTaskHandle         handle;
    ringbuffer_type     sensor[TEMP_SENSOR_COUNT];
    int16_t             temperature;
} temp_ctrl_tsk_type;

void temp_ctrl_tsk_init();
void temp_ctrl_tsk_deinit();
void temp_ctrl_tsk_start();
void temp_ctrl_tsk_stop();
void temp_ctrl_temperature_resp(cJSON *respone);

#ifdef __cplusplus
}
#endif
#endif