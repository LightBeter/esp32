#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include "tsk_wifi.h"
#include "tsk_udp_multicast.h"
#include "datetime.h"
#include "storage.h"
#include "misc.h"

#define TAG                         "WifiTsk"
#define TSK_NAME                    TAG
#define TSK_STACK_SIZE              4096
#define WIFI_AP_DEFAULT_PASSWORD    "00000000"
#define WIFI_AP_MAX_STA_CONN        10
#define WIFI_TSK_EVENT_STA_GOT_IP   BIT0
#define WIFI_TSK_EVENT_AP_START     BIT1

void wifi_start_sta();
void wifi_start_ap();
void wifi_stop();
void wifi_tsk(void * parm);
void wifi_eventHandler(void* arg, esp_event_base_t event_base,
                       int32_t event_id, void* event_data);

static xTaskHandle         tsk_handle = NULL;
static wifi_config_t       wifiConf;
static wifi_tsk_state      wifiState = wifi_tsk_state_idle;
static EventGroupHandle_t  wifiEventGroup;  
static esp_netif_t*        wifiStaNetif = NULL;
static esp_netif_t*        wifiApNetif  = NULL;
static char   staSSID[32] = "";
static char   staPWD[64]  = "";
static bool   wifiConnected = false;

void tsk_wifi_init()
{
    ESP_LOGI( TAG, " initialise" );
    const wifi_init_config_t initconf = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&initconf) );
    ESP_ERROR_CHECK( esp_wifi_restore() );
    ESP_ERROR_CHECK( esp_wifi_set_ps(WIFI_PS_NONE) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_eventHandler, NULL));
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &wifi_eventHandler, NULL));
    wifiEventGroup = xEventGroupCreate();
    wifiApNetif  = esp_netif_create_default_wifi_ap();
    wifiStaNetif = esp_netif_create_default_wifi_sta();
    if ( !storage_read_wifi_sta(staSSID, staPWD) ) {
        memset(staSSID, 0x00, 32);
        memset(staPWD, 0x00, 64);
    }
}

void tsk_wifi_deinit()
{
    ESP_LOGI( TAG, "deinitialize" );
    ESP_ERROR_CHECK( esp_wifi_deinit() );
    vEventGroupDelete(wifiEventGroup);
}

void tsk_wifi_start()
{
    ESP_LOGI( TAG, "start" );
    if (!tsk_handle) {
        xTaskCreate(wifi_tsk, TAG, TSK_STACK_SIZE, NULL, 5, &tsk_handle);
    }
}

void tsk_wifi_stop()
{
    ESP_LOGI( TAG, "stop" );
    if (tsk_handle) {
        vTaskDelete(tsk_handle);
        tsk_handle = NULL; 
    }
    wifi_stop();
}

void tsk_wifi_set(int mode, const char *ssid, const char *pwd)
{
    if (1 == mode) {
        memset(staSSID, 0x00, 32);
        memset(staPWD, 0x00, 64);
        memcpy(staSSID, ssid, strlen(ssid));
        memcpy(staPWD, pwd, strlen(pwd));
    } else if ( 2 == mode) {
        memset(staSSID, 0x00, 32);
        memset(staPWD, 0x00, 64);        
    } else {
        return;
    }
    storage_write_wifi_sta(staSSID, staPWD);
}

void wifi_start_sta()
{
    const char *ssid = staSSID;
    const char *password = staPWD;
    memset( &wifiConf, 0x00, sizeof(wifi_config_t) );
    memcpy(wifiConf.sta.ssid, ssid, strlen(ssid));
    memcpy(wifiConf.sta.password, password, strlen(password));
    wifiConf.sta.pmf_cfg.capable = true;
    wifiConf.sta.pmf_cfg.required = false;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifiConf) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    wifiState = wifi_tsk_state_sta;
    ESP_LOGI( TAG, "start wifi in sta mode finished." );
}

void wifi_start_ap()
{
    const char *ssid = hostname();
    const char *password = WIFI_AP_DEFAULT_PASSWORD;
    memset( &wifiConf, 0x00, sizeof(wifi_config_t) );
    memcpy( wifiConf.ap.password, password, strlen(password) + 1 );
    memcpy( wifiConf.ap.ssid, ssid, strlen(ssid) + 1 );
    wifiConf.ap.ssid_len = strlen(ssid);
    wifiConf.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifiConf.ap.max_connection = WIFI_AP_MAX_STA_CONN;
    wifiConf.ap.beacon_interval = 100;
    /* start wifi ap mode */
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &wifiConf) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    wifiState = wifi_tsk_state_ap;
    ESP_LOGI( TAG, "start wifi in ap mode finished." );
}

void wifi_stop()
{
    wifiState = wifi_tsk_state_idle;
    ESP_ERROR_CHECK( esp_wifi_stop() ); 
}

esp_netif_t* wifi_netif()
{
    if (wifi_tsk_state_sta == wifiState) {
        return wifiStaNetif;
    } else if (wifi_tsk_state_ap == wifiState) {
        return wifiApNetif;
    } else {
        return NULL;
    }
}

void wifi_eventHandler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (WIFI_EVENT_STA_START == event_id) {
            esp_netif_set_hostname(wifiStaNetif, hostname());
            esp_wifi_connect();
            ESP_LOGI( TAG, "station start" );
        } else if (WIFI_EVENT_STA_STOP == event_id) {
            wifiConnected = false;
            ESP_LOGI( TAG, "sstation start" );
        } else if (WIFI_EVENT_STA_CONNECTED == event_id) {
            ESP_LOGI( TAG, "station connected from AP" );
        } else if (WIFI_EVENT_STA_DISCONNECTED == event_id) {
            wifiConnected = false;
            if (wifi_tsk_state_sta == wifiState) esp_wifi_connect();
            ESP_LOGI( TAG, "station disconnected from AP" );
        } else if (WIFI_EVENT_AP_START == event_id) {
            ESP_LOGI( TAG, "Soft AP Start" );
            esp_netif_set_hostname(wifiApNetif, hostname()); 
            xEventGroupSetBits(wifiEventGroup, WIFI_TSK_EVENT_AP_START); 
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifiConnected = true;
        xEventGroupSetBits(wifiEventGroup, WIFI_TSK_EVENT_STA_GOT_IP);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI( TAG, "station got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_tsk(void * parm)
{
    bool        sta_done = false;
    uint32_t    tick = 0;
    EventBits_t uxBits;

    /* wifi handle */
    if (strlen((char*)staSSID) == 0) {
        wifi_start_ap();
        xEventGroupWaitBits(wifiEventGroup, WIFI_TSK_EVENT_AP_START, true, false, portMAX_DELAY); 
        goto time_sync_handle;
    }
    
    led_set_mode(INDICATOR_MODE_FLASH);
    wifi_start_sta();  
    while (tick++ < 60 && !sta_done) {
        uxBits = xEventGroupWaitBits(wifiEventGroup, WIFI_TSK_EVENT_STA_GOT_IP, true, false, pdMS_TO_TICKS(1000)); 
        if (uxBits & WIFI_TSK_EVENT_STA_GOT_IP) {
            sta_done = true;
            ESP_LOGI( TAG, "sta connect to ap done." );
        } 
    }

    if (!sta_done) {
        ESP_LOGI( TAG, "sta connect to ap time out, change to ap mode." );
        wifi_stop();
        wifi_start_ap();
        xEventGroupWaitBits(wifiEventGroup, WIFI_TSK_EVENT_AP_START, true, false, portMAX_DELAY);   
    } 

time_sync_handle:
    led_set_mode(INDICATOR_MODE_OPEN);
    tsk_udp_multicast_start();
    /* sntp handle */
    while(1) {
        if (wifi_tsk_state_sta == wifiState && wifiConnected) {
            datetime_sync_sntp();
        } else if (wifi_tsk_state_ap == wifiState) {
            datetime_sync_extrtc();
        }
        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }
    tsk_handle = NULL;    
    vTaskDelete(NULL);
}


