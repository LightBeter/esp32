#include <stdio.h>
#include <string.h>
#include <time.h>
//#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "tsk_ledctrl.h"
#include "tsk_ledctrl_misc.h"
#include "tsk_fanctrl.h"
#include "misc.h"

#define TAG                         "LedCtrl"
#define TSK_NAME                    TAG
#define TSK_STACK_SIZE              (5 * 1024)
#define SAVE_DELAY_S                (3)

void led_ctrl_get_by_sch();
void led_ctrl_save_config();
void led_ctrl_tsk(void * parm);
void led_ctrl_boot_play();

static xTaskHandle         tsk_handle = NULL; 
static SemaphoreHandle_t   mutex = NULL;
EventGroupHandle_t  led_event_group;
led_value_t         led_output[LED_CHAN_COUNT];
bool                led_sw = false;  
ledctrl_mode_t      led_mode = LED_CTRL_MODE_SCHEDULE;
led_value_t         led_power = 0;
ledctrl_demo_t      led_demo = { .remaining = -1, .targ = {0} };
led_value_t         led_manual[LED_CHAN_COUNT];
led_sch_item_t      led_schedule[LED_CTRL_SCH_MAX];
led_sch_id_t        led_schedule_Id = 0;
uint32_t            led_curSaveToken = 0;
uint32_t            led_preSaveToken = 0;


void tsk_ledctrl_init()
{
    /* initialise pwm */
    led_pwm_initialise();
    /* initialise task runtime */
    mutex  = xSemaphoreCreateMutex();
    led_event_group= xEventGroupCreate();
    memset(&led_demo, 0x00, sizeof(ledctrl_demo_t));
    /* load led ctrl config  */
    ledctrl_read_config();
    indicator_set_chan(LED_CTRL_MODE_MANUAL == led_mode ? INDICATOR_CHAN_MANUAL : INDICATOR_CHAN_AUTO);
}

void tsk_ledctrl_deinit()
{
    led_ctrl_save_config();
    vEventGroupDelete(led_event_group);
    vSemaphoreDelete(mutex);
    led_pwm_deinitialise();
}

void tsk_ledctrl_start()
{
    if (!tsk_handle)
        xTaskCreatePinnedToCore(led_ctrl_tsk, TSK_NAME, TSK_STACK_SIZE, NULL, TSK_PRIO_LEDCTRL, &tsk_handle, TSK_COREID_LEDCTRL);
}

void tsk_ledctrl_stop()
{
    if (tsk_handle) {
        vTaskDelete(tsk_handle);
        tsk_handle = NULL; 
    }
    /* close all led */
    for (int index = 0; index < LED_CHAN_COUNT; index++)
        led_pwm_output(index, 0);
}

void tsk_ledctrl_lock()
{
    while( xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE );
}

void tsk_ledctrl_unlock()
{
    while( xSemaphoreGive(mutex) != pdTRUE );
}

void led_ctrl_save_config()
{
    if (led_preSaveToken != led_curSaveToken) {
        ledctrl_write_config();
        led_preSaveToken = led_curSaveToken;
    }
}

void led_ctrl_boot_play()
{    
    uint8_t chan = 0;
    for (chan = 0 ; chan < LED_CHAN_COUNT; chan++) { 
#if defined(CONFIG_DEVICE_MODEL_LM12) && CONFIG_DEVICE_MODEL_LM12
        if (LED_CHAN_GREEN == chan)
            continue;
#endif
        led_pwm_output_play(chan, 50, 1);
        vTaskDelay(pdMS_TO_TICKS(700));
        led_pwm_output_play(chan, 0, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    for (chan = 0 ; chan < LED_CHAN_COUNT; chan++) { 
        led_pwm_output_play(chan, 50, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(700));

    for (chan = 0 ; chan < LED_CHAN_COUNT; chan++) { 
        led_pwm_output_play(chan, 0, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}

void led_ctrl_tsk(void * parm)
{
    const EventBits_t uxBitsToWaitFor = LED_CTRL_TSK_EVENT_UPDATE;
    EventBits_t events = 0;
    led_chan_t index = 0;
    bool indicator_state = false;
    int8_t tick = SAVE_DELAY_S;

    led_ctrl_boot_play();
    while (1) 
    {
        tsk_ledctrl_lock();
        /* save new config */
        if (0 == tick) {
            led_ctrl_save_config();
        }
        /* update output */
        if (false == led_sw || tempctrl_is_over()) { 
            for (index = 0; index < LED_CHAN_COUNT; index++)
                led_output[index] = LED_VALUE_MIN; 
        } else if (led_demo.remaining > 0) {
            led_demo.remaining -= 1;
            for (index = 0; index < LED_CHAN_COUNT; index++)
                led_output[index] = led_demo.targ[index];
        } else {
            if(LED_CTRL_MODE_MANUAL == led_mode) {
                for (index = 0; index < LED_CHAN_COUNT; index++)
                    led_output[index] = led_manual[index];
            } else {   
                led_ctrl_get_by_sch();
            }
        }
        indicator_state = led_sw;
        tsk_ledctrl_unlock();

        for (index = 0; index < LED_CHAN_COUNT; index++)
            led_pwm_output(index, led_output[index]);

        indicator_set_state(indicator_state);
        indicator_set_chan(LED_CTRL_MODE_MANUAL == led_mode ? INDICATOR_CHAN_MANUAL : INDICATOR_CHAN_AUTO);

        events = xEventGroupWaitBits(led_event_group, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000)); 
        if (0 == events) {
            tick = (tick > 0) ? (tick  - 1) : (-1);
        } else {
            tick = SAVE_DELAY_S;
        }
    }
}

void led_ctrl_get_by_sch()
{
    int16_t now, diff, leftDiff = 0, rightDiff = 0; 
    int16_t  index, left = -1, right = -1;
    led_value_t offset = 0;

    now = ledctrl_current_time();
    for (index = 0 ; index < LED_CTRL_SCH_MAX; index++) {
        if (false == led_schedule[index].valid) 
            continue;
        /* left */
        if (now >= led_schedule[index].time) {
            diff = now - led_schedule[index].time;
        } else {
            diff = LED_CTRL_SCH_TIME_MAX - (led_schedule[index].time - now);
        }
        if (diff < leftDiff || left == -1) {
            left = index;
            leftDiff = diff;
        }
        /* right */
        if (led_schedule[index].time >= now) {
            diff = led_schedule[index].time - now;
        } else {
            diff = LED_CTRL_SCH_TIME_MAX - (now - led_schedule[index].time);
        }
        if (diff < rightDiff || right == -1) {
            right = index;
            rightDiff = diff;
        }
    }

    if (left == -1 || right == -1) {
        for (index = 0; index < LED_CHAN_COUNT; index++)
            led_output[index] = 0; 
    } else if (leftDiff == 0) {
        for (index = 0; index < LED_CHAN_COUNT; index++)
            led_output[index] = led_schedule[left].targ[index];  
    } else if (rightDiff == 0) {
        for (index = 0; index < LED_CHAN_COUNT; index++)
            led_output[index] = led_schedule[right].targ[index];  
    } else {
        if (led_schedule[right].time > led_schedule[left].time) {
            diff = led_schedule[right].time - led_schedule[left].time;
        } else {
            diff = LED_CTRL_SCH_TIME_MAX - (led_schedule[left].time - led_schedule[right].time);
        }

        for (index = 0; index < LED_CHAN_COUNT; index++) {
            offset = led_schedule[right].targ[index] - led_schedule[left].targ[index];
            offset = (leftDiff * offset) / diff;
            led_output[index] = led_schedule[left].targ[index] + offset; 
        }
    }
    /* Power Limit */
    for (index = 0; index < LED_CHAN_COUNT; index++) {
        led_output[index] = (led_output[index] * led_power) / 1000; 
    }
    
#if 0
        ESP_LOGI(TAG, "left [%8u] output: %04d, %04d, %04d, %04d, %04d, %04d, %04d", 
            led_schedule[left].time, led_schedule[left].targ[0], 
            led_schedule[left].targ[1], led_schedule[left].targ[2],
            led_schedule[left].targ[3], led_schedule[left].targ[4],
            led_schedule[left].targ[5], led_schedule[left].targ[6]);
        ESP_LOGI(TAG, "right[%8u] output: %04d, %04d, %04d, %04d, %04d, %04d, %04d", 
            led_schedule[right].time, led_schedule[right].targ[0], 
            led_schedule[right].targ[1], led_schedule[right].targ[2],
            led_schedule[right].targ[3], led_schedule[right].targ[4],
            led_schedule[right].targ[5], led_schedule[right].targ[6]);
        ESP_LOGI(TAG, "offset: %d", offset);   
        ESP_LOGI(TAG, "now [%8u] output: %04d, %04d, %04d, %04d, %04d, %04d, %04d", 
            now, led_output[0], 
            led_output[1], led_output[2],
            led_output[3], led_output[4],
            led_output[5], led_output[6]);
#endif
}