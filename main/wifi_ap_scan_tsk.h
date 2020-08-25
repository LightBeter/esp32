#ifndef _WIFI_AP_SCAN_TSK_H
#define _WIFI_AP_SCAN_TSK_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_wifi.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_AP_SCAN_MAX            43

typedef struct {
    SemaphoreHandle_t   mutex;
    xTaskHandle         handle;
    EventGroupHandle_t  eventGroup;  
    wifi_ap_record_t    list[WIFI_AP_SCAN_MAX];
    uint16_t            num;
} wifi_ap_scan_tsk_type;

void wifi_ap_scan_tsk_init();
void wifi_ap_scan_tsk_deinit();
void wifi_ap_scan_tsk_start();
void wifi_ap_scan_tsk_stop();
void wifi_ap_scan_tsk_scan();
void wifi_ap_scan_tsk_get_list(cJSON *packet);

#ifdef __cplusplus
}
#endif
#endif