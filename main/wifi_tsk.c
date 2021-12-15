#include <string.h>
#include <esp_event.h>
#include <esp_log.h>
#include "wifi_tsk.h"
#include "udp_multicast_tsk.h"
#include "datetime.h"
#include "device.h"

#define TAG                         "WifiTsk"
#define TSK_NAME                    TAG
#define TSK_STACK_SIZE              3072
#define WIFI_AP_DEFAULT_PASSWORD    "00000000"
#define WIFI_AP_MAX_STA_CONN        10

#define WIFI_TSK_EVENT_STA_GOT_IP   BIT0

void wifi_start_sta(const wifi_config_t *conf);
void wifi_start_ap(const char* hostname, const char* password);
void wifi_stop();
void wifi_tsk(void * parm);
void wifi_eventHandler(void* arg, esp_event_base_t event_base,
                       int32_t event_id, void* event_data);

wifi_tsk_type     _wifiTsk;
esp_netif_t*      _staNetif = NULL;
esp_netif_t*      _apNetif  = NULL;

void wifi_tsk_init()
{
    const wifi_init_config_t initconf = WIFI_INIT_CONFIG_DEFAULT();
    _wifiTsk.handle    = NULL;
    _wifiTsk.isConnect = false;
    _wifiTsk.state     = wifi_tsk_state_idle;
    _wifiTsk.initConf  = initconf;
    _wifiTsk.eventGroup= xEventGroupCreate();
    _apNetif  = esp_netif_create_default_wifi_ap();
    _staNetif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK( esp_wifi_init(&_wifiTsk.initConf) );
    ESP_ERROR_CHECK( esp_wifi_set_ps(WIFI_PS_NONE) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_eventHandler, NULL));
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &wifi_eventHandler, NULL));
    ESP_LOGI( TAG, " initialise wifi finished." );
}

void wifi_tsk_deinit()
{
    ESP_ERROR_CHECK( esp_wifi_deinit() );
    vEventGroupDelete(_wifiTsk.eventGroup);
}

void wifi_tsk_start()
{
    if (!_wifiTsk.handle) {
        xTaskCreate(wifi_tsk, TAG, TSK_STACK_SIZE, NULL, 5, &_wifiTsk.handle);
    }
}

void wifi_tsk_stop()
{
    if (_wifiTsk.handle) {
        vTaskDelete(_wifiTsk.handle);
        _wifiTsk.handle = NULL; 
    }
    wifi_stop();
}

void wifi_start_sta(const wifi_config_t *conf)
{
    _wifiTsk.state = wifi_tsk_state_sta;
    memcpy( &_wifiTsk.conf, conf, sizeof(wifi_config_t) );
    _wifiTsk.conf.sta.pmf_cfg.capable = true;
    _wifiTsk.conf.sta.pmf_cfg.required = false;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &_wifiTsk.conf) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_LOGI( TAG, "start wifi in sta mode finished." );
}

void wifi_start_ap(const char* hostname, const char* password)
{
    _wifiTsk.state = wifi_tsk_state_ap;
    memset( &_wifiTsk.conf, 0x00, sizeof(wifi_config_t) );
    memcpy( _wifiTsk.conf.ap.password, password, strlen(password) + 1 );
    memcpy( _wifiTsk.conf.ap.ssid, hostname, strlen(hostname) + 1 );
    _wifiTsk.conf.ap.ssid_len = strlen(hostname);
    _wifiTsk.conf.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    _wifiTsk.conf.ap.max_connection = WIFI_AP_MAX_STA_CONN;
    _wifiTsk.conf.ap.beacon_interval = 100;
    /* start wifi ap mode */
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &_wifiTsk.conf) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_LOGI( TAG, "start wifi in ap mode finished." );
}

void wifi_stop()
{
    if (wifi_tsk_state_idle != _wifiTsk.state) {
        ESP_ERROR_CHECK( esp_wifi_stop() );
        ESP_ERROR_CHECK( esp_wifi_restore() );
        _wifiTsk.state = wifi_tsk_state_idle;
    }   
}

esp_netif_t* wifi_netif()
{
    if (wifi_tsk_state_sta == _wifiTsk.state) {
        return _staNetif;
    } else if (wifi_tsk_state_ap == _wifiTsk.state) {
        return _apNetif;
    } else {
        return NULL;
    }
}

void wifi_eventHandler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (WIFI_EVENT_STA_START == event_id) {
            esp_netif_set_hostname(_staNetif, device_hostname());
            esp_wifi_connect();
            ESP_LOGI( TAG, "station start" );
        } else if (WIFI_EVENT_STA_STOP == event_id) {
            _wifiTsk.isConnect = false;
            ESP_LOGI( TAG, "sstation start" );
        } else if (WIFI_EVENT_STA_CONNECTED == event_id) {
            ESP_LOGI( TAG, "station connected from AP" );
        } else if (WIFI_EVENT_STA_DISCONNECTED == event_id) {
            _wifiTsk.isConnect = false;
            if (wifi_tsk_state_sta == _wifiTsk.state) esp_wifi_connect();
            ESP_LOGI( TAG, "station disconnected from AP" );
        } else if (WIFI_EVENT_AP_START == event_id) {
            esp_netif_set_hostname(_apNetif, device_hostname());  
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        _wifiTsk.isConnect = true;
        xEventGroupSetBits(_wifiTsk.eventGroup, WIFI_TSK_EVENT_STA_GOT_IP);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI( TAG, "station got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_tsk(void * parm)
{
    const wifi_config_t *conf = device_wificonf(); 
    const EventBits_t uxBitsToWaitFor = WIFI_TSK_EVENT_STA_GOT_IP;
    bool        sta_done = false;
    uint32_t    tick = 0;
    EventBits_t uxBits;

    /* wifi handle */
    if (strlen((char*)conf->sta.ssid) == 0) {
        wifi_start_ap( device_hostname(), WIFI_AP_DEFAULT_PASSWORD );
        goto time_sync_handle;
    }
    
    device_set_led_mode(DEVICE_LED_MODE_FLASH);
    wifi_start_sta(conf);  
    while (tick++ < 60 && !sta_done) {
        uxBits = xEventGroupWaitBits(_wifiTsk.eventGroup, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000)); 
        if (uxBits & WIFI_TSK_EVENT_STA_GOT_IP) {
            sta_done = true;
            ESP_LOGI( TAG, "sta connect to ap done." );
        } 
    }

    if (!sta_done) {
        ESP_LOGI( TAG, "sta connect to ap time out, change to ap mode." );
        wifi_stop();
        wifi_start_ap( device_hostname(), WIFI_AP_DEFAULT_PASSWORD );  
    } 

time_sync_handle:
    device_set_led_mode(DEVICE_LED_MODE_OPEN);
    ESP_LOGI( TAG, "Start Udp Multicast Task." );
    udp_multicast_tsk_start();
    /* sntp handle */
    while(1) {
        if (wifi_tsk_state_sta == _wifiTsk.state && _wifiTsk.isConnect) {
            datetime_sync_sntp();
        } else if (wifi_tsk_state_ap == _wifiTsk.state) {
            datetime_sync_extrtc();
        }
        vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    }
   _wifiTsk.handle = NULL;    
    vTaskDelete(NULL);
}


