#include "app_global.h"

#define TAG  "network event"

const char *wifi_event_desc[] = {
    [WIFI_EVENT_WIFI_READY] = "ESP32 wifi ready",          
    [WIFI_EVENT_SCAN_DONE] = "ESP32 finish scanning AP",  
    [WIFI_EVENT_STA_START] = "ESP32 station start",       
    [WIFI_EVENT_STA_STOP]  = "ESP32 station stop",        
    [WIFI_EVENT_STA_CONNECTED] = "ESP32 station connected to AP",
    [WIFI_EVENT_STA_DISCONNECTED] = "ESP32 station disconnected from AP",
    [WIFI_EVENT_STA_AUTHMODE_CHANGE] = "the auth mode of AP connected by ESP32 station changed", 
    [WIFI_EVENT_STA_WPS_ER_SUCCESS] = "ESP32 station wps succeeds in enrollee mode",
    [WIFI_EVENT_STA_WPS_ER_FAILED] = "ESP32 station wps fails in enrollee mode", 
    [WIFI_EVENT_STA_WPS_ER_TIMEOUT] = "ESP32 station wps timeout in enrollee mode",
    [WIFI_EVENT_STA_WPS_ER_PIN] = "ESP32 station wps pin code in enrollee mode",
    [WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP] = "ESP32 station wps overlap in enrollee mode", 
    [WIFI_EVENT_AP_START] = "ESP32 soft-AP start",
    [WIFI_EVENT_AP_STOP] = "ESP32 soft-AP stop", 
    [WIFI_EVENT_AP_STACONNECTED] = "a station connected to ESP32 soft-AP",
    [WIFI_EVENT_AP_STADISCONNECTED] = "a station disconnected from ESP32 soft-AP",
    [WIFI_EVENT_AP_PROBEREQRECVED] = "Receive probe request packet in soft-AP interface",
};

const char *ip_event_desc[] = {
    [IP_EVENT_STA_GOT_IP] = "ESP32 station got IP from connected AP", 
    [IP_EVENT_STA_LOST_IP] = "ESP32 station lost IP and the IP is reset to 0",
    [IP_EVENT_AP_STAIPASSIGNED] = "ESP32 soft-AP assign an IP to a connected station",
    [IP_EVENT_GOT_IP6] = "ESP32 station or ap or ethernet interface v6IP addr is preferred",
    [IP_EVENT_ETH_GOT_IP] = "ESP32 ethernet got IP from connected AP",
};

void network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (WIFI_EVENT == event_base) {
        ESP_LOGI(TAG, "%s", wifi_event_desc[event_id]);
        if (WIFI_EVENT_STA_START == event_id) {
            esp_wifi_connect();
        } else if (WIFI_EVENT_STA_DISCONNECTED == event_id) {
            esp_wifi_connect();
        }
    } else if (IP_EVENT == event_base) {
        ESP_LOGI(TAG, "%s", ip_event_desc[event_id]);
        if (IP_EVENT_STA_GOT_IP == event_id) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
        }
    }
}
