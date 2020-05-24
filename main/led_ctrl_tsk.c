#include <stdio.h>
#include <string.h>
#include <time.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include "led_ctrl_tsk.h"
#include "led_ctrl_misc.h"

#define TAG                    "LedCtrl"
#define TSK_NAME               TAG
#define TSK_STACK_SIZE         4096

void led_ctrl_lock();
void led_ctrl_unlock();
void led_ctrl_get_by_sch();
void led_ctrl_tsk(void * parm);

led_ctrl_tsk_t      _led_ctrl_tsk;
led_ctrl_demo_t     _led_ctrl_demo;
led_value_t         _led_ctrl_manual[LED_CHAN_COUNT];
led_ctrl_sch_item_t _led_ctrl_schedule[LED_CTRL_SCH_MAX];

void led_ctrl_tsk_init()
{
    /* initialise pwm */
    led_pwm_initialise();
    /* initialise task runtime */
    _led_ctrl_tsk.handle = NULL;
    _led_ctrl_tsk.mutex  = xSemaphoreCreateMutex();
    _led_ctrl_tsk.mode   = LED_CTRL_MODE_SCHEDULE;
    _led_ctrl_tsk.preSaveToken = 0;
    _led_ctrl_tsk.curSaveToken = 0;
    memset(&_led_ctrl_demo, 0x00, sizeof(led_ctrl_demo_t));
    /* load config  */
    led_ctrl_read_config(&_led_ctrl_tsk.mode,
                         _led_ctrl_manual,
                         _led_ctrl_schedule);
}

void led_ctrl_tsk_deinit()
{
    led_ctrl_write_config(_led_ctrl_tsk.mode,
                          _led_ctrl_manual,
                          _led_ctrl_schedule);
    vSemaphoreDelete(_led_ctrl_tsk.mutex);
}

void led_ctrl_tsk_start()
{
    if (!_led_ctrl_tsk.handle) {
        xTaskCreate(led_ctrl_tsk, TSK_NAME, TSK_STACK_SIZE, NULL, 3, &_led_ctrl_tsk.handle);
    }
}

void led_ctrl_tsk_stop()
{
    if (_led_ctrl_tsk.handle) {
        vTaskDelete(_led_ctrl_tsk.handle);
        _led_ctrl_tsk.handle = NULL; 
    }
    /* close all led */
    for (int index = 0; index < LED_CHAN_COUNT; index++) {
        led_pwm_output(index, 0) ;
    }
}

void led_ctrl_lock()
{
    while( xSemaphoreTake(_led_ctrl_tsk.mutex, portMAX_DELAY) != pdTRUE );
}

void led_ctrl_unlock()
{
    while( xSemaphoreGive(_led_ctrl_tsk.mutex) != pdTRUE );
}

void led_ctrl_get_mode(cJSON *packet)
{
    cJSON_AddNumberToObject(packet, "mode", _led_ctrl_tsk.mode);
}

bool led_ctrl_set_mode(const cJSON *packet)
{
    cJSON *pItem = cJSON_GetObjectItem(packet, "mode");
     if (!cJSON_IsNumber(pItem)) {
        ESP_LOGI(TAG, "%s invalid param mode.", __FUNCTION__);
        return false;
    } else if (!is_vaild_mode(pItem->valueint)) {
        ESP_LOGI(TAG, "%s invalid param mode value %d.", __FUNCTION__, pItem->valueint );
        return false;  
    } 
    led_ctrl_lock();
    _led_ctrl_tsk.mode = pItem->valueint;
    _led_ctrl_tsk.curSaveToken++;
    led_ctrl_unlock();
    return true;
}

void led_ctrl_get_current_output(cJSON *packet)
{
    led_ctrl_lock();
    led_ctrl_write_targ(packet, "values", _led_ctrl_tsk.output);
    led_ctrl_unlock();    
}

bool led_ctrl_set_demo(const cJSON *packet) 
{
    uint16_t    lifetime = 0;
    led_value_t targ[LED_CHAN_COUNT];
    /* lifetime */
    if (true != led_ctrl_read_time(packet, "lifetime", &lifetime, 180)) {
        return false;
    }
    /* channel values */
    if (true != led_ctrl_read_targ( packet, "values", targ)) {
        return false;
    } 
    led_ctrl_lock();
    _led_ctrl_demo.remaining = lifetime;
    memcpy(_led_ctrl_demo.targ, targ, sizeof(led_value_t) * LED_CHAN_COUNT);
    led_ctrl_unlock();
    return true;
}

void led_ctrl_get_manual(cJSON *packet)
{
    led_ctrl_lock();
    led_ctrl_write_targ(packet, "values", _led_ctrl_manual);
    led_ctrl_unlock();
}

bool led_ctrl_set_manual(const cJSON *packet)
{
    led_value_t targ[LED_CHAN_COUNT];
    /* channel values */
    if (true != led_ctrl_read_targ( packet, "values", targ)) {
        return false;
    } 
    led_ctrl_lock();
    memcpy(_led_ctrl_manual, targ, sizeof(led_value_t) * LED_CHAN_COUNT);
    _led_ctrl_demo.remaining = 0;
    _led_ctrl_tsk.curSaveToken++;
    led_ctrl_unlock();
    return true;
}

void led_ctrl_get_schedule(cJSON *packet)
{
    cJSON *pSchedule = cJSON_CreateArray();
    cJSON *pSub;

    led_ctrl_lock();
    for(int index = 0; index < LED_CTRL_SCH_MAX; index++) {
        if (!_led_ctrl_schedule[index].valid)
            continue;
        pSub = cJSON_CreateObject();
        led_ctrl_write_time(pSub, "t", _led_ctrl_schedule[index].time);
        led_ctrl_write_targ(pSub, "values", _led_ctrl_schedule[index].targ);
        cJSON_AddItemToArray(pSchedule, pSub);
    }
    led_ctrl_unlock();
    cJSON_AddItemToObject(packet, "schedule", pSchedule);
}

bool led_ctrl_set_schedule(const cJSON *packet)
{
    cJSON *pSchedule, *pSub;
    led_value_t targ[LED_CHAN_COUNT];
    uint16_t    t[LED_CTRL_SCH_MAX];
    int index, tIndex, count;
    /* check schedule */
    pSchedule = cJSON_GetObjectItem(packet, "schedule");
    count     = cJSON_GetArraySize(pSchedule);
    if (!cJSON_IsArray(pSchedule)) {
        ESP_LOGI(TAG, "%s invalid param schedule.", __FUNCTION__);
        return false; 
    }  else if (count < 2 || count > LED_CTRL_SCH_MAX) {
        ESP_LOGI(TAG, "%s invalid schedule length %d, range[%d-%d].", 
                 __FUNCTION__, count, 2, LED_CTRL_SCH_MAX);
        return false; 
    } 
    for (index = 0; index < count; index++) {
        pSub = cJSON_GetArrayItem(pSchedule, index);
        if (true != led_ctrl_read_time(pSub, "t", &t[index], LED_CTRL_SCH_TIME_MAX)) 
            return false;
        if (true != led_ctrl_read_targ(pSub, "values", targ))
            return false;
        for (tIndex = 0; tIndex < index; tIndex++) {
            if (t[index] == t[tIndex]) {
                ESP_LOGI(TAG, "%s invalid schedule, time %d repeat Existing.", 
                         __FUNCTION__, t[index]);
                return false;
            }
        }
    }
    /* update schedule */
    led_ctrl_lock();
    for(index = 0; index < LED_CTRL_SCH_MAX; index++) {
        if (index < count) {
            pSub = cJSON_GetArrayItem(pSchedule, index);
            led_ctrl_read_time(pSub, "t", &_led_ctrl_schedule[index].time, LED_CTRL_SCH_TIME_MAX);
            led_ctrl_read_targ(pSub, "values", _led_ctrl_schedule[index].targ);
            _led_ctrl_schedule[index].valid = true;
        } else {
            _led_ctrl_schedule[index].valid = false;
        }
    }
    _led_ctrl_demo.remaining = 0;
    _led_ctrl_tsk.curSaveToken++;
    led_ctrl_unlock();
    return true;
}

void led_ctrl_tsk(void * parm)
{
    led_chan_t index = 0;
    while (1) {
        led_ctrl_lock();
        /* save new config */
        if (_led_ctrl_tsk.preSaveToken != _led_ctrl_tsk.curSaveToken) {
            led_ctrl_write_config(_led_ctrl_tsk.mode,
                          _led_ctrl_manual,
                          _led_ctrl_schedule);
            _led_ctrl_tsk.preSaveToken = _led_ctrl_tsk.curSaveToken;
        }
        if (_led_ctrl_demo.remaining > 0) {
            /* demo */
            _led_ctrl_demo.remaining -= 1;
            for (index = 0; index < LED_CHAN_COUNT; index++)
                _led_ctrl_tsk.output[index] = _led_ctrl_demo.targ[index];
        } else if(LED_CTRL_MODE_MANUAL == _led_ctrl_tsk.mode) {
            /* manual */
            for (index = 0; index < LED_CHAN_COUNT; index++)
                _led_ctrl_tsk.output[index] = _led_ctrl_manual[index];
        } else if(LED_CTRL_MODE_ALL_OFF == _led_ctrl_tsk.mode) {
            /* all off */
            memset(_led_ctrl_tsk.output, 0, sizeof(led_value_t)  * LED_CHAN_COUNT); 
        } else if (LED_CTRL_MODE_ALL_ON == _led_ctrl_tsk.mode) {
            /* all in */
            memset(_led_ctrl_tsk.output, 100, sizeof(led_value_t)  * LED_CHAN_COUNT); 
        } else {   
            /* schedule */
            led_ctrl_get_by_sch();
        }
        led_ctrl_unlock();
       
        for (index = 0; index < LED_CHAN_COUNT; index++)
            led_pwm_output(index, _led_ctrl_tsk.output[index]) ;
        vTaskDelay(pdMS_TO_TICKS(1000 * 1));
    }
}

void led_ctrl_get_by_sch()
{
}