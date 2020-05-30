#include <string.h>
#include <esp_log.h>
#include "wss_cmd_handle.h"
#include "wss_tsk.h"
#include "device.h"
#include "datetime.h"
#include "led_ctrl_tsk.h"

#define TAG        "WSS_HANDLE"

wss_respond_code_t read_devinfo_handle(cJSON *request, cJSON *respone)
{
    cJSON_AddStringToObject(respone, "model", DEVICE_MODEL);
    cJSON_AddStringToObject(respone, "serial ", device_serialnum());
    cJSON_AddStringToObject(respone, "firmware ", device_appdesc()->version);
    cJSON_AddNumberToObject(respone, "group", LED_CHAN_COUNT);
    return wss_respond_code_ok;
}

wss_respond_code_t read_devtime_handle(cJSON *request, cJSON *respone)
{
    struct  timeval  tv;
    struct  timezone tz;
    gettimeofday(&tv, &tz);
    cJSON_AddNumberToObject(respone, "utc", tv.tv_sec);
    cJSON_AddNumberToObject(respone, "tz", (-tz.tz_minuteswest / 60));
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
    zone = cJSON_GetObjectItem(request, "tz");
    if (!cJSON_IsNumber(zone)) {
        ESP_LOGI(TAG, "%s invalid timezone.", __FUNCTION__);
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

wss_respond_code_t read_ledctrl_mode_handle(cJSON *request, cJSON *respone)
{
    led_ctrl_get_mode(respone);
    return wss_respond_code_ok;
}

wss_respond_code_t read_ledctrl_current_output_handle(cJSON *request, cJSON *respone)
{
    led_ctrl_get_current_output(respone);
    return wss_respond_code_ok;
}

wss_respond_code_t read_ledctrl_manual_handle(cJSON *request, cJSON *respone)
{
    led_ctrl_get_manual(respone);
    return wss_respond_code_ok;
}

wss_respond_code_t read_ledctrl_schedule_handle(cJSON *request, cJSON *respone)
{
    led_ctrl_get_schedule(respone);
    return wss_respond_code_ok;
}

wss_respond_code_t write_ledctrl_mode_handle(cJSON *request, cJSON *respone)
{
    if (true == led_ctrl_set_mode(request)) {
        return wss_respond_code_ok;
    } else {
        return wss_respond_code_invalid_param;
    }
}

wss_respond_code_t write_ledctrl_demo_output_handle(cJSON *request, cJSON *respone)
{
    if (true == led_ctrl_set_demo(request)) {
        return wss_respond_code_ok;
    } else {
        return wss_respond_code_invalid_param;
    }  
}

wss_respond_code_t write_ledctrl_manual_handle(cJSON *request, cJSON *respone)
{
    if (true == led_ctrl_set_manual(request)) {
        return wss_respond_code_ok;
    } else {
        return wss_respond_code_invalid_param;
    }  
}

wss_respond_code_t write_ledctrl_schedule_handle(cJSON *request, cJSON *respone)
{
    if (true == led_ctrl_set_schedule(request)) {
        return wss_respond_code_ok;
    } else {
        return wss_respond_code_invalid_param;
    } 
}