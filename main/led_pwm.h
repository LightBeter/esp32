#ifndef   _LED_PWM_H
#define  _LED_PWM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LED_VALUE_MIN       0
#define LED_VALUE_MAX       100

typedef int8_t  led_value_t;

typedef enum {
    LED_CHAN_WHITE = 0x00,
    LED_CHAN_DARK_BLUE,
    LED_CHAN_BLUE,
    LED_CHAN_RED,
    LED_CHAN_GREEN,
    LED_CHAN_UV_405,
    LED_CHAN_UV_415,
    LED_CHAN_COUNT,
} led_chan_t;

typedef struct {
    uint32_t   min;
    uint32_t   max;
    uint32_t   range;
} led_output_cfg_t;

typedef struct {
    uint32_t   cur;
    uint32_t   targ;
} led_chan_state_t;

void led_pwm_initialise();
void led_pwm_output(led_chan_t ch, led_value_t percent);

#ifdef __cplusplus
}
#endif
#endif