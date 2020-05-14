#include <string.h>
#include <esp_event_loop.h>
#include <esp_event_legacy.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <mdns.h>
#include <esp_log.h>
#include "wifi_tsk.h"
#include "device.h"

#define TAG                         "WifiTsk"
#define TSK_NAME                    TAG
#define WIFI_TSK_STACK_SIZE         2048
#define WIFI_AP_DEFAULT_PASSWORD    "00000000"
#define WIFI_AP_MAX_STA_CONN        4

#define WIFI_TSK_EVENT_STA_GOT_IP   BIT0

void wifi_start_sta(const wifi_config_t *conf);
void wifi_start_ap(const char* hostname, const char* password);
void wifi_stop();
void wifi_tsk(void * parm);
esp_err_t wifi_eventHandler(void* ctx, system_event_t* event);

wifi_tsk_type _wifiTsk;

void wifi_tsk_init()
{
    const wifi_init_config_t initconf = WIFI_INIT_CONFIG_DEFAULT();
    _wifiTsk.handle = NULL;
    _wifiTsk.state = wifi_tsk_state_idle;
    _wifiTsk.initconf = initconf;
    _wifiTsk.event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_wifi_init(&_wifiTsk.initconf) );
    ESP_ERROR_CHECK( esp_event_loop_init(wifi_eventHandler, NULL) );
    ESP_LOGI( TAG, " initialise wifi finished." );
}

void wifi_tsk_deinit()
{
    ESP_ERROR_CHECK( esp_wifi_deinit() );
    vEventGroupDelete(_wifiTsk.event_group);
}

void wifi_tsk_start()
{
    if (!_wifiTsk.handle) {
        xTaskCreate(wifi_tsk, TAG, WIFI_TSK_STACK_SIZE, NULL, 3, &_wifiTsk.handle);
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
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &_wifiTsk.conf) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_LOGI( TAG, "start wifi in sta mode finished." );
}

void wifi_start_ap(const char* hostname, const char* password)
{
    _wifiTsk.state = wifi_tsk_state_sta;
    memset( &_wifiTsk.conf, 0x00, sizeof(wifi_config_t) );
    memcpy(_wifiTsk.conf.ap.password, password, strlen(password) + 1);
    memcpy( _wifiTsk.conf.ap.ssid, hostname, strlen(hostname) + 1 );
    _wifiTsk.conf.ap.ssid_len = strlen(hostname);
    _wifiTsk.conf.ap.max_connection = WIFI_AP_MAX_STA_CONN;
    _wifiTsk.conf.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
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

esp_err_t wifi_eventHandler(void* ctx, system_event_t* event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI( TAG, "station connected to AP" );
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGI( TAG, "station connected from AP" );
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        if (wifi_tsk_state_sta == _wifiTsk.state) {
            esp_wifi_connect();
        }
        ESP_LOGI( TAG, "station disconnected from AP" );
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(_wifiTsk.event_group, WIFI_TSK_EVENT_STA_GOT_IP);
        ESP_LOGI(TAG, "station got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    default:
        break;
    }
    return mdns_handle_system_event(ctx, event);
}

void wifi_tsk(void * parm)
{
    bool        sta_done = false;
    uint32_t    tick = 0;
    EventBits_t uxBits;
    const EventBits_t uxBitsToWaitFor = (WIFI_TSK_EVENT_STA_GOT_IP);
    const wifi_config_t *conf = device_wificonf(); 

    if (strlen((char*)conf->sta.ssid) == 0) {
        wifi_start_ap( device_hostname(), WIFI_AP_DEFAULT_PASSWORD );
        goto exit;
    }
    
    wifi_start_sta(conf);  
    while (1) {
        uxBits = xEventGroupWaitBits(_wifiTsk.event_group, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000)); 
        if (uxBits & WIFI_TSK_EVENT_STA_GOT_IP) {
            sta_done = true;
            ESP_LOGI( TAG, "sta connect to ap done." );
            break;
        } else if (++tick > 30) {
            ESP_LOGI( TAG, "sta connect to ap done time out." );
            break;
        }
    }
    if (!sta_done) {
        wifi_stop();
        wifi_start_ap( device_hostname(), WIFI_AP_DEFAULT_PASSWORD );  
    }
exit:
   _wifiTsk.handle = NULL;    
    vTaskDelete(NULL);
}