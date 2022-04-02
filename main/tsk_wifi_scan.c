#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <string.h>
#include <esp_log.h>
#include "tsk_wifi_scan.h"
#include "wss_cmd_handle.h"
#include "websocket_server.h"

#define TAG                           "tsk_wifiscan"
#define TSK_STACK_SIZE                4096
#define WIFI_SCAN_MAX                 32
#define WIFI_SCAN_TSK_EVENT_TRIGGER   BIT0
#define WIFI_SCAN_TSK_EVENT_DONE      BIT1

void tsk_wifi_scan_lock();
void tsk_wifi_scan_unlock();
void broadcast_ap_info();
void wifi_ap_scan_tsk(void * parm);
void wifi_scan_eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static xTaskHandle        tsk_handle = NULL;
static SemaphoreHandle_t  mutex = NULL;
static EventGroupHandle_t wifi_scan_event_handle = NULL;
static wifi_ap_record_t   wifi_ap_list[WIFI_SCAN_MAX];
static uint16_t    wifi_ap_num = 0;
static uint16_t    trigger_scan_clientFlags = 0;

void tsk_init_wifi_scan()
{
    ESP_LOGI( TAG, "initialize" );
    mutex = xSemaphoreCreateMutex();
    wifi_scan_event_handle = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &wifi_scan_eventHandler, NULL));
}

void tsk_deinit_wifi_scan()
{
    ESP_LOGI( TAG, "deinitialize" );
    vEventGroupDelete(wifi_scan_event_handle);
}

void tsk_start_wifi_scan()
{
    ESP_LOGI( TAG, "start" );
    if (!tsk_handle) {
        xTaskCreate(wifi_ap_scan_tsk, TAG, TSK_STACK_SIZE, NULL, 5, &tsk_handle);
    }
}

void tsk_stop_wifi_scan()
{
    ESP_LOGI( TAG, "stop" );
    if (tsk_handle) {
        vTaskDelete(tsk_handle);
        tsk_handle = NULL; 
    }
}

void tsk_trigger_wifi_scan(int client)
{
    tsk_wifi_scan_lock();
    trigger_scan_clientFlags |=  0x1 << client;
    tsk_wifi_scan_unlock();
    xEventGroupSetBits(wifi_scan_event_handle, WIFI_SCAN_TSK_EVENT_TRIGGER);
}

void tsk_wifi_scan_lock()
{
    while( xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE );
}

void tsk_wifi_scan_unlock()
{
    while( xSemaphoreGive(mutex) != pdTRUE );
}

void wifi_scan_eventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (WIFI_EVENT_SCAN_DONE == event_id) {
        ESP_LOGI( TAG, "wifi ap scan done" );
        xEventGroupSetBits(wifi_scan_event_handle, WIFI_SCAN_TSK_EVENT_DONE);
    }  
}

void wifi_ap_scan_tsk(void * parm)
{  
    const wifi_scan_config_t wifi_scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
        .scan_time.passive = 300,
    };
    const EventBits_t uxBitsToWaitFor = WIFI_SCAN_TSK_EVENT_TRIGGER | WIFI_SCAN_TSK_EVENT_DONE;
    EventBits_t uxBits;
    bool scan_done = true;

    while (1) {
        uxBits = xEventGroupWaitBits(wifi_scan_event_handle, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000)); 
        if (uxBits & WIFI_SCAN_TSK_EVENT_DONE) {
            wifi_ap_num = WIFI_SCAN_MAX;
            scan_done = true;
            ESP_ERROR_CHECK( esp_wifi_scan_get_ap_records(&wifi_ap_num, wifi_ap_list) );
            broadcast_ap_info();
        }        
        if (uxBits & WIFI_SCAN_TSK_EVENT_TRIGGER && scan_done) {
            scan_done = false;
            ESP_LOGI( TAG, "wifi ap scan start" );
            ESP_ERROR_CHECK( esp_wifi_scan_start(&wifi_scan_config, false) );
        }
    }
    vTaskDelete(NULL);
}

void broadcast_ap_info()
{
    cJSON *packet = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();
    cJSON *pSub;
    char *out = NULL;
    uint16_t mask;
  
    for(int index = 0; index < wifi_ap_num; index++) {
        pSub = cJSON_CreateObject();
        cJSON_AddStringToObject(pSub, "ssid", (char*)wifi_ap_list[index].ssid);
        cJSON_AddNumberToObject(pSub, "auth", wifi_ap_list[index].authmode);
        cJSON_AddNumberToObject(pSub, "rssi", wifi_ap_list[index].rssi);
        cJSON_AddItemToArray(list, pSub);
    }
    cJSON_AddItemToObject(packet, "list", list);
    cJSON_AddNumberToObject(packet, "cmd", wss_cmd_b_wifi_ap_list);
    out = cJSON_PrintUnformatted(packet);
    cJSON_Delete(packet);

    tsk_wifi_scan_lock();
    for (int index = 0; index < 16; index++) {
        mask = 0x1 << index;
        if (trigger_scan_clientFlags & mask) {
            ws_server_send_text_client( index, out, strlen(out) );
            trigger_scan_clientFlags &= ~mask;
        }
        
    }
    tsk_wifi_scan_unlock();
    cJSON_free(out);
}