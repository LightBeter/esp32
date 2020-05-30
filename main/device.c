#include <stdio.h>
#include <string.h>
#include <mdns.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include "device.h"
#include "storage.h"
#include "datetime.h"
#include "wifi_tsk.h"
#include "wss_tsk.h"
#include "temp_ctrl_tsk.h"
#include "led_ctrl_tsk.h"

#define TAG                      "Device"
#define TSK_NAME                 "DeviceTsk"
#define DEVICE_TSK_STACK_SIZE    2048

void mdns_start();
void mdns_stop();
void device_main_tsk(void * parm);

device_type _device;

const esp_app_desc_t* device_appdesc()
{
    return &_device.appdesc;
}

const char* device_serialnum()
{
    return _device.serialnum;
}

const char* device_hostname()
{
    return _device.hostname;
}

const wifi_config_t* device_wificonf()
{
    return &_device.wificonf; 
}

void device_save_wificonf(const wifi_config_t* conf)
{
    if (0 == memcmp(&_device.wificonf, conf, sizeof(wifi_config_t))) {
        return;
    }
    memcpy(&_device.wificonf, conf, sizeof(wifi_config_t));
    storage_write_wificonf(&_device.wificonf);
}

void device_set_event(const EventBits_t uxBitsToSet)
{
    xEventGroupSetBits(_device.event_group, uxBitsToSet);
}

void mdns_start()
{ 
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set(_device.hostname) );
    ESP_ERROR_CHECK( mdns_instance_name_set(DEVICE_MODEL) );
    ESP_ERROR_CHECK( mdns_service_add("WebServer", "_websocket", "_tcp", 80, NULL, 0) );
}

void mdns_stop()
{
    mdns_free();
}

void device_init()
{
    /* Initialize tcp/ip protocol stack */
    tcpip_adapter_init();
    /* Initialize storage */
    storage_init();
    /* Initialize application desc */
    storage_read_appdesc(&_device.appdesc);
    /* Initialize wifi configure */
    storage_read_wificonf(&_device.wificonf);
    /* Initialize datetime */
    datetime_init();
    /* Initialize Wifi Task */
    wifi_tsk_init();
    /* Initialize WSS Task */
    wss_tsk_init();
    /* Initialize Temperature Control Task */
    temp_ctrl_tsk_init();
    /* Initialize Led Control Task */
    led_ctrl_tsk_init();
    /* Initialize hostname */
    uint8_t mac[6];
    ESP_ERROR_CHECK( esp_wifi_get_mac(ESP_IF_WIFI_AP , mac) );
    sprintf( _device.serialnum, "%02X%02X%02X%02X%02X%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
    sprintf( _device.hostname, DEVICE_MODEL "_%02X%02X%02X", mac[3], mac[4], mac[5] );
    /* device event group */
    _device.event_group = xEventGroupCreate();
}

void  device_run()
{
    xTaskCreate(device_main_tsk, TSK_NAME, DEVICE_TSK_STACK_SIZE, NULL, 3, &_device.handle);
}

void device_main_tsk(void * parm)
{
    size_t freeHeapSize;
    EventBits_t uxBits;
    const EventBits_t uxBitsToWaitFor = (DEVICE_REQ_REBOOT_BIT | DEVICE_REQ_RECOVERY_FACTORY_BIT);
    /* Start Task */
    ESP_LOGI( TAG, "Start mDNS." );
    mdns_start();
    ESP_LOGI( TAG, "Start Temperature Control Task." );
    temp_ctrl_tsk_start();
    ESP_LOGI( TAG, "Start Led Control Task." );
    led_ctrl_tsk_start();
    ESP_LOGI( TAG, "Start Wifi Task." );
    wifi_tsk_start();
    ESP_LOGI( TAG, "Start Websocket Task." );
    wss_tsk_start();

    while (1) {
        uxBits = xEventGroupWaitBits( _device.event_group, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000) ); 
        if (uxBits & DEVICE_REQ_RECOVERY_FACTORY_BIT) {
            ESP_LOGI( TAG, "user request recovery factory." );
            break;
        } 
        if (uxBits & DEVICE_REQ_REBOOT_BIT) {
            ESP_LOGI( TAG, "user request reboot." );
            break;
        } 

        freeHeapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        //ESP_LOGI( TAG, "heap free size %zu.", freeHeapSize );
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    /* stop Task */
    ESP_LOGI( TAG, "Stop Websocket Task." );
    wss_tsk_stop();
    ESP_LOGI( TAG, "Stop Wifi Task." );
    wifi_tsk_stop();
    ESP_LOGI( TAG, "Stop Led Control Task." );
    led_ctrl_tsk_stop();
    ESP_LOGI( TAG, "Stop Temperature Control Task." );
    temp_ctrl_tsk_stop();
    ESP_LOGI( TAG, "Stop mDNS." );
    mdns_stop();
    /* deInitialize */
    ESP_LOGI( TAG, "Deinitialise Websocket Task." );
    wss_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise Wifi Task." );
    wifi_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise Led Control Task." );
    led_ctrl_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise Temperature Control Task." );
    temp_ctrl_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise datetime." );
    datetime_deinit();
    ESP_LOGI( TAG, "Deinitialise Storage." );
    storage_deinit();
    /* reboot */
    ESP_LOGI( TAG, "Device reboot." );
    esp_restart();
    _device.handle = NULL;
    vTaskDelete(NULL);
}


