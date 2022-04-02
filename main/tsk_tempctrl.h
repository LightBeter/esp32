#ifndef _TSK_TEMPCTRL_H
#define _TSK_TEMPCTRL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_FAN_DUTY(_duty_)     (_duty_ < 10 || _duty_ > 100)
#define INVALID_FAN_PLAY(_play_)     (_play_ < 0 || _play_ > 30)

enum {
    ADC_CHAN_REF = 0,
    ADC_CHAN_NTC,
    ADC_CHAN_COUNT
};

void tsk_tempctrl_init();
void tsk_tempctrl_deinit();
void tsk_tempctrl_start();
void tsk_tempctrl_stop();
bool tempctrl_is_over();
int16_t tempctrl_get_temperature();
uint16_t tempctrl_get_fanduty();
void tempctrl_set_fanduty(uint16_t value);
void tempctrl_set_fanplay(uint16_t value);

#ifdef __cplusplus
}
#endif
#endif