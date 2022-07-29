#ifndef _TSK_WS_H
#define _TSK_WS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool valid;
    int  lifetime;
} ws_client_life_t;

void tsk_ws_init();
void tsk_ws_deinit();
void tsk_ws_start();
void tsk_ws_stop();

#ifdef __cplusplus
}
#endif
#endif