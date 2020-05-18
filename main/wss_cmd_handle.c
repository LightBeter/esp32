#include <string.h>
#include <esp_log.h>
#include "wss_cmd_handle.h"
#include "wss_tsk.h"
#include "device.h"
#include "datetime.h"
#include "led_ctrl.h"

#define TAG        "WSS_HANDLE"

wss_respond_code_t read_devinfo_handle(cJSON *request, cJSON *respone)
{
    cJSON_AddStringToObject(respone, "module", "esp32");
    cJSON_AddStringToObject(respone, "hw", "1.0.0");
    cJSON_AddStringToObject(respone, "sw", device_appdesc()->version);
    return wss_respond_code_ok;
}

wss_respond_code_t read_devtime_handle(cJSON *request, cJSON *respone)
{
    struct  timeval  tv;
    struct  timezone tz;
    gettimeofday(&tv, &tz);
    cJSON_AddNumberToObject(respone, "utc", tv.tv_sec);
    cJSON_AddNumberToObject(respone, "zone", (-tz.tz_minuteswest / 60));
    return wss_respond_code_ok;
}

wss_respond_code_t write_devtime_handle(cJSON *request, cJSON *respone)
{
    cJSON *utc, *zone;

    utc = cJSON_GetObjectItem(request, "utc");
    if (!cJSON_IsNumber(utc)) {
        ESP_LOGI(TAG, "%s invalid utc.", __FUNCTION__);
        return wss_respond_code_invalid_param;
    } 
    zone = cJSON_GetObjectItem(request, "zone");
    if (!cJSON_IsNumber(zone)) {
        ESP_LOGI(TAG, "%s invalid zone.", __FUNCTION__);
        return wss_respond_code_invalid_param;
    } 
    datetime_set( utc->valuedouble, zone->valuedouble );
    return wss_respond_code_ok;
}

wss_respond_code_t write_devnetwork_handle(cJSON *request, cJSON *respone)
{
    int mode;
    cJSON *item;
    size_t len;
    wifi_config_t conf;

    memset( &conf, 0x00, sizeof(wifi_config_t) );
    item = cJSON_GetObjectItem(request, "mode");
    if (!cJSON_IsNumber(item)) {
        ESP_LOGI(TAG, "%s get mode failure.", __FUNCTION__);
        return wss_respond_code_invalid_param;
    } 
    mode = item->valueint;
    if (WIFI_MODE_STA == mode) {
        item = cJSON_GetObjectItem(request, "ssid");
        if (!cJSON_IsString(item)) {
            ESP_LOGI(TAG, "%s invalid ssid.", __FUNCTION__);
            return wss_respond_code_invalid_param;
        } 
        len = strlen(item->valuestring);
        if (len <= 0 || len >= 32) {
            ESP_LOGI(TAG, "%s invalid ssid len(%zu) failure.", __FUNCTION__, len);
            return wss_respond_code_invalid_param;
        }
        memcpy(&conf.sta.ssid, item->valuestring, len);

        item = cJSON_GetObjectItem(request, "password");
        if (!cJSON_IsString(item)) {
            ESP_LOGI(TAG, "%s invalid password.", __FUNCTION__);
           return wss_respond_code_invalid_param;
        }
        len = strlen(item->valuestring);
        if (len >= 64) {
            ESP_LOGI(TAG, "%s invalid password len(%zu).", __FUNCTION__, len);
            return wss_respond_code_invalid_param;
        }
        memcpy(&conf.sta.password, item->valuestring, len);
    } else if (WIFI_MODE_AP == mode) {
        /* nothing to do */
    } else {
        ESP_LOGI(TAG, "%s invalid mode %d.", __FUNCTION__, mode);
        return wss_respond_code_invalid_param;  
    }
    ESP_LOGI(TAG, "%s save network config\r\n\tmode: %d\r\n\tssid: %s\r\n\tpassword: %s.", 
                    __FUNCTION__, mode, conf.sta.ssid, conf.sta.password);
    device_save_wificonf( &conf );
    return wss_respond_code_ok;
}

wss_respond_code_t exec_reboot_handle(cJSON *request, cJSON *respone)
{
    device_set_event(DEVICE_REQ_REBOOT_BIT);
    return wss_respond_code_ok; 
}

wss_respond_code_t exec_recovery_factory_handle(cJSON *request, cJSON *respone)
{
    device_set_event(DEVICE_REQ_RECOVERY_FACTORY_BIT);
    return wss_respond_code_ok; 
}

wss_respond_code_t exec_ledtemporary_handler(cJSON *request, cJSON *respone)
{
    cJSON *item, *pSub;
    uint8_t chl, value[LED_CHL_COUNT], active;

    /* active */
    item = cJSON_GetObjectItem(request, "active");
    if (!cJSON_IsNumber(item)) {
        ESP_LOGI(TAG, "%s invalid active.", __FUNCTION__);
        return wss_respond_code_invalid_param; 
    }
    if (item->valueint < 1 || item->valueint > 180) {
        ESP_LOGI(TAG, "%s invalid active %d.", __FUNCTION__, item->valueint);
        return wss_respond_code_invalid_param; 
    }
    active = item->valueint;
    /* channel */
    item = cJSON_GetObjectItem(request, "targ");
    if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) !=  LED_CHL_COUNT) {
        ESP_LOGI(TAG, "%s invalid targ.", __FUNCTION__);
        return wss_respond_code_invalid_param; 
    }
    for (chl = 0; chl < LED_CHL_COUNT; chl++) {
        pSub = cJSON_GetArrayItem(item, chl);
        if(NULL == pSub ){ 
            ESP_LOGI(TAG, "%s invalid channel %d targ failure.", __FUNCTION__, chl);
            return wss_respond_code_invalid_param; 
        }
        if (!cJSON_IsNumber(pSub)) {
            ESP_LOGI(TAG, "%s invalid channel %d targ.", __FUNCTION__, chl);
            return wss_respond_code_invalid_param; 
        }
        if (pSub->valueint < 0 || pSub->valueint > 100) {
            ESP_LOGI(TAG, "%s invalid channel %d targ %d.", __FUNCTION__, chl, item->valueint);
            return wss_respond_code_invalid_param;  
        }
        value[chl] = pSub->valueint;
    }
    return wss_respond_code_ok; 
}