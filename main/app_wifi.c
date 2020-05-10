#include <string.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include "app_wifi.h"
#include "app_webserver.h"
#include "app_misc.h"

#define TAG                         "app_wifi"

#define NVS_WIFI_NAMESPACE          "wifi_storage"
#define WIFI_AP_DEFAULT_PASSWORD    "00000000"
#define WIFI_MAX_STA_CONN           4
  
#define APP_WIFI_EVENT_STA_GOT_IP      BIT0


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

device_wifi_type _device_wifi = { 
    .state = device_wifi_state_idle,
};

void initialise_wifi(void)
{
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    _device_wifi.init_cfg = cfg;
    _device_wifi.event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_wifi_init(&_device_wifi.init_cfg) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL) );
    load_wifi_config();
    ESP_LOGI( TAG, "initialise wifi finished." );
}

void deinitialise_wifi(void)
{
    stop_wifi();
    ESP_ERROR_CHECK( esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler) );
    ESP_ERROR_CHECK( esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler) );
    ESP_ERROR_CHECK( esp_wifi_deinit() );
    vEventGroupDelete( _device_wifi.event_group );
    ESP_LOGI( TAG, "deinitialise wifi finished." );
}

void start_wifi_sta(wifi_config_t *conf)
{
    _device_wifi.state = device_wifi_state_sta;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, conf) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_LOGI( TAG, "start wifi in sta mode finished." );
}

void start_wifi_ap(wifi_config_t *conf)
{
    _device_wifi.state = device_wifi_state_ap;
    memcpy(_device_wifi.cfg.ap.ssid, _device_hostname, strlen(_device_hostname) + 1);
    _device_wifi.cfg.ap.ssid_len = strlen((const char*)_device_wifi.cfg.ap.ssid);
    memcpy(_device_wifi.cfg.ap.password, WIFI_AP_DEFAULT_PASSWORD, strlen(WIFI_AP_DEFAULT_PASSWORD) + 1);
    _device_wifi.cfg.ap.max_connection = WIFI_MAX_STA_CONN;
    _device_wifi.cfg.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &_device_wifi.cfg) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_LOGI( TAG, "start wifi in ap mode finished." );
}

void stop_wifi()
{
    if (device_wifi_state_idle != _device_wifi.state) {
        _device_wifi.state = device_wifi_state_idle;
        ESP_ERROR_CHECK( esp_wifi_stop() );
        ESP_ERROR_CHECK( esp_wifi_restore() );
    }
    ESP_LOGI( TAG, "stop wifi finished." );
}

void load_wifi_config(void)
{
    esp_err_t err;
    nvs_handle_t handle;
    uint8_t mode;
    char ssid[32];
    char passwd[64];
    uint8_t bssid[6];
    uint8_t bssid_set;
    size_t  len = 0;

    err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "%s Error (%s) opening NVS handle!\n", __FUNCTION__, esp_err_to_name(err));
        return;
    }

    err = nvs_get_u8(handle, "wifi_mode", &mode);
    if (ESP_OK != err || (WIFI_MODE_AP != mode && WIFI_MODE_STA != mode)) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_mode %u!\n", __FUNCTION__, esp_err_to_name(err), mode);
        goto default_handle;
    }

    _device_wifi.mode = mode;
    if (WIFI_MODE_STA == mode) {
        len = sizeof(_device_wifi.cfg.sta.ssid);
        err = nvs_get_blob(handle, "wifi_ssid", ssid, &len);
        if (ESP_OK != err || len != sizeof(_device_wifi.cfg.sta.ssid)) {
            ESP_LOGW( TAG, "%s Error (%s) wifi_ssid!\n", __FUNCTION__, esp_err_to_name(err));
            goto default_handle;
        }
        len = sizeof(_device_wifi.cfg.sta.password);
        err = nvs_get_blob(handle, "wifi_passwd", passwd, &len);
        if (ESP_OK != err || len != sizeof(_device_wifi.cfg.sta.password)) {
            ESP_LOGW( TAG, "%s Error (%s) wifi_passwd!\n", __FUNCTION__, esp_err_to_name(err));
            goto default_handle;
        }
        err = nvs_get_u8(handle, "wifi_bssid_set", &bssid_set);
        if (ESP_OK != err) {
            ESP_LOGW( TAG, "%s Error (%s) wifi_bssid_set!\n", __FUNCTION__, esp_err_to_name(err));
            goto default_handle;
        }
        if (bssid_set) {
            len = sizeof(_device_wifi.cfg.sta.bssid);
            err = nvs_get_blob(handle, "wifi_bssid", bssid, &len);
            if (ESP_OK != err || len != sizeof(_device_wifi.cfg.sta.bssid)) {
                ESP_LOGW( TAG, "%s Error (%s) bssid!\n", __FUNCTION__, esp_err_to_name(err));
                goto default_handle;
            }
        }
        memcpy(_device_wifi.cfg.sta.ssid, ssid, sizeof(_device_wifi.cfg.sta.ssid));
        memcpy(_device_wifi.cfg.sta.password, passwd, sizeof(_device_wifi.cfg.sta.password));
        _device_wifi.cfg.sta.bssid_set = bssid_set;
        if (bssid_set) {
            memcpy(_device_wifi.cfg.sta.bssid, bssid, sizeof(_device_wifi.cfg.sta.bssid));
        } else {
            memset(_device_wifi.cfg.sta.bssid, 0x00, sizeof(_device_wifi.cfg.sta.bssid));
        }
    }
    nvs_close(handle);
    return;

default_handle:
    _device_wifi.mode = WIFI_MODE_AP;
    nvs_close(handle);
    return;    
}

void save_wifi_config(void)
{
    esp_err_t err;
    nvs_handle_t handle;

    err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "%s Error (%s) opening NVS handle!\n", __FUNCTION__, esp_err_to_name(err));
        return;
    }

    err = nvs_set_u8(handle, "wifi_mode", _device_wifi.mode);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_mode!\n", __FUNCTION__, esp_err_to_name(err));
    }

    if (WIFI_MODE_STA == _device_wifi.mode) {
        err = nvs_set_blob(handle, "wifi_ssid", _device_wifi.cfg.sta.ssid, sizeof(_device_wifi.cfg.sta.ssid));
        if (ESP_OK != err) {
            ESP_LOGW( TAG, "%s Error (%s) wifi_ssid!\n", __FUNCTION__, esp_err_to_name(err));
        }
        err = nvs_set_blob(handle, "wifi_passwd", _device_wifi.cfg.sta.password, sizeof(_device_wifi.cfg.sta.password));
        if (ESP_OK != err) {
            ESP_LOGW( TAG, "%s Error (%s) wifi_passwd!\n", __FUNCTION__, esp_err_to_name(err));
        }
        err = nvs_set_u8(handle, "wifi_bssid_set", _device_wifi.cfg.sta.bssid_set);
        if (ESP_OK != err) {
            ESP_LOGW( TAG, "%s Error (%s) wifi_bssid_set!\n", __FUNCTION__, esp_err_to_name(err));
        }
        err = nvs_set_blob(handle, "wifi_bssid", _device_wifi.cfg.sta.bssid, 6);
        if (ESP_OK != err) {
            ESP_LOGW( TAG, "%s Error (%s) wifi_bssid!\n", __FUNCTION__, esp_err_to_name(err));
        }
    }
    err = nvs_commit(handle);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) nvs_commit!\n", __FUNCTION__, esp_err_to_name(err));
    }
    nvs_close(handle);
    return;    
}

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (WIFI_EVENT == event_base) {
        ESP_LOGI(TAG, "%s", wifi_event_desc[event_id]);
        if (WIFI_EVENT_STA_START == event_id) {
            esp_wifi_connect();
        } else if (WIFI_EVENT_STA_CONNECTED == event_id) {
            start_webserver();
        } else if (WIFI_EVENT_STA_DISCONNECTED == event_id) {
            stop_webserver();
            esp_wifi_connect();
        } else if(WIFI_EVENT_AP_START == event_id) {
            start_webserver();
        } else if (WIFI_EVENT_AP_STOP == event_id) {
            stop_webserver();
        }   
    } else if (IP_EVENT == event_base) {
        ESP_LOGI(TAG, "%s", ip_event_desc[event_id]);
        if (IP_EVENT_STA_GOT_IP == event_id) {
            xEventGroupSetBits(_device_wifi.event_group, APP_WIFI_EVENT_STA_GOT_IP);
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
        }
    } 
}

void wifi_sc_task(void * parm)
{
    bool        sta_done = false;
    uint32_t    tick = 0;
    EventBits_t uxBits;
    const EventBits_t uxBitsToWaitFor =  (APP_WIFI_EVENT_STA_GOT_IP);

    if (WIFI_MODE_AP == _device_wifi.mode) {
        start_wifi_ap(&_device_wifi.cfg);  
        vTaskDelete(NULL);
        goto exit;
    }
    
    start_wifi_sta(&_device_wifi.cfg);  
    while (1) {
        uxBits = xEventGroupWaitBits(_device_wifi.event_group, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000)); 
        if (uxBits & APP_WIFI_EVENT_STA_GOT_IP) {
            sta_done = true;
            ESP_LOGI( TAG, "sta connect to ap done." );
            break;
        } else if (++tick > 30) {
            ESP_LOGI( TAG, "sta connect to ap done time out." );
            break;
        }
    }
    if (!sta_done) {
        stop_wifi();
        start_wifi_ap(&_device_wifi.cfg);  
    }
exit:
    vTaskDelete(NULL);
}

