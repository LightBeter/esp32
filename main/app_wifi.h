#ifndef _APP_WIFI_H
#define _APP_WIFI_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include "esp_wifi.h"
#include "esp_event.h"

typedef enum {
    device_wifi_state_idle = 0x00,
    device_wifi_state_sc,
    device_wifi_state_sta,
    device_wifi_state_ap,
    device_wifi_state_stop,
} device_wifi_state;

typedef struct {
    wifi_mode_t         mode;
    wifi_init_config_t  init_cfg;
    wifi_config_t       cfg;
    device_wifi_state   state;
    EventGroupHandle_t  event_group;
} device_wifi_type;

void initialise_wifi(void);
void deinitialise_wifi(void);
void load_wifi_config(void);
void save_wifi_config(void);
void start_wifi_smartconfig();
void start_wifi_sta();
void start_wifi_ap();
void stop_wifi();
void wifi_sc_task(void * parm);

extern device_wifi_type        _device_wifi;

#endif