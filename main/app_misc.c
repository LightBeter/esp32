
#include <stdio.h>
#include "mdns.h"
#include "app_global.h"

#define TAG   "app misc"

char*               _device_hostname = NULL;
EventGroupHandle_t  _app_event_group;
TaskHandle_t        _app_task_handles[APP_TSK_COUNT];

void initialise_device(void)
{
    uint8_t mac[6];
    _device_hostname = malloc(32);
    esp_wifi_get_mac( ESP_IF_WIFI_AP , mac );
    sprintf( _device_hostname, "ESP32_%02X%02X%02X", mac[3], mac[4], mac[5] );
    ESP_LOGI( TAG, "initialise device finished." );
}

void deinitialise_device(void)
{
    if (_device_hostname) {
        free(_device_hostname);
        _device_hostname = NULL;
    }
    ESP_LOGI( TAG, "deinitialise device finished." );
}

void initialise_mdns(void)
{ 
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set(_device_hostname) );
    ESP_ERROR_CHECK( mdns_instance_name_set("ESP32S LED") );
    ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, NULL, 0) );
    ESP_LOGI( TAG, "initialise mdns finished." );
}

void deinitialise_mdns(void)
{
    mdns_free();
    ESP_LOGI( TAG, "deinitialise mdns finished." );
}

