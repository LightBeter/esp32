#ifndef _LED_CTRL_MISC_H
#define _LED_CTRL_MISC_H

#include <cJSON.h>
#include "led_ctrl_tsk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define is_vaild_mode(_mode)     (_mode >= 0 && _mode < LED_CTRL_MODE_COUNT)
#define is_vaild_output(_val)    (_val >= 0 && _val <= LED_VALUE_MAX)
#define is_vaild_sch_time(_t)    (_t >= 0 && _t <= LED_CTRL_SCH_TIME_MAX)


bool led_ctrl_read_time(const cJSON *packet, const char *name, uint16_t *value, uint16_t max);
bool led_ctrl_read_targ(const cJSON *packet, const char *name, led_value_t *targ);

void led_ctrl_write_time(cJSON *packet, const char *name, uint16_t value);
void led_ctrl_write_targ(cJSON *packet, const char *name, const led_value_t *targ);

void led_ctrl_read_config(bool *sw, led_ctrl_mode_t *mode, led_value_t *manual, 
                          led_ctrl_sch_id_t *schedule_id, led_value_t *power, 
                          led_ctrl_sch_item_t *schedule);
void led_ctrl_write_config(bool sw, led_ctrl_mode_t mode, const led_value_t *manual, 
                           led_ctrl_sch_id_t schedule_id, led_value_t power, 
                           const led_ctrl_sch_item_t *schedule);

#ifdef __cplusplus
}
#endif
#endif