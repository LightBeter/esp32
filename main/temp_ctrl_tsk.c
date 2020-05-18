#include <string.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_err.h>
#include <esp_log.h>
#include "temp_ctrl_tsk.h"

#define TAG                         "TempCtrlTsk"
#define TSK_NAME                    TAG
#define TEMP_CTRL_TSK_STACK_SIZE    2048
#define TEMP_CTRL_SAMPLE_INTERVAL   20
#define TEMP_CTRL_INTERVAL          (1000 / TEMP_CTRL_SAMPLE_INTERVAL)

#define TEMP_CTRL_ADC_DEFAULT_VREF  1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define TEMP_CTRL_ADC_BUFFER_LEN    64
#define TEMP_CTRL_ADC_UNIT          ADC_UNIT_1
#define TEMP_CTRL_ADC_WIDTH_BIT     ADC_WIDTH_BIT_12
#define TEMP_CTRL_ADC_ATTEN         ADC_ATTEN_DB_11

#define TEMP_CTRL_FAN_OPEN_LIMIT    1234
#define TEMP_CTRL_FAN_CLOSE_LIMIT   1234
#define TEMP_CTRL_FAN_CHL           (1ULL << GPIO_NUM_2)
#define TEMP_CTRL_FAN_OPEN_LEVEL    1
#define TEMP_CTRL_FAN_CLOSE_LEVEL   0

void temp_ctrl_tsk(void * parm);

const adc1_channel_t _sensor_conf[TEMP_SENSOR_COUNT] = {  ADC1_CHANNEL_0, ADC1_CHANNEL_3 };
temp_ctrl_tsk_type   _tempCtrlTsk;

void temp_ctrl_tsk_init()
{
    /* initial adc */
    ESP_ERROR_CHECK( adc1_config_width(TEMP_CTRL_ADC_WIDTH_BIT) );
    for (int index = 0; index < TEMP_SENSOR_COUNT; index++) {
        ringbuffer_init( &_tempCtrlTsk.sensor[index], TEMP_CTRL_ADC_BUFFER_LEN );
    }
    _tempCtrlTsk.handle = NULL;
    /* initial gpio */
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = TEMP_CTRL_FAN_CHL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(TEMP_CTRL_FAN_CHL, TEMP_CTRL_FAN_CLOSE_LEVEL);
    /* initial tsk */
}

void temp_ctrl_tsk_deinit()
{
    for (int index = 0; index < TEMP_SENSOR_COUNT; index++) {
        ringbuffer_deinit( &_tempCtrlTsk.sensor[index] );
    }
}

void temp_ctrl_tsk_start()
{
    if (!_tempCtrlTsk.handle) {
        xTaskCreate(temp_ctrl_tsk, TAG, TEMP_CTRL_TSK_STACK_SIZE, NULL, 3, &_tempCtrlTsk.handle);
    }
}

void temp_ctrl_tsk_stop()
{
    if (_tempCtrlTsk.handle) {
        vTaskDelete(_tempCtrlTsk.handle);
        _tempCtrlTsk.handle = NULL; 
    }
}

void temp_ctrl_tsk(void * parm)
{
    uint16_t tick = 0, index = 0, adc_max, temp;
    uint32_t voltage;
    esp_adc_cal_value_t val_type;
    esp_adc_cal_characteristics_t adc_chars;
    
    for (index = 0; index < TEMP_SENSOR_COUNT; index++) {
        ESP_ERROR_CHECK( adc1_config_channel_atten(_sensor_conf[index], TEMP_CTRL_ADC_ATTEN) );
    }
    //Characterize ADC
    memset(&adc_chars, 0x00, sizeof(esp_adc_cal_characteristics_t));
    val_type = esp_adc_cal_characterize(TEMP_CTRL_ADC_UNIT, 
                                        TEMP_CTRL_ADC_ATTEN, 
                                        TEMP_CTRL_ADC_WIDTH_BIT, 
                                        TEMP_CTRL_ADC_DEFAULT_VREF, 
                                        &adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI( TAG, "Characterized using Two Point Value" );
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI( TAG, "Characterized using eFuse Vref" );
    } else {
        ESP_LOGI( TAG, "Characterized using Default Vref" );
    }
    
    while (1) {
        for (index = 0; index < TEMP_SENSOR_COUNT; index++) {
            temp = adc1_get_raw(_sensor_conf[index]);
            ringbuffer_enter(&_tempCtrlTsk.sensor[index], temp);
        }

        if (tick++ < TEMP_CTRL_INTERVAL) 
            goto next;
        
        tick = 0;
        for (adc_max = 0, index = 0; index < TEMP_SENSOR_COUNT; index++) {
            temp = ringbuffer_avg(&_tempCtrlTsk.sensor[index]);
            if (temp > adc_max) adc_max = temp;
        }
        
        voltage = esp_adc_cal_raw_to_voltage(adc_max, &adc_chars);
        if (voltage >= TEMP_CTRL_FAN_OPEN_LIMIT) {
            gpio_set_level(TEMP_CTRL_FAN_CHL, TEMP_CTRL_FAN_OPEN_LEVEL);
        } else if (voltage <= TEMP_CTRL_FAN_CLOSE_LIMIT) {
            gpio_set_level(TEMP_CTRL_FAN_CHL, TEMP_CTRL_FAN_CLOSE_LEVEL);
        }
        //ESP_LOGI( TAG, "voltage %u",  voltage);
next:        
        vTaskDelay(pdMS_TO_TICKS(TEMP_CTRL_SAMPLE_INTERVAL));
    }
}