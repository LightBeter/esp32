#ifndef _TSK_WIFI_SCAN_H_
#define _TSK_WIFI_SCAN_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void tsk_wifi_scan_init();
void tsk_deinit_wifi_scan();
void tsk_wifi_scan_start();
void tsk_wifi_scan_stop();
void tsk_trigger_wifi_scan(int client);

#ifdef __cplusplus
}
#endif
#endif