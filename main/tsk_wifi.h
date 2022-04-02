#ifndef _TSK_WIFI_H
#define _TSK_WIFI_H

#include <esp_netif.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    wifi_tsk_state_idle = 0x00,
    wifi_tsk_state_sta,
    wifi_tsk_state_ap,
} wifi_tsk_state;

void tsk_wifi_init();
void tsk_wifi_deinit();
void tsk_wifi_start();
void tsk_wifi_stop();
void tsk_wifi_set(int mode, const char *ssid, const char *pwd);
esp_netif_t* wifi_netif();

#ifdef __cplusplus
}
#endif
#endif