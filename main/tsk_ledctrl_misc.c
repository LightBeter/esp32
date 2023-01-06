#include <string.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include "tsk_ledctrl_misc.h"
#include "tsk_ledctrl.h"

#define TAG                "LedCtrl"
#define TSK_NAME            TAG

extern EventGroupHandle_t  led_event_group;
extern led_value_t         led_output[LED_CHAN_COUNT];
extern bool                led_sw;  
extern ledctrl_mode_t      led_mode;
extern led_value_t         led_power;
extern ledctrl_demo_t      led_demo;
extern led_value_t         led_manual[LED_CHAN_COUNT];
extern led_sch_item_t      led_schedule[LED_CTRL_SCH_MAX];
extern led_sch_id_t        led_schedule_Id;
extern uint32_t            led_curSaveToken;

bool ledctrl_switch()
{
    return led_sw;
}

bool ledctrl_is_close()
{
    if (false == led_sw)
        return true;

    for (uint8_t index = 0; index < LED_CHAN_COUNT; index++) {
        if (LED_VALUE_MIN != led_output[index])
            return false;
    }
    return true;
}

ledctrl_mode_t ledctrl_mode()
{
    return led_mode;
}

const led_value_t* ledctrl_current_output()
{
    return led_output;
}

const led_value_t* ledctrl_manual_output()
{
    return led_manual;
}

led_value_t ledctrl_power()
{
    return led_power;
}

led_sch_id_t ledctrl_sch_id()
{
    return led_schedule_Id;
}

bool ledctrl_check_values(const led_value_t *values)
{
    for (int index = 0; index < LED_CHAN_COUNT; index++) {
        if (!is_vaild_output(values[index])) {
            ESP_LOGI(TAG, "%s invalid channel %d value %d, range[0-100].", 
                     __FUNCTION__, index, values[index]);
            return false;  
        }
    } 
    return true;
}

const led_sch_item_t* ledctrl_sch_item(uint8_t index)
{
    if (index >= LED_CTRL_SCH_MAX)
        return NULL;

    if (!led_schedule[index].valid)
        return NULL;

    return &led_schedule[index];
}


bool ledctrl_set_mode(bool sw, ledctrl_mode_t mode)
{
    if (!is_vaild_mode(mode))
        return false;

    tsk_ledctrl_lock();
    if (led_sw == sw && led_mode == mode) {
        tsk_ledctrl_unlock(); 
        return true;
    }
    led_sw   = sw;
    led_mode = mode;
    led_demo.remaining = 0;
    led_curSaveToken++;
    tsk_ledctrl_unlock(); 
    xEventGroupSetBits(led_event_group, LED_CTRL_TSK_EVENT_UPDATE);
    return true; 
}

bool ledctrl_set_demo(uint16_t lifetime, const led_value_t *values)
{
    if (lifetime > 180)
        return false;
    
    if (!ledctrl_check_values(values))
        return false;          

    tsk_ledctrl_lock();
    led_demo.remaining = lifetime;
    memcpy(led_demo.targ, values, sizeof(led_value_t) * LED_CHAN_COUNT);
    tsk_ledctrl_unlock(); 
    xEventGroupSetBits(led_event_group, LED_CTRL_TSK_EVENT_UPDATE);
    return true;
}

bool ledctrl_set_manual(const led_value_t *values)
{
    if (!ledctrl_check_values(values))
        return false;
    
    if (0 == memcmp(led_manual, values, sizeof(led_value_t) * LED_CHAN_COUNT)) 
        return true;

    tsk_ledctrl_lock();
    memcpy(led_manual, values, sizeof(led_value_t) * LED_CHAN_COUNT);
    led_demo.remaining = 0;
    led_curSaveToken++;
    tsk_ledctrl_unlock();
    xEventGroupSetBits(led_event_group, LED_CTRL_TSK_EVENT_UPDATE);
    return true;
}

void ledctrl_set_sch_item(uint8_t index, uint16_t time, led_value_t *values)
{
    memcpy(led_schedule[index].targ, values, sizeof(led_value_t) * LED_CHAN_COUNT);
    led_schedule[index].time = time;
    led_schedule[index].valid = true;    
}

void ledctrl_clean_sch()
{
    memset(led_schedule, 0x00, sizeof(led_sch_item_t) * LED_CTRL_SCH_MAX);
}

void ledctrl_save_sch(led_sch_id_t id, led_value_t power)
{
    led_power = power;
    led_schedule_Id = id;
    led_curSaveToken++;   
    led_demo.remaining = 0;
}

int16_t ledctrl_current_time()
{
    time_t t;
    struct tm* stime;
    t = time(NULL);
    stime = localtime(&t);
    return stime->tm_hour * 60 + stime->tm_min;
}

void ledctrl_read_config()
{
    size_t  ret;
    FILE* file;
    int index, chl;

    file = fopen("/spiffs/config.json", "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for read");
        goto default_handle;
    }
    /* led switch */
    ret = fread(&led_sw, sizeof(bool), 1, file);
    if (ret != 1) {
        ESP_LOGW(TAG, "Failed to read config param led switch");
        goto default_handle;
    }
    /* mode */
    ret = fread(&led_mode, sizeof(ledctrl_mode_t), 1, file);
    if (ret != 1 || !is_vaild_mode(led_mode)) {
        ESP_LOGE(TAG, "Failed to read config param mode");
        goto default_handle;
    }
    /* manual output */
    ret = fread(led_manual, sizeof(led_value_t), LED_CHAN_COUNT, file);
    if (ret != LED_CHAN_COUNT) {
        ESP_LOGE(TAG, "Failed to read config param manual");
        goto default_handle;
    }

    for (index = 0; index < LED_CHAN_COUNT; index++) {
        if (!is_vaild_output(led_manual[index]))
            goto default_handle;
    }
    /* schedule */
    ret = fread(&led_schedule_Id, sizeof(led_sch_id_t), 1, file);
    if (ret != 1) {
        ESP_LOGE(TAG, "Failed to read config param schedule id");
        goto default_handle;
    }

    ret = fread(led_schedule, sizeof(led_sch_item_t), LED_CTRL_SCH_MAX, file);
    if (ret != LED_CTRL_SCH_MAX) {
        ESP_LOGE(TAG, "Failed to read config param schedule");
        goto default_handle;
    }

    for (index = 0; index < LED_CTRL_SCH_MAX; index++) {
        if (!led_schedule[index].valid)
            continue;

        if (!is_vaild_sch_time(led_schedule[index].time))
            goto default_handle;

        for (chl = 0; chl < LED_CHAN_COUNT; chl ++)
            if (!is_vaild_output(led_schedule[index].targ[chl]))
                goto default_handle;
    }

    ret = fread(&led_power, sizeof(led_value_t), 1, file);
    if (ret != 1 || !is_vaild_output(led_power)) {
        led_power = LED_VALUE_MAX;
    }

    fclose(file);
    return;

default_handle:
    /* default manual */
    for (index = 0; index < LED_CHAN_COUNT; index++)
        led_manual[index] = (LED_VALUE_MAX * 3) / 10;

    /* default schedule */
    ledctrl_clean_sch();
    led_schedule[0].valid = true;
    led_schedule[0].time  = 6 * 60;
    for (index = 0; index < LED_CHAN_COUNT; index++) 
        led_schedule[0].targ[index] = LED_VALUE_MIN;

    led_schedule[1].valid = true;
    led_schedule[1].time  = 12 * 60;
    for (index = 0; index < LED_CHAN_COUNT; index++) 
        led_schedule[1].targ[index] = LED_VALUE_MAX / 2;

    led_schedule[2].valid = true;
    led_schedule[2].time  = 15 * 60;
    for (index = 0; index < LED_CHAN_COUNT; index++) 
        led_schedule[2].targ[index] = LED_VALUE_MAX / 2;

    led_schedule[3].valid = true;
    led_schedule[3].time  = 19 * 60;
    for (index = 0; index < LED_CHAN_COUNT; index++) 
        led_schedule[3].targ[index] = LED_VALUE_MIN;
    /* default mode */
    led_mode = LED_CTRL_MODE_SCHEDULE;
    led_sw = false;
    led_schedule_Id = 0;
    led_power = LED_VALUE_MAX / 2;

    if (file) 
        fclose(file);
    ledctrl_write_config();        
    return;
}

void ledctrl_write_config()
{
    size_t  ret;
    FILE* file;

    file = fopen("/spiffs/config.json", "wb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for write");
        goto exit;
    }
    /* led switch */
    ret = fwrite(&led_sw, sizeof(bool), 1, file);
    if (ret != 1) {
        ESP_LOGE(TAG, "Failed to write led switch");
        goto exit;
    }
    /* mode */
    ret = fwrite(&led_mode, sizeof(ledctrl_mode_t), 1, file);
    if (ret != 1) {
        ESP_LOGE(TAG, "Failed to write config param mode");
        goto exit;
    }
    /* manual output */
    ret = fwrite(led_manual, sizeof(led_value_t), LED_CHAN_COUNT, file);
    if (ret != LED_CHAN_COUNT) {
        ESP_LOGE(TAG, "Failed to write config param manual");
        goto exit;
    }
    /* schedule */
    ret = fwrite(&led_schedule_Id, sizeof(led_sch_id_t), 1, file);
    if (ret != 1) {
        ESP_LOGE(TAG, "Failed to write config param schedule id");
        goto exit;
    }
    
    ret = fwrite(led_schedule, sizeof(led_sch_item_t), LED_CTRL_SCH_MAX, file);
    if (ret != LED_CTRL_SCH_MAX) {
        ESP_LOGE(TAG, "Failed to write config param schedule");
        goto exit;
    }
    
    ret = fwrite(&led_power, sizeof(led_value_t), 1, file);
    if (ret != 1) {
        ESP_LOGE(TAG, "Failed to write config param schedule");
        goto exit;
    }
exit:    
    fflush(file);
    fclose(file);
    return;
}