#ifndef _LED_CTRL_TSK_H
#define _LED_CTRL_TSK_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <cJSON.h>
#include "led_pwm.h"

#define LED_CTRL_SCH_MAX        48
#define LED_CTRL_SCH_TIME_MAX   (24 * 60 -1)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum led_ctrl_mode_tag {
    LED_CTRL_MODE_MANUAL = 0,
    LED_CTRL_MODE_SCHEDULE,
    LED_CTRL_MODE_ALL_OFF,
    LED_CTRL_MODE_ALL_ON,
    LED_CTRL_MODE_COUNT
} led_ctrl_mode_t;

typedef uint8_t  led_value_t;

typedef struct led_ctrl_sch_item_tag {
    bool        valid;
    led_value_t targ[LED_CHAN_COUNT];
    uint16_t    time; 
} led_ctrl_sch_item_t;

typedef struct led_ctrl_demo_tag {
    uint32_t    remaining;
    led_value_t targ[LED_CHAN_COUNT];
} led_ctrl_demo_t;

typedef struct led_ctrl_tsk_tag {
    xTaskHandle       handle;
    SemaphoreHandle_t mutex;
    led_ctrl_mode_t   mode;
    led_value_t       output[LED_CHAN_COUNT];
    uint32_t          curSaveToken;
    uint32_t          preSaveToken;
} led_ctrl_tsk_t;

void led_ctrl_tsk_init();
void led_ctrl_tsk_deinit();
void led_ctrl_tsk_start();
void led_ctrl_tsk_stop();

void led_ctrl_get_mode(cJSON *packet);
bool led_ctrl_set_mode(const cJSON *packet);
void led_ctrl_get_current_output(cJSON *packet);
bool led_ctrl_set_demo(const cJSON *packet);
void led_ctrl_get_manual(cJSON *packet);
bool led_ctrl_set_manual(const cJSON *packet);
void led_ctrl_get_schedule(cJSON *packet);
bool led_ctrl_set_schedule(const cJSON *packet);

#ifdef __cplusplus
}
#endif
#endif