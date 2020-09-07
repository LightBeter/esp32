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
#define LED_PWM_FADE_ENABLE             1
#define LED_PWM_FADE_TIME               (500)

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
#if 0
const ledc_channel_config_t ledc_chan_cfg[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = LED_CHANNEL_CFG(LEDC_CHANNEL_0, GPIO_NUM_32),
    [LED_CHAN_DARK_BLUE] = LED_CHANNEL_CFG(LEDC_CHANNEL_1, GPIO_NUM_25),
    [LED_CHAN_BLUE]      = LED_CHANNEL_CFG(LEDC_CHANNEL_2, GPIO_NUM_33),
    [LED_CHAN_RED]       = LED_CHANNEL_CFG(LEDC_CHANNEL_3, GPIO_NUM_14),
    [LED_CHAN_GREEN]     = LED_CHANNEL_CFG(LEDC_CHANNEL_4, GPIO_NUM_12),
    [LED_CHAN_UV_405]    = LED_CHANNEL_CFG(LEDC_CHANNEL_5, GPIO_NUM_26),
    [LED_CHAN_UV_415]    = LED_CHANNEL_CFG(LEDC_CHANNEL_6, GPIO_NUM_27),
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

#define LED_PWM_FREQ                   (18 * 1000)
const led_output_cfg_t ledc_output_cfg[LED_CHAN_COUNT] = {
    [LED_CHAN_WHITE]     = LED_OUTPUT_CFG( 80, 890 ),
    [LED_CHAN_DARK_BLUE] = LED_OUTPUT_CFG( 80, 890 ), 
    [LED_CHAN_BLUE]      = LED_OUTPUT_CFG( 80, 890 ),  
    [LED_CHAN_RED]       = LED_OUTPUT_CFG( 80, 890 ),     
    [LED_CHAN_GREEN]     = LED_OUTPUT_CFG( 80, 890 ), 
    [LED_CHAN_UV_405]    = LED_OUTPUT_CFG( 80, 890 ), 
    [LED_CHAN_UV_415]    = LED_OUTPUT_CFG( 80, 890 ), 
};


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
    ledc_set_fade_with_time(ledc_chan_cfg[ch].speed_mode, 
                            ledc_chan_cfg[ch].channel, 
                            targ * LED_PWM_DUTY_MAX / LED_VALUE_MAX, 
                            LED_PWM_FADE_TIME);                          
    ledc_fade_start(ledc_chan_cfg[ch].speed_mode, 
                    ledc_chan_cfg[ch].channel, 
                    LEDC_FADE_NO_WAIT);
#else    
    ledc_set_duty(ledc_chan_cfg[ch].speed_mode, 
                  ledc_chan_cfg[ch].channel, 
                  LED_CURRENT_2_DUTY(targ, ledc_output_cfg[ch].max));
    ledc_update_duty(ledc_chan_cfg[ch].speed_mode,
                     ledc_chan_cfg[ch].channel);
#endif
}