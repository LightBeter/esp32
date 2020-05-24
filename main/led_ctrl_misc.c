#include <string.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include "led_ctrl_misc.h"
#include "led_pwm.h"

#define TAG                    "LedCtrl"
#define TSK_NAME               TAG

bool led_ctrl_read_time(const cJSON *packet, const char *name, uint16_t *value, uint16_t max) 
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

bool led_ctrl_read_targ(const cJSON *packet, const char *name, led_value_t *targ) 
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

void led_ctrl_write_time(cJSON *packet, const char *name, uint16_t value)
{
    cJSON_AddNumberToObject(packet, name, value);
}

void led_ctrl_write_targ(cJSON *packet, const char *name, const led_value_t *targ)
{
    int values[LED_CHAN_COUNT];
    cJSON *array;

    for (int i = 0; i < LED_CHAN_COUNT; i++) {
       values[i] =  targ[i];
    }
    array = cJSON_CreateIntArray(values, LED_CHAN_COUNT);
    cJSON_AddItemToObject(packet, name, array);
}

void led_ctrl_read_config(led_ctrl_mode_t *mode, led_value_t *manual, led_ctrl_sch_item_t *schedule)
{
    size_t  ret;
    FILE* file;
    int index, chl;

    file = fopen("/spiffs/config.json", "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for read");
        goto default_handle;
    }
    /* mode */
    ret = fread( mode, sizeof(led_ctrl_mode_t), 1, file);
    if (ret != 1 || !is_vaild_mode(*mode)) {
        ESP_LOGE(TAG, "Failed to read config param mode");
        goto default_handle;
    }
    /* manual output */
    ret = fread( manual, sizeof(led_value_t), LED_CHAN_COUNT, file);
    if (ret != LED_CHAN_COUNT) {
        ESP_LOGE(TAG, "Failed to read config param manual");
        goto default_handle;
    }
    for (index = 0; index < LED_CHAN_COUNT; index++) {
        if (!is_vaild_output(manual[index]))
            goto default_handle;
    }
    /* schedule */
    ret = fread( schedule, sizeof(led_ctrl_sch_item_t), LED_CTRL_SCH_MAX, file);
    if (ret != LED_CTRL_SCH_MAX) {
        ESP_LOGE(TAG, "Failed to read config param schedule");
        goto default_handle;
    }
    for (index = 0; index < LED_CTRL_SCH_MAX; index++) {
        if (!schedule[index].valid)
            continue;
        if (!is_vaild_sch_time(schedule[index].time))
            goto default_handle;
        for (chl = 0; chl < LED_CHAN_COUNT; chl ++)
            if (!is_vaild_output(schedule[index].targ[chl]))
                goto default_handle;
    }
    fclose(file);
    return;
default_handle:
    /* default manual */
    memset(manual, 50, sizeof(led_value_t) * LED_CHAN_COUNT);
    /* default schedule */
    memset(schedule, 0x00, sizeof(led_ctrl_sch_item_t) * LED_CTRL_SCH_MAX);
    schedule[0].valid = true;
    schedule[0].time  = 6 * 60;
    memset(schedule[0].targ, 0, sizeof(led_value_t) * LED_CHAN_COUNT);
    schedule[1].valid = true;
    schedule[1].time  = 12 * 60;
    memset(schedule[1].targ, 100, sizeof(led_value_t)  * LED_CHAN_COUNT);
    schedule[2].valid = true;
    schedule[2].time  = 15 * 60;
    memset(schedule[2].targ, 100, sizeof(led_value_t)  * LED_CHAN_COUNT);
    schedule[3].valid = true;
    schedule[3].time  = 19 * 60;
    memset(schedule[3].targ, 0, sizeof(led_value_t)  * LED_CHAN_COUNT);
    /* default mode */
    *mode = LED_CTRL_MODE_MANUAL;
    if (file) fclose(file);
    return;
}

void led_ctrl_write_config(led_ctrl_mode_t mode, const led_value_t *manual, const led_ctrl_sch_item_t *schedule)
{
    size_t  ret;
    FILE* file;

    file = fopen("/spiffs/config.json", "wb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for write");
        goto exit;
    }
    /* mode */
    ret = fwrite( &mode, sizeof(led_ctrl_mode_t), 1, file);
    if (ret != 1) {
        ESP_LOGE(TAG, "Failed to write config param mode");
        goto exit;
    }
    /* manual output */
    ret = fwrite( manual, sizeof(led_value_t), LED_CHAN_COUNT, file);
    if (ret != LED_CHAN_COUNT) {
        ESP_LOGE(TAG, "Failed to write config param manual");
        goto exit;
    }
    /* schedule */
    ret = fwrite( schedule, sizeof(led_ctrl_sch_item_t), LED_CTRL_SCH_MAX, file);
    if (ret != LED_CTRL_SCH_MAX) {
        ESP_LOGE(TAG, "Failed to write config param schedule");
        goto exit;
    }
exit:    
    fflush(file);
    fclose(file);
    return;
}