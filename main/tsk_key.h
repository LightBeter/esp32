#ifndef _TSK_KEY_H_
#define _TSK_KEY_H_

typedef enum {
    KEY_STATE_DOWN = 0,
    KEY_STATE_UP   = 1,
} key_state_t;


void tsk_key_init();
void tsk_key_deinit();
void tsk_key_start();
void tsk_key_stop();

#endif