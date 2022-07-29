#ifndef _TSK_FANCTRL_H
#define _TSK_FANCTRL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_FAN_DUTY(_duty_)     (_duty_ < 10 || _duty_ > 100)
#define INVALID_FAN_PLAY(_play_)     (_play_ > 30)

enum {
    ADC_CHAN_REF = 0,
    ADC_CHAN_NTC,
    ADC_CHAN_COUNT
};

void tsk_fanctrl_init();
void tsk_fanctrl_deinit();
void tsk_fanctrl_start();
void tsk_fanctrl_stop();
bool tempctrl_is_over();
int16_t fanctrl_get_temperature();
uint16_t fanctrl_get_fanduty();
void fanctrl_set_duty(uint16_t value);
void fanctrl_set_play(uint16_t value);

#ifdef __cplusplus
}
#endif
#endif