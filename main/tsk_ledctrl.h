#ifndef _TSK_LEDCTRL_H
#define _TSK_LEDCTRL_H

#ifdef __cplusplus
extern "C" {
#endif

void tsk_ledctrl_init();
void tsk_ledctrl_deinit();
void tsk_ledctrl_start();
void tsk_ledctrl_stop();
void tsk_ledctrl_lock();
void tsk_ledctrl_unlock();

#ifdef __cplusplus
}
#endif
#endif