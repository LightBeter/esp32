#ifndef _DEVICE_H
#define _DEVICE_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_image_format.h>
#include <esp_wifi.h>

#define DEVICE_MODEL                       "ESP32"
#define DEVICE_SERIAL_NUM_LEN               12           
#define DEVICE_REQ_REBOOT_BIT               BIT0
#define DEVICE_REQ_RECOVERY_FACTORY_BIT     BIT1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    xTaskHandle          handle;
    esp_app_desc_t       appdesc;
    char                 serialnum[DEVICE_SERIAL_NUM_LEN + 1];
    char                 hostname[32];
    wifi_config_t        wificonf;
    EventGroupHandle_t   event_group;
} device_type;

void  device_init();
void  device_run();

const esp_app_desc_t* device_appdesc();
const char*           device_serialnum();
const char*           device_hostname();
const wifi_config_t*  device_wificonf();
void device_save_wificonf(const wifi_config_t* conf);
void device_set_event(const EventBits_t uxBitsToSet);

#ifdef __cplusplus
}
#endif
#endif