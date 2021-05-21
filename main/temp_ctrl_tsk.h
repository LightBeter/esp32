#ifndef _TEMP_CTRL_TSK_H
#define _TEMP_CTRL_TSK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>
#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAN_PWM_CTRL                1
#define INVALID_FAN_DUTY(_duty_)     (_duty_ < 10 || _duty_ > 100)

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
#if defined(FAN_PWM_CTRL) && (FAN_PWM_CTRL == 1)
    bool                highTemperature;
    uint16_t            fanDuty;
    uint16_t            curFanDuty;
    uint16_t            playTime;
    uint32_t            curSaveToken;
    uint32_t            preSaveToken;
#endif
} temp_ctrl_tsk_type;

void temp_ctrl_tsk_init();
void temp_ctrl_tsk_deinit();
void temp_ctrl_tsk_start();
void temp_ctrl_tsk_stop();
bool temp_is_over();
void temp_ctrl_temperature_resp(cJSON *respone);

#if defined(FAN_PWM_CTRL) && (FAN_PWM_CTRL == 1)
uint16_t temp_ctrl_fan_duty();
void temp_ctrl_fan_duty_set(uint16_t value);
void temp_ctrl_fan_play(uint16_t playtime);
void temp_ctrl_fan_update();
#endif
#ifdef __cplusplus
}
#endif
#endif