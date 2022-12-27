#include <stdio.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include "led_pwm.h"

#define TAG                             "LedPwm"
#define LED_PWM_SPEED_MODE              LEDC_HIGH_SPEED_MODE
#define LED_PWM_TIMER                   LEDC_TIMER_0
#define LED_PWM_DUTY_RESOLUTION         LEDC_TIMER_11_BIT
#define LED_PWM_DUTY_MAX                ((0x1 << LED_PWM_DUTY_RESOLUTION) - 1)
#define LED_PWM_FREQ                    (18 * 1000)
#define LED_PWM_FADE_ENABLE             1
#define LED_PWM_FADE_TIME               (500)
#define LED_PWM_FADE_SCALE              (6)
#define LED_PWM_FADE_CYCLE_NUM          (18 * 4)

#define LED_CURRENT_2_DUTY(_c_, _max_)         (_c_ * LED_PWM_DUTY_MAX / _max_)
#define LED_OUTPUT_CFG(_min_, _range)    { \
    .min = _min_,                         \
    .range = _range,               \
}

#define LED_CHANNEL_CFG(_chl, _gpio_num)     { \
    .gpio_num   = _gpio_num,               \
    .channel    = _chl,                    \
    .speed_mode = LED_PWM_SPEED_MODE,      \
    .intr_type  = LEDC_INTR_DISABLE,       \
    .timer_sel  = LED_PWM_TIMER,           \
    .duty       = 0,                       \
    .hpoint     = 0,                       \
}

#if defined(CONFIG_DEVICE_MODEL_LM30) && CONFIG_DEVICE_MODEL_LM30
const ledc_channel_config_t ledc_chan_cfg[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = LED_CHANNEL_CFG(LEDC_CHANNEL_0, GPIO_NUM_12),
    [LED_CHAN_DARK_BLUE] = LED_CHANNEL_CFG(LEDC_CHANNEL_1, GPIO_NUM_26),
    [LED_CHAN_BLUE]      = LED_CHANNEL_CFG(LEDC_CHANNEL_2, GPIO_NUM_13),
    [LED_CHAN_RED]       = LED_CHANNEL_CFG(LEDC_CHANNEL_3, GPIO_NUM_33),
    [LED_CHAN_GREEN]     = LED_CHANNEL_CFG(LEDC_CHANNEL_4, GPIO_NUM_25),
    [LED_CHAN_UV_405]    = LED_CHANNEL_CFG(LEDC_CHANNEL_5, GPIO_NUM_27),
    [LED_CHAN_UV_415]    = LED_CHANNEL_CFG(LEDC_CHANNEL_6, GPIO_NUM_32),
};
#else
const ledc_channel_config_t ledc_chan_cfg[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = LED_CHANNEL_CFG(LEDC_CHANNEL_0, GPIO_NUM_25),
    [LED_CHAN_DARK_BLUE] = LED_CHANNEL_CFG(LEDC_CHANNEL_1, GPIO_NUM_27),
    [LED_CHAN_BLUE]      = LED_CHANNEL_CFG(LEDC_CHANNEL_2, GPIO_NUM_13),
    [LED_CHAN_RED]       = LED_CHANNEL_CFG(LEDC_CHANNEL_3, GPIO_NUM_33),
    [LED_CHAN_GREEN]     = LED_CHANNEL_CFG(LEDC_CHANNEL_4, GPIO_NUM_32),
    [LED_CHAN_UV_405]    = LED_CHANNEL_CFG(LEDC_CHANNEL_5, GPIO_NUM_26),
    [LED_CHAN_UV_415]    = LED_CHANNEL_CFG(LEDC_CHANNEL_6, GPIO_NUM_12),
};
#endif

#if defined(CONFIG_DEVICE_MODEL_LM30) && CONFIG_DEVICE_MODEL_LM30
const led_output_cfg_t ledc_output_cfg[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = LED_OUTPUT_CFG( 10, 1000 ),
    [LED_CHAN_DARK_BLUE] = LED_OUTPUT_CFG( 10, 1000 ), 
    [LED_CHAN_BLUE]      = LED_OUTPUT_CFG( 10, 1000 ),  
    [LED_CHAN_RED]       = LED_OUTPUT_CFG( 10, 1000 ),     
    [LED_CHAN_GREEN]     = LED_OUTPUT_CFG( 10, 1000 ), 
    [LED_CHAN_UV_405]    = LED_OUTPUT_CFG( 10, 1000 ), 
    [LED_CHAN_UV_415]    = LED_OUTPUT_CFG( 10, 1000 ), 
};
#elif defined(CONFIG_DEVICE_MODEL_LM15) && CONFIG_DEVICE_MODEL_LM15
const led_output_cfg_t ledc_output_cfg[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = LED_OUTPUT_CFG( 10, 780 ),
    [LED_CHAN_DARK_BLUE] = LED_OUTPUT_CFG( 10, 780 ), 
    [LED_CHAN_BLUE]      = LED_OUTPUT_CFG( 10, 780 ),  
    [LED_CHAN_RED]       = LED_OUTPUT_CFG( 10, 780 ),     
    [LED_CHAN_GREEN]     = LED_OUTPUT_CFG( 10, 780 ), 
    [LED_CHAN_UV_405]    = LED_OUTPUT_CFG( 10, 780 ), 
    [LED_CHAN_UV_415]    = LED_OUTPUT_CFG( 10, 780 ), 
};
#else
const led_output_cfg_t ledc_output_cfg[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = LED_OUTPUT_CFG( 10, 900 ),
    [LED_CHAN_DARK_BLUE] = LED_OUTPUT_CFG( 10, 900 ), 
    [LED_CHAN_BLUE]      = LED_OUTPUT_CFG( 10, 900 ),  
    [LED_CHAN_RED]       = LED_OUTPUT_CFG( 10, 900 ),     
    [LED_CHAN_GREEN]     = LED_OUTPUT_CFG( 10, 900 ), 
    [LED_CHAN_UV_405]    = LED_OUTPUT_CFG( 10, 900 ), 
    [LED_CHAN_UV_415]    = LED_OUTPUT_CFG( 10, 900 ), 
};
#endif

led_value_t ledc_chan_output[LED_CHAN_COUNT] = { 0, 0, 0, 0, 0, 0, 0 };

void led_pwm_initialise()
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LED_PWM_DUTY_RESOLUTION, 
        .freq_hz    = LED_PWM_FREQ,    
        .speed_mode = LED_PWM_SPEED_MODE,
        .timer_num  = LED_PWM_TIMER
    };
    ESP_ERROR_CHECK( ledc_timer_config(&ledc_timer) );
    for(int i = 0; i < LED_CHAN_COUNT; i++) {
        ESP_ERROR_CHECK( ledc_channel_config(&ledc_chan_cfg[i]) );
    }
#if defined(LED_PWM_FADE_ENABLE) && (LED_PWM_FADE_ENABLE == 1)
    ESP_ERROR_CHECK( ledc_fade_func_install(0) );
    for (int i = 0; i < LED_CHAN_COUNT; i++) {
        ledc_set_fade_step_and_start(ledc_chan_cfg[i].speed_mode, 
                                     ledc_chan_cfg[i].channel, 
                                     0, 
                                     LED_PWM_FADE_SCALE,
                                     LED_PWM_FADE_CYCLE_NUM, 
                                     LEDC_FADE_WAIT_DONE);
    }
#else 
    for (int i = 0; i < LED_CHAN_COUNT; i++) {
        ledc_set_duty(ledc_chan_cfg[i].speed_mode, ledc_chan_cfg[i].channel, 0);
        ledc_update_duty(ledc_chan_cfg[i].speed_mode, ledc_chan_cfg[i].channel);
    }
#endif  
}

void led_pwm_deinitialise()
{
#if defined(LED_PWM_FADE_ENABLE) && (LED_PWM_FADE_ENABLE == 1)
    for (int i = 0; i < LED_CHAN_COUNT; i++) {
        ledc_set_fade_step_and_start(ledc_chan_cfg[i].speed_mode, 
                                     ledc_chan_cfg[i].channel, 
                                     0, 
                                     LED_PWM_FADE_SCALE,
                                     LED_PWM_FADE_CYCLE_NUM, 
                                     LEDC_FADE_WAIT_DONE);
    }
    ledc_fade_func_uninstall();
#else
    for (int i = 0; i < LED_CHAN_COUNT; i++) {
        ledc_set_duty(ledc_chan_cfg[i].speed_mode, ledc_chan_cfg[i].channel, 0);
        ledc_update_duty(ledc_chan_cfg[i].speed_mode, ledc_chan_cfg[i].channel);
    }        
#endif    
}

void led_pwm_output(led_chan_t ch, led_value_t percent)
{
    uint32_t targ;
    
    if (percent > LED_VALUE_MAX) 
        percent = LED_VALUE_MAX; 
    
    if (percent == ledc_chan_output[ch])
        return;

    ledc_chan_output[ch] = percent;
    if (percent < 5) {
       targ = 0; 
    } else {
       targ = ledc_output_cfg[ch].range * (percent - 5) / LED_VALUE_MAX + ledc_output_cfg[ch].min;
    }   
#if defined(LED_PWM_FADE_ENABLE) && (LED_PWM_FADE_ENABLE == 1)                         
    ledc_set_fade_step_and_start(ledc_chan_cfg[ch].speed_mode, ledc_chan_cfg[ch].channel,
                                 targ * LED_PWM_DUTY_MAX / LED_VALUE_MAX, 
                                 LED_PWM_FADE_SCALE,
                                 LED_PWM_FADE_CYCLE_NUM,
                                 LEDC_FADE_NO_WAIT);
#else    
    ledc_set_duty(ledc_chan_cfg[ch].speed_mode, 
                  ledc_chan_cfg[ch].channel, 
                  targ * LED_PWM_DUTY_MAX / LED_VALUE_MAX);
    ledc_update_duty(ledc_chan_cfg[ch].speed_mode,
                     ledc_chan_cfg[ch].channel);
#endif
}

void led_pwm_output_play(led_chan_t ch, led_value_t percent, uint32_t scale)
{
    uint32_t targ;
    
    if (percent > LED_VALUE_MAX) 
        percent = LED_VALUE_MAX; 
    
    if (percent == ledc_chan_output[ch])
        return;

    ledc_chan_output[ch] = percent;
    if (percent < 5) {
       targ = 0; 
    } else {
       targ = ledc_output_cfg[ch].range * (percent - 5) / LED_VALUE_MAX + ledc_output_cfg[ch].min;
    }
#if defined(LED_PWM_FADE_ENABLE) && (LED_PWM_FADE_ENABLE == 1)                                
    ledc_set_fade_step_and_start(ledc_chan_cfg[ch].speed_mode, ledc_chan_cfg[ch].channel,
                                 targ * LED_PWM_DUTY_MAX / LED_VALUE_MAX, 
                                 scale,
                                 LED_PWM_FADE_CYCLE_NUM,
                                 LEDC_FADE_NO_WAIT);
#else    
    ledc_set_duty(ledc_chan_cfg[ch].speed_mode, 
                  ledc_chan_cfg[ch].channel, 
                  targ * LED_PWM_DUTY_MAX / LED_VALUE_MAX);
    ledc_update_duty(ledc_chan_cfg[ch].speed_mode,
                     ledc_chan_cfg[ch].channel);
#endif
}