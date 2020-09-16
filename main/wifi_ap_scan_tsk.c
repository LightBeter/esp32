#include <string.h>
#include <esp_log.h>
#include "wifi_ap_scan_tsk.h"
#include "wss_cmd_handle.h"
#include "websocket_server.h"

#define TAG             "WifiApScanTsk"
#define TSK_STACK_SIZE  4096
#define WIFI_AP_SCAN_TSK_EVENT_TRIGGER   BIT0
#define WIFI_AP_SCAN_TSK_EVENT_DONE      BIT1

void wifi_ap_scan_lock();
void wifi_ap_scan_unlock();
void wifi_ap_scan_tsk(void * parm);
void wifi_ap_scan_tsk_broadcast();
void wifi_ap_scan_eventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

wifi_ap_scan_tsk_type    _wifiApScan;
const wifi_scan_config_t _wifiApScanConfig = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = true,
#if 1    
    .scan_type = WIFI_SCAN_TYPE_PASSIVE,
    .scan_time.passive = 300,
#else    
    .scan_type = WIFI_SCAN_TYPE_PASSIVE,
    .scan_time.active.min = 0,
    .scan_time.active.max = 0,
#endif
};

void wifi_ap_scan_tsk_init()
{
    _wifiApScan.num     = 0;
    _wifiApScan.eventGroup= xEventGroupCreate();
    _wifiApScan.mutex  = xSemaphoreCreateMutex();
    _wifiApScan.handle = NULL;
    _wifiApScan.clientFlags = 0; 
    _wifiApScan.isScanning  = false;
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &wifi_ap_scan_eventHandler, NULL));
}

void wifi_ap_scan_tsk_deinit()
{
    vEventGroupDelete(_wifiApScan.eventGroup);
}

void wifi_ap_scan_tsk_start()
{
    if (!_wifiApScan.handle) {
        xTaskCreate(wifi_ap_scan_tsk, TAG, TSK_STACK_SIZE, NULL, 5, &_wifiApScan.handle);
    }
}

void wifi_ap_scan_tsk_stop()
{
    if (_wifiApScan.handle) {
        vTaskDelete(_wifiApScan.handle);
        _wifiApScan.handle = NULL; 
    }
}

void wifi_ap_scan_lock()
{
    while( xSemaphoreTake(_wifiApScan.mutex, portMAX_DELAY) != pdTRUE );
}

void wifi_ap_scan_unlock()
{
    while( xSemaphoreGive(_wifiApScan.mutex) != pdTRUE );
}

void wifi_ap_scan_tsk_trigger(int client)
{
    wifi_ap_scan_lock();
    _wifiApScan.clientFlags |=  0x1 << client;
    wifi_ap_scan_unlock();
    xEventGroupSetBits(_wifiApScan.eventGroup, WIFI_AP_SCAN_TSK_EVENT_TRIGGER);
}

void wifi_ap_scan_eventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (WIFI_EVENT_SCAN_DONE == event_id) {
        ESP_LOGI( TAG, "wifi ap scan done" );
        xEventGroupSetBits(_wifiApScan.eventGroup, WIFI_AP_SCAN_TSK_EVENT_DONE);
    }  
}

void wifi_ap_scan_tsk(void * parm)
{  
    const EventBits_t uxBitsToWaitFor = WIFI_AP_SCAN_TSK_EVENT_TRIGGER | WIFI_AP_SCAN_TSK_EVENT_DONE;
    EventBits_t uxBits;

    while (1) {
        uxBits = xEventGroupWaitBits(_wifiApScan.eventGroup, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000)); 
        if (uxBits & WIFI_AP_SCAN_TSK_EVENT_DONE) {
            _wifiApScan.num = WIFI_AP_SCAN_MAX;
            ESP_ERROR_CHECK( esp_wifi_scan_get_ap_records(&_wifiApScan.num, _wifiApScan.list) );
            _wifiApScan.isScanning = false;
            wifi_ap_scan_tsk_broadcast();
        }        

        if (uxBits & WIFI_AP_SCAN_TSK_EVENT_TRIGGER && false == _wifiApScan.isScanning) {
            _wifiApScan.isScanning = true;
            ESP_ERROR_CHECK( esp_wifi_scan_start(&_wifiApScanConfig, false) );
        } 
    }
    vTaskDelete(NULL);
}

void wifi_ap_scan_tsk_broadcast()
{
    cJSON *packet = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    cJSON *pSub;
    char *out = NULL;
    uint16_t mask;
  
    for(int index = 0; index < _wifiApScan.num; index++) {
        pSub = cJSON_CreateObject();
        cJSON_AddStringToObject(pSub, "ssid", (char*)_wifiApScan.list[index].ssid);
        cJSON_AddNumberToObject(pSub, "auth", _wifiApScan.list[index].authmode);
        cJSON_AddNumberToObject(pSub, "rssi", _wifiApScan.list[index].rssi);
        cJSON_AddItemToArray(list, pSub);
    }
    cJSON_AddItemToObject(packet, "list", list);
    cJSON_AddNumberToObject(packet, "cmd", wss_cmd_b_wifi_ap_list);
    out = cJSON_PrintUnformatted(packet);
    cJSON_Delete(packet);

    for (int index = 0; index < 16; index++) {
        mask = 0x1 << index;
        wifi_ap_scan_lock();
        if (_wifiApScan.clientFlags & mask) {
            ws_server_send_text_client( index, out, strlen(out) );
            _wifiApScan.clientFlags &= ~mask;
        }
        wifi_ap_scan_unlock();
    }
    cJSON_free(out);
}