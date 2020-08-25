#include <string.h>
#include "wifi_ap_scan_tsk.h"

#define TAG             "WifiApScanTsk"
#define TSK_STACK_SIZE  2048

void wifi_ap_scan_tsk(void * parm);
void wifi_ap_scan();

wifi_ap_scan_tsk_type    _wifiApScan;
const wifi_scan_config_t _wifiApScanConfig = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = true,
#if 1    
    .scan_type = WIFI_SCAN_TYPE_PASSIVE,
    .scan_time.passive = 100,
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
}

void wifi_ap_scan_tsk_deinit()
{
    vEventGroupDelete(_wifiApScan.eventGroup);
}

void wifi_ap_scan_tsk_start()
{
    if (!_wifiApScan.handle) {
        xTaskCreate(wifi_ap_scan_tsk, TAG, TSK_STACK_SIZE, NULL, 3, &_wifiApScan.handle);
    }
}

void wifi_ap_scan_tsk_stop()
{
    if (_wifiApScan.handle) {
        vTaskDelete(_wifiApScan.handle);
        _wifiApScan.handle = NULL; 
    }
}

void wifi_ap_scan_tsk_get_list(cJSON *packet)
{
    cJSON *list = cJSON_CreateArray();
    cJSON *pSub;
  
    wifi_ap_scan();
    for(int index = 0; index < _wifiApScan.num; index++) {
        pSub = cJSON_CreateObject();
        cJSON_AddStringToObject(pSub, "ssid", (char*)_wifiApScan.list[index].ssid);
        cJSON_AddNumberToObject(pSub, "auth", _wifiApScan.list[index].authmode);
        cJSON_AddNumberToObject(pSub, "rssi", _wifiApScan.list[index].rssi);
        cJSON_AddItemToArray(list, pSub);
    }
    cJSON_AddItemToObject(packet, "list", list);
}

void wifi_ap_scan()
{
    _wifiApScan.num = WIFI_AP_SCAN_MAX;
    ESP_ERROR_CHECK( esp_wifi_scan_start(&_wifiApScanConfig, true) );
    ESP_ERROR_CHECK( esp_wifi_scan_get_ap_records(&_wifiApScan.num, _wifiApScan.list) );
}

void wifi_ap_scan_tsk(void * parm)
{  
    vTaskDelete(NULL);
}