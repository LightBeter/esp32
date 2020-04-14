#ifndef _APP_GLOBAL_H
#define _APP_GLOBAL_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#define APP_REQ_REBOOT_BIT      BIT0

extern EventGroupHandle_t _app_event_group;
extern wifi_init_config_t _wifi_init_cfg;
extern wifi_config_t      _wifi_cfg;

extern void wifi_init_sta();
extern void wifi_deinit_sta();
extern void network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

#endif