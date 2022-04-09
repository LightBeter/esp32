#include <string.h>
#include <esp_log.h>
#include "wss_cmd_handle.h"
#include "tsk_wss.h"
#include "datetime.h"
#include "tsk_tempctrl.h"
#include "tsk_ledctrl.h"
#include "tsk_ledctrl_misc.h"
#include "tsk_wifi_scan.h"
#include "tsk_wifi.h"
#include "misc.h"
#include "ota.h"

#define TAG        "WSS_HANDLE"

bool get_time_by_json(const cJSON *packet, const char *name, uint16_t *value, uint16_t max);
bool get_output_by_json(const cJSON *packet, const char *name, led_value_t *targ);
void set_outpu_to_json(cJSON *packet, const char *name, const led_value_t *targ);

bool get_time_by_json(const cJSON *packet, const char *name, uint16_t *value, uint16_t max) 
{
    cJSON *pItem = cJSON_GetObjectItem(packet, name);
     if (!cJSON_IsNumber(pItem)) {
        ESP_LOGI(TAG, "%s invalid param %s.", __FUNCTION__, name);
        return false;
    } else if (pItem->valueint < 0 || pItem->valueint > max) {
        ESP_LOGI(TAG, "%s invalid param %s value %d, range[%u-%u].", 
                __FUNCTION__, name, pItem->valueint, 0, max);
        return false;  
    } else {
        *value = pItem->valueint;
        return true;
    }  
}

bool get_output_by_json(const cJSON *packet, const char *name, led_value_t *targ) 
{
    cJSON *pItem, *pSub;

    pItem = cJSON_GetObjectItem(packet, name);
    if (!cJSON_IsArray(pItem) || cJSON_GetArraySize(pItem) !=  LED_CHAN_COUNT) {
        ESP_LOGI(TAG, "%s invalid param %s.", __FUNCTION__, name);
        return false; 
    }
    for (int index = 0; index < LED_CHAN_COUNT; index++) {
        pSub = cJSON_GetArrayItem(pItem, index);
        if(NULL == pSub ){ 
            ESP_LOGI(TAG, "%s get channel %d value failure.", __FUNCTION__, index);
            return false; 
        }
        if (!cJSON_IsNumber(pSub)) {
            ESP_LOGI(TAG, "%s invalid channel %d value.", __FUNCTION__, index);
            return false; 
        }
        if (!is_vaild_output(pSub->valueint)) {
            ESP_LOGI(TAG, "%s invalid channel %d value %d, range[0-100].", 
                     __FUNCTION__, index, pSub->valueint);
            return false;  
        }
        targ[index] = pSub->valueint;
    }
    return true;
}

void set_outpu_to_json(cJSON *packet, const char *name, const led_value_t *targ)
{
    int values[LED_CHAN_COUNT];
    cJSON *array;

    for (int i = 0; i < LED_CHAN_COUNT; i++) {
       values[i] =  targ[i];
    }
    array = cJSON_CreateIntArray(values, LED_CHAN_COUNT);
    cJSON_AddItemToObject(packet, name, array);
}


wss_respond_code_t read_dev_info_handle(cJSON *request, cJSON *respone)
{
    cJSON_AddStringToObject(respone, "model", DEVICE_MODEL);
    cJSON_AddStringToObject(respone, "serial", serialnum());
    cJSON_AddStringToObject(respone, "firmware", app_version());
    cJSON_AddNumberToObject(respone, "group", LED_CHAN_COUNT);
    cJSON_AddBoolToObject(respone, "fanspeedctrl", 1);
    return wss_respond_code_ok;
}

wss_respond_code_t read_dev_time_handle(cJSON *request, cJSON *respone)
{
    time_t  utc;
    int tz;
    datetime_get(&utc, &tz);
    cJSON_AddNumberToObject(respone, "utc", utc);
    cJSON_AddNumberToObject(respone, "tz", tz);
    return wss_respond_code_ok;
}

wss_respond_code_t write_dev_time_handle(cJSON *request, cJSON *respone)
{
    cJSON *utc, *zone;
    utc = cJSON_GetObjectItem(request, "utc");
    if (!cJSON_IsNumber(utc)) {
        ESP_LOGE(TAG, "%s invalid utc.", __FUNCTION__);
        return wss_respond_code_invalid_param;
    } 
    zone = cJSON_GetObjectItem(request, "tz");
    if (!cJSON_IsNumber(zone)) {
        ESP_LOGE(TAG, "%s invalid timezone.", __FUNCTION__);
        return wss_respond_code_invalid_param;
    } 
    datetime_set_tz( zone->valuedouble );
    datetime_set( utc->valuedouble );
    return wss_respond_code_ok;
}

wss_respond_code_t read_fan_duty_handle(cJSON *request, cJSON *respone)
{
    cJSON_AddNumberToObject(respone, "duty", tempctrl_get_fanduty());
    return wss_respond_code_ok;
}

wss_respond_code_t write_fan_duty_handle(cJSON *request, cJSON *respone)
{
    wss_respond_code_t ret = wss_respond_code_ok;
    cJSON *duty, *play;

    duty = cJSON_GetObjectItem(request, "duty");
    if (duty) {
        if (!cJSON_IsNumber(duty)) {
            ESP_LOGI(TAG, "%s invalid utc.", __FUNCTION__);
            ret = wss_respond_code_invalid_param;
        } 
    }

    play = cJSON_GetObjectItem(request, "play");
    if (play) {
        if (!cJSON_IsNumber(play)) {
            ESP_LOGI(TAG, "%s invalid play time.", __FUNCTION__);
            ret = wss_respond_code_invalid_param;
        } 
    }

    if ( !duty && !play ) {
        ret = wss_respond_code_invalid_param;   
    } else if (wss_respond_code_ok == ret) {
        if (duty)
            tempctrl_set_fanduty( duty->valueint );
        if (play)
            tempctrl_set_fanplay( play->valueint );
    }
    return ret;   
}

wss_respond_code_t write_dev_network_handle(cJSON *request, cJSON *respone)
{
    int mode;
    cJSON *item;
    const char *ssid, *pwd;
    size_t len;

    item = cJSON_GetObjectItem(request, "mode");
    if (!cJSON_IsNumber(item)) {
        ESP_LOGI(TAG, "%s get mode failure.", __FUNCTION__);
        return wss_respond_code_invalid_param;
    } 
    mode = item->valueint;
    if (1 == mode) {
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
        ssid = item->valuestring;

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
        pwd = item->valuestring;
        tsk_wifi_set(mode, ssid, pwd);
        ESP_LOGI(TAG, "%s save network config(sta)\r\n\tssid: %s\r\n\tpassword: %s.", __FUNCTION__, ssid, pwd);
    } else if (2 == mode) {
        tsk_wifi_set(mode, NULL, NULL);
        ESP_LOGI(TAG, "%s save network config(ap)", __FUNCTION__);
    } else {
        ESP_LOGI(TAG, "%s invalid mode %d.", __FUNCTION__, mode);
        return wss_respond_code_invalid_param;  
    }
    return wss_respond_code_ok;
}

wss_respond_code_t exec_wifi_ap_scan_handle(int client, cJSON *request, cJSON *respone)
{
    tsk_trigger_wifi_scan(client);
    return wss_respond_code_ok;
}

wss_respond_code_t exec_reboot_handle(cJSON *request, cJSON *respone)
{
    global_event_set(GLOBAL_EVENT_REBOOT);
    return wss_respond_code_ok; 
}

wss_respond_code_t exec_recovery_factory_handle(cJSON *request, cJSON *respone)
{
    global_event_set(GLOBAL_EVENT_RECOVERY_FACTORY);
    return wss_respond_code_ok; 
}

wss_respond_code_t read_ledctrl_state_handle(cJSON *request, cJSON *respone)
{
    /* time */
    time_t  utc;
    int tz;
    datetime_get(&utc, &tz);
    cJSON_AddNumberToObject(respone, "utc", utc);
    cJSON_AddNumberToObject(respone, "tz", tz);
    /* temperature */
    cJSON_AddNumberToObject(respone, "temperature", tempctrl_get_temperature());
    /* switch & mode & current output */
    tsk_ledctrl_lock();
    cJSON_AddNumberToObject(respone, "switch", ledctrl_switch());
    cJSON_AddNumberToObject(respone, "mode", ledctrl_mode());
    set_outpu_to_json(respone, "values", ledctrl_current_output());
    tsk_ledctrl_unlock();
    return wss_respond_code_ok;
}

wss_respond_code_t read_ledctrl_mode_handle(cJSON *request, cJSON *respone)
{
    tsk_ledctrl_lock();
    cJSON_AddNumberToObject(respone, "switch", ledctrl_switch());
    cJSON_AddNumberToObject(respone, "mode", ledctrl_mode());
    tsk_ledctrl_unlock();
    return wss_respond_code_ok;
}

wss_respond_code_t read_ledctrl_current_output_handle(cJSON *request, cJSON *respone)
{
    tsk_ledctrl_lock();
    set_outpu_to_json(respone, "values", ledctrl_current_output());
    tsk_ledctrl_unlock();
    return wss_respond_code_ok;
}

wss_respond_code_t read_ledctrl_manual_handle(cJSON *request, cJSON *respone)
{
    tsk_ledctrl_lock();
    set_outpu_to_json(respone, "values", ledctrl_manual_output());
    tsk_ledctrl_unlock();
    return wss_respond_code_ok;
}

wss_respond_code_t read_ledctrl_schedule_handle(cJSON *request, cJSON *respone)
{
    cJSON *pSchedule = cJSON_CreateArray();
    cJSON *pSub;
    const led_sch_item_t *item;   

    tsk_ledctrl_lock();
    for(int index = 0; index < LED_CTRL_SCH_MAX; index++) {
        item = ledctrl_sch_item(index);
        if (item) {
            pSub = cJSON_CreateObject();
            cJSON_AddNumberToObject(pSub, "t", item->time);
            set_outpu_to_json(pSub, "values", item->targ);
            cJSON_AddItemToArray(pSchedule, pSub);
        }
    }
    cJSON_AddNumberToObject(respone, "id", ledctrl_sch_id());
    cJSON_AddNumberToObject(respone, "power", ledctrl_power());
    tsk_ledctrl_unlock();
    cJSON_AddItemToObject(respone, "schedule", pSchedule);
    return wss_respond_code_ok;
}

wss_respond_code_t write_ledctrl_mode_handle(cJSON *request, cJSON *respone)
{
    cJSON *sw, *mode;

    sw = cJSON_GetObjectItem(request, "switch");
    if (!cJSON_IsNumber(sw)) {
        ESP_LOGE(TAG, "%s invalid param switch.", __FUNCTION__);
        goto err_exit;
    }

    mode = cJSON_GetObjectItem(request, "mode");
     if (!cJSON_IsNumber(mode)) {
        ESP_LOGI(TAG, "%s invalid param mode.", __FUNCTION__);
        goto err_exit;
    } 

    if (ledctrl_set_mode(sw->valueint != 0, mode->valueint)) 
        return wss_respond_code_ok;
err_exit:
    return wss_respond_code_invalid_param;
}

wss_respond_code_t write_ledctrl_demo_output_handle(cJSON *request, cJSON *respone)
{
    uint16_t    lifetime = 0;
    led_value_t targ[LED_CHAN_COUNT];
    /* lifetime */
    if (!get_time_by_json(request, "lifetime", &lifetime, 180))
        goto err_exit;
    /* channel values */
    if (!get_output_by_json(request, "values", targ))
        goto err_exit;

    if (ledctrl_set_demo(lifetime, targ)) 
        return wss_respond_code_ok;
err_exit:
    return wss_respond_code_invalid_param; 
}

wss_respond_code_t write_ledctrl_manual_handle(cJSON *request, cJSON *respone)
{
    led_value_t targ[LED_CHAN_COUNT];
    /* channel values */
    if (true != get_output_by_json(request, "values", targ))
        goto err_exit;

    if (ledctrl_set_manual(targ)) 
        return wss_respond_code_ok;
err_exit:
    return wss_respond_code_invalid_param;
}

wss_respond_code_t write_ledctrl_schedule_handle(cJSON *request, cJSON *respone)
{
    cJSON *pId, *pPower, *pSchedule, *pSub;
    led_value_t power = LED_VALUE_MAX, targ[LED_CHAN_COUNT];
    uint16_t  t[LED_CTRL_SCH_MAX];
    int index, tIndex, count;

    /* check schedule Id */
    pId = cJSON_GetObjectItem(request, "id");
    if (!cJSON_IsNumber(pId)) {
        ESP_LOGI(TAG, "%s invalid param id.", __FUNCTION__);
        goto err_exit;
    }
    /* check power Id */
    pPower = cJSON_GetObjectItem(request, "power");
    if (cJSON_IsNumber(pPower))
        power = pPower->valueint;

    if (!is_vaild_output(power)) {
        ESP_LOGI(TAG, "%s invalid power.", __FUNCTION__);
        goto err_exit;
    }

    /* check schedule */
    pSchedule = cJSON_GetObjectItem(request, "schedule");
    count     = cJSON_GetArraySize(pSchedule);
    if (!cJSON_IsArray(pSchedule)) {
        ESP_LOGI(TAG, "%s invalid param schedule.", __FUNCTION__);
        goto err_exit;
    }  else if (count < 2 || count > LED_CTRL_SCH_MAX) {
        ESP_LOGI(TAG, "%s invalid schedule length %d, range[%d-%d].", __FUNCTION__, count, 2, LED_CTRL_SCH_MAX);
        goto err_exit;
    } 
    for (index = 0; index < count; index++) {
        pSub = cJSON_GetArrayItem(pSchedule, index);
        if (!get_time_by_json(pSub, "t", &t[index], LED_CTRL_SCH_TIME_MAX)) 
            goto err_exit;
        
        if (!get_output_by_json(pSub, "values", targ))
            goto err_exit;

        for (tIndex = 0; tIndex < index; tIndex++) {
            if (t[index] == t[tIndex]) {
                ESP_LOGI(TAG, "%s invalid schedule, time %d repeat Existing.", __FUNCTION__, t[index]);
                goto err_exit;
            }
        }
    }
    /* update schedule */
    tsk_ledctrl_lock();
    ledctrl_clean_sch();
    for(index = 0; index < count; index++) {
        pSub = cJSON_GetArrayItem(pSchedule, index);
        get_output_by_json(pSub, "values", targ);
        ledctrl_set_sch_item(index, t[index], targ);
    }
    ledctrl_save_sch(pId->valueint, power);
    tsk_ledctrl_unlock();
    return wss_respond_code_ok;
err_exit:
    return wss_respond_code_invalid_param;
}

wss_respond_code_t exec_ota_begin_handle(int client, cJSON *request, cJSON *respone)
{
    cJSON *item;

    item = cJSON_GetObjectItem(request, "img_size");
    if (!cJSON_IsNumber(item)) {
        ESP_LOGI(TAG, "%s invalid img_size.", __FUNCTION__);
        return wss_respond_code_invalid_param;
    } 

    if (true == ota_begin(client, item->valueint)) {
        return wss_respond_code_ok;
    } else {
        return wss_respond_code_exec_failure;
    } 
}

wss_respond_code_t exec_ota_done_handle(int client, cJSON *request, cJSON *respone)
{
    if (true == ota_end(client)) {
        return wss_respond_code_ok;
    } else {
        return wss_respond_code_exec_failure;
    } 
}
