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
    ADC_CHAN_REF = 0,
    ADC_CHAN_NTC,
    ADC_CHAN_COUNT
};

typedef struct {
    xTaskHandle         handle;
    ringbuffer_type     buffers[ADC_CHAN_COUNT];
    int16_t             temperature;
    bool                over;
} temp_ctrl_tsk_type;

void temp_ctrl_tsk_init();
void temp_ctrl_tsk_deinit();
void temp_ctrl_tsk_start();
void temp_ctrl_tsk_stop();
bool temp_is_over();
void temp_ctrl_temperature_resp(cJSON *respone);

#ifdef __cplusplus
}
#endif
#endif