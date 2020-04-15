#include <string.h>
#include "app_global.h"
#include "esp_smartconfig.h"

#define TAG                         "wifi"

#define WIFI_AP_SSID_PRE            "ESP32LED_"
#define WIFI_AP_DEFAULT_PASSWORD    "00000000"
#define WIFI_MAX_STA_CONN           4

#define WIFI_EVENT_SC_START         BIT0
#define WIFI_EVENT_SC_DONE          BIT1

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

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

const char *sc_event_desc[] = {
    [SC_EVENT_SCAN_DONE] = "ESP32 station smartconfig has finished to scan for APs", 
    [SC_EVENT_FOUND_CHANNEL] = "ESP32 station smartconfig has found the channel of the target AP",
    [SC_EVENT_GOT_SSID_PSWD] = "ESP32 station smartconfig got the SSID and password",
    [SC_EVENT_SEND_ACK_DONE] = "ESP32 station smartconfig has sent ACK to cellphone",
};

device_wifi_type _device_wifi = { 
    .mode  = WIFI_MODE_AP, 
    .state = device_wifi_state_idle,
    .cfg = {
        .ap = {
            .password = WIFI_AP_DEFAULT_PASSWORD,
            .max_connection = WIFI_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    },
};

void initialise_wifi(void)
{
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    _device_wifi.init_cfg = cfg;
    _device_wifi.event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_wifi_init(&_device_wifi.init_cfg) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL) );
    ESP_LOGI( TAG, "initialise wifi finished." );
}

void deinitialise_wifi(void)
{
    ESP_ERROR_CHECK( esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler) );
    ESP_ERROR_CHECK( esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler) );
    ESP_ERROR_CHECK( esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler) );
    ESP_ERROR_CHECK( esp_wifi_deinit() );
    vEventGroupDelete( _device_wifi.event_group );
    ESP_LOGI( TAG, "deinitialise wifi finished." );
}

void start_wifi_smartconfig()
{
    _device_wifi.state = device_wifi_state_sc;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_LOGI( TAG, "start wifi in sc mode finished." );
}

void start_wifi_sta()
{
    _device_wifi.state = device_wifi_state_sta;
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &_device_wifi.cfg) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
    ESP_LOGI( TAG, "start wifi in sta mode finished." );
}

void start_wifi_ap()
{
    uint8_t mac[6];
    esp_wifi_get_mac( ESP_IF_WIFI_AP , mac );
    sprintf( (char *)_device_wifi.cfg.ap.ssid, "%s_%02X%02X%02X" , WIFI_AP_SSID_PRE, mac[3], mac[4], mac[5] );
    _device_wifi.cfg.ap.ssid_len = strlen((const char*)_device_wifi.cfg.ap.ssid);
    _device_wifi.state = device_wifi_state_ap;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &_device_wifi.cfg) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_LOGI( TAG, "start wifi in ap mode finished." );
}

void stop_wifi()
{
    _device_wifi.state = device_wifi_state_stop;
    ESP_ERROR_CHECK( esp_wifi_stop() );
    ESP_LOGI( TAG, "stop wifi finished." );
}

void wifi_sc_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();

    /* start wifi in smartconfig */
    start_wifi_smartconfig();
    while (1) {
        uxBits = xEventGroupWaitBits(_device_wifi.event_group, WIFI_EVENT_SC_START | WIFI_EVENT_SC_DONE, true, false, pdMS_TO_TICKS(60000)); 
        if(uxBits & WIFI_EVENT_SC_START) {
            /* start smartconfig */
            ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
            ESP_LOGI( TAG, "start smartconfig finished." );
            continue;
        }

        if (uxBits & WIFI_EVENT_SC_DONE) {
            ESP_LOGI( TAG, "smartconfig done." );
        } else {
            ESP_LOGI( TAG, "smartconfig over." );
        }
        break;
    }
    /* stop smartconfig */
    esp_smartconfig_stop();
    if (device_wifi_state_sc == _device_wifi.state) {
        /* smart config timeout */
        if (WIFI_MODE_AP == _device_wifi.mode) {
            stop_wifi();
            start_wifi_ap();
        } else {
            start_wifi_sta();
        }
    }
    vTaskDelete(NULL);
}

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (WIFI_EVENT == event_base) {
        ESP_LOGI(TAG, "%s", wifi_event_desc[event_id]);
        if (device_wifi_state_sc == _device_wifi.state) {
            if (WIFI_EVENT_STA_START == event_id) {
                /* start smartconfig */
                xEventGroupSetBits(_device_wifi.event_group, WIFI_EVENT_SC_START);
            }
        } else if (device_wifi_state_sta == _device_wifi.state) {
            if (WIFI_EVENT_STA_START == event_id || WIFI_EVENT_STA_DISCONNECTED == event_id) {
                /* start/retry connection */
                esp_wifi_connect();
            } 
        }
    } else if (IP_EVENT == event_base) {
        ESP_LOGI(TAG, "%s", ip_event_desc[event_id]);
        if (IP_EVENT_STA_GOT_IP == event_id) {
            /* Got ip address */
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
        }
    } else if (SC_EVENT == event_base) {
        ESP_LOGI(TAG, "%s", sc_event_desc[event_id]);
        if (SC_EVENT_GOT_SSID_PSWD == event_id && device_wifi_state_sc == _device_wifi.state) {
            /* Got SSID and password */
            smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
            wifi_config_t *wifi_config = &_device_wifi.cfg;
            bzero( wifi_config, sizeof(wifi_config_t) );
            memcpy( wifi_config->sta.ssid, evt->ssid, sizeof(wifi_config->sta.ssid) );
            memcpy( wifi_config->sta.password, evt->password, sizeof(wifi_config->sta.password) );
            wifi_config->sta.bssid_set = evt->bssid_set;
            if (wifi_config->sta.bssid_set == true) {
                memcpy( wifi_config->sta.bssid, evt->bssid, sizeof(wifi_config->sta.bssid) );
            }
            _device_wifi.mode = WIFI_MODE_STA;
            ESP_LOGI( TAG, "SSID:%s", wifi_config->sta.ssid );
            ESP_LOGI( TAG, "PASSWORD:%s", wifi_config->sta.password );
            start_wifi_sta();
        } else if (SC_EVENT_SEND_ACK_DONE == event_id) {
            /* smartconfig Done */
            xEventGroupSetBits(_device_wifi.event_group, WIFI_EVENT_SC_DONE);
        }
    }
}

