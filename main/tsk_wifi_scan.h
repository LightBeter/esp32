#ifndef _TSK_WIFI_SCAN_H_
#define _TSK_WIFI_SCAN_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void tsk_init_wifi_scan();
void tsk_deinit_wifi_scan();
void tsk_start_wifi_scan();
void tsk_stop_wifi_scan();
void tsk_trigger_wifi_scan(int client);

#ifdef __cplusplus
}
#endif
#endif