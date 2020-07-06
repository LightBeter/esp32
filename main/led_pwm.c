#include <stdio.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include "led_pwm.h"

#define TAG                         "LedPwm"
#define LED_PWM_FREQ                5000
#define LED_PWM_SPEED_MODE          LEDC_HIGH_SPEED_MODE
#define LED_PWM_TIMER               LEDC_TIMER_0
#define LED_PWM_DUTY_RESOLUTION     LEDC_TIMER_8_BIT
#define LED_PWM_DUTY_MAX            ((0x1 << LED_PWM_DUTY_RESOLUTION) - 1)
#define LED_PWM_FADE_ENABLE         1
#define LED_PWM_FADE_TIME           (800)

#define LED_CURRENT_MAX             (3000)
#define LED_CURRENT_2_DUTY(_c_)     (_c_ * LED_PWM_DUTY_MAX / LED_CURRENT_MAX)

#define LED_CHANNEL_CFG(_chl, _gpio_num)     { \
    .gpio_num   = _gpio_num,               \
    .channel    = _chl,                    \
    .speed_mode = LED_PWM_SPEED_MODE,      \
    .intr_type  = LEDC_INTR_DISABLE,       \
    .timer_sel  = LED_PWM_TIMER,           \
    .duty       = 0,                       \
    .hpoint     = 0,                       \
}

const led_output_cfg_t led_output_cfg[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = { LED_CURRENT_2_DUTY(100),  LED_CURRENT_2_DUTY(1300) },
    [LED_CHAN_DARK_BLUE] = { LED_CURRENT_2_DUTY(100),  LED_CURRENT_2_DUTY(1000) }, 
    [LED_CHAN_BLUE]      = { LED_CURRENT_2_DUTY(100),  LED_CURRENT_2_DUTY(1000) }, 
    [LED_CHAN_RED]       = { LED_CURRENT_2_DUTY(100),  LED_CURRENT_2_DUTY(600) },    
    [LED_CHAN_GREEN]     = { LED_CURRENT_2_DUTY(100),  LED_CURRENT_2_DUTY(600) },
    [LED_CHAN_UV_405]    = { LED_CURRENT_2_DUTY(100),  LED_CURRENT_2_DUTY(700) },
    [LED_CHAN_UV_415]    = { LED_CURRENT_2_DUTY(100),  LED_CURRENT_2_DUTY(700) },
};

ledc_channel_config_t led_pwm_channel[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = LED_CHANNEL_CFG(LEDC_CHANNEL_0, GPIO_NUM_32),
    [LED_CHAN_DARK_BLUE] = LED_CHANNEL_CFG(LEDC_CHANNEL_1, GPIO_NUM_33),
    [LED_CHAN_BLUE]      = LED_CHANNEL_CFG(LEDC_CHANNEL_2, GPIO_NUM_25),
    [LED_CHAN_RED]       = LED_CHANNEL_CFG(LEDC_CHANNEL_3, GPIO_NUM_26),
    [LED_CHAN_GREEN]     = LED_CHANNEL_CFG(LEDC_CHANNEL_4, GPIO_NUM_27),
    [LED_CHAN_UV_405]    = LED_CHANNEL_CFG(LEDC_CHANNEL_5, GPIO_NUM_14),
    [LED_CHAN_UV_415]    = LED_CHANNEL_CFG(LEDC_CHANNEL_6, GPIO_NUM_12),
};

void led_pwm_initialise()
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LED_PWM_DUTY_RESOLUTION, 
        .freq_hz    = LED_PWM_FREQ,    
        .speed_mode = LED_PWM_SPEED_MODE,
        .timer_num  = LED_PWM_TIMER
    };
    ESP_ERROR_CHECK( ledc_timer_config(&ledc_timer) );
#if defined(LED_PWM_FADE_ENABLE) && (LED_PWM_FADE_ENABLE == 1)
    ESP_ERROR_CHECK( ledc_fade_func_install(0) );
#endif
    for(int i = 0; i < LED_CHAN_COUNT; i++) {
        ESP_ERROR_CHECK( ledc_channel_config(&led_pwm_channel[i]) );
    }
}

void led_pwm_output(led_chan_t ch, led_value_t percent)
{
    uint32_t targ;
    if (percent < LED_VALUE_MIN) {
        percent = LED_VALUE_MIN;
    } else if (percent > LED_VALUE_MAX) {
        percent = LED_VALUE_MAX;
    } 

    if (percent < 1) {
       targ = 0; 
    } else {
       targ = (led_output_cfg[ch].max - led_output_cfg[ch].min)  * percent / LED_VALUE_MAX;
    }
#if defined(LED_PWM_FADE_ENABLE) && (LED_PWM_FADE_ENABLE == 1)
    ledc_set_fade_with_time(led_pwm_channel[ch].speed_mode, led_pwm_channel[ch].channel, targ, LED_PWM_FADE_TIME);
    ledc_fade_start(led_pwm_channel[ch].speed_mode,  led_pwm_channel[ch].channel, LEDC_FADE_NO_WAIT);
#else   
    ledc_set_duty(led_pwm_channel[ch].speed_mode, led_pwm_channel[ch].channel, targ);
    ledc_update_duty(led_pwm_channel[ch].speed_mode, led_pwm_channel[ch].channel);
#endif
}