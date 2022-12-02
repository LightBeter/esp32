#ifndef _TSK_LEDCTRL_MISC_H
#define _TSK_LEDCTRL_MISC_H

#include "led_pwm.h"

#define LED_CTRL_SCH_MAX            64
#define LED_CTRL_SCH_TIME_MAX       (24 * 60 -1)
#define LED_CTRL_TSK_EVENT_UPDATE   BIT0

#define is_vaild_mode(_mode_)     (_mode_>= 0  && _mode_ < LED_CTRL_MODE_COUNT)
#define is_vaild_output(_val_)    (_val_ >= 0 && _val_ <= LED_VALUE_MAX)
#define is_vaild_sch_time(_t_)    (_t_ >= 0 && _t_ <= LED_CTRL_SCH_TIME_MAX)

typedef enum ledctrl_mode_tag {
    LED_CTRL_MODE_MANUAL = 0,
    LED_CTRL_MODE_SCHEDULE,
    LED_CTRL_MODE_COUNT
} ledctrl_mode_t;

typedef struct led_sch_item_tag {
    bool        valid;
    led_value_t targ[LED_CHAN_COUNT];
    uint16_t    time; 
} led_sch_item_t;

typedef int led_sch_id_t;

typedef struct ledctrl_demo_tag {
    uint32_t    remaining;
    led_value_t targ[LED_CHAN_COUNT];
} ledctrl_demo_t;

#ifdef __cplusplus
extern "C" {
#endif

bool ledctrl_switch();
bool ledctrl_is_close();
ledctrl_mode_t ledctrl_mode();
led_value_t ledctrl_power();
const led_value_t* ledctrl_current_output();
const led_value_t* ledctrl_manual_output();
led_sch_id_t ledctrl_sch_id();
const led_sch_item_t* ledctrl_sch_item(uint8_t index);

bool ledctrl_set_mode(bool sw, ledctrl_mode_t mode);
bool ledctrl_set_demo(uint16_t lifetime, const led_value_t *values);
bool ledctrl_set_manual(const led_value_t *values);
void ledctrl_set_sch_item(uint8_t index, uint16_t time, led_value_t *values);
void ledctrl_clean_sch();
void ledctrl_save_sch(led_sch_id_t id, led_value_t power);
int16_t ledctrl_current_time();
bool ledctrl_check_values(const led_value_t *values);
void ledctrl_read_config();
void ledctrl_write_config();

#ifdef __cplusplus
}
#endif
#endif