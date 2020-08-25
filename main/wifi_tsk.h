#ifndef _WIFI_TSK_H
#define _WIFI_TSK_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_wifi.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_AP_SCAN_MAX            32

typedef enum {
    wifi_tsk_state_idle = 0x00,
    wifi_tsk_state_sta,
    wifi_tsk_state_ap,
} wifi_tsk_state;

typedef struct {
    xTaskHandle         handle;
    bool                isConnect;
    wifi_tsk_state      state;
    wifi_init_config_t  initConf;
    wifi_config_t       conf;
    EventGroupHandle_t  eventGroup;  
    SemaphoreHandle_t   mutex;
} wifi_tsk_type;

typedef struct {
    wifi_ap_record_t    list[WIFI_AP_SCAN_MAX];
    uint16_t            num;
    bool                running;
} wifi_tsk_ap_info;

void wifi_tsk_init();
void wifi_tsk_deinit();
void wifi_tsk_start();
void wifi_tsk_stop();
wifi_tsk_state wifi_state();
void wifi_tsk_get_ap_list(cJSON *packet);

#ifdef __cplusplus
}
#endif
#endif