
#include <stdio.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_wifi.h>
#include <mdns.h>
#include <esp_log.h>
#include "app_misc.h"
#include "app_wifi.h"
#include "app_udp.h"

#define TAG   "app misc"

esp_app_desc_t      _running_app_info;
char*               _device_hostname = NULL;
EventGroupHandle_t  _app_event_group;
TaskHandle_t        _app_task_handles[APP_TSK_COUNT];

void load_app_desc(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGW(TAG, "Current running partition address 0x%08x", running->address);
    if (esp_ota_get_partition_description(running, &_running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware(%s) version: %s", _running_app_info.project_name, _running_app_info.version);
    } else {
        ESP_LOGW(TAG, "load firmware version failure");  
    }
}

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
    ESP_ERROR_CHECK( mdns_instance_name_set("ESP32 LED") );
    ESP_ERROR_CHECK( mdns_service_add("HttpServer", "_http", "_tcp", 80, NULL, 0) );
    ESP_LOGI( TAG, "initialise mdns finished." );
}

void deinitialise_mdns(void)
{
    mdns_free();
    ESP_LOGI( TAG, "deinitialise mdns finished." );
}

void app_start_tsk()
{
    xTaskCreate(app_wifi_tsk, "app_wifi_tsk", APP_WIFI_TSK_STACK_SIZE, NULL, 3, &_app_task_handles[APP_TSK_WIFI]);
    xTaskCreate(app_udp_tsk, "app_udp_tsk", APP_UDP_TSK_STACK_SIZE, NULL, 3, &_app_task_handles[APP_TSK_UDP]);
}

void app_stop_tsk()
{
    eTaskState state;
    for(int i = 0; i < APP_TSK_COUNT; i++){
        state = eTaskGetState(_app_task_handles[i]);
        ESP_LOGI( TAG, "Task %d state %d.", i, state );
        if (eDeleted != state && eReady != state) {
            vTaskDelete(_app_task_handles[i]);   //Delete user task first (prevents the resetting of an unsubscribed task)
        }
    }
}