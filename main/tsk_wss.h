#ifndef _TSK_WSS_H
#define _TSK_WSS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool valid;
    int  lifetime;
} wss_client_t;

void tsk_wss_init();
void tsk_wss_deinit();
void tsk_wss_start();
void tsk_wss_stop();

#ifdef __cplusplus
}
#endif
#endif