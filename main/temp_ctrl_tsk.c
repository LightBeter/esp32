#include <string.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_err.h>
#include <esp_log.h>
#include "temp_ctrl_tsk.h"
#include "led_ctrl_tsk.h"

#define TAG                         "TempCtrlTsk"
#define TSK_NAME                    TAG
#define TSK_STACK_SIZE              2048
#define TEMP_CTRL_SAMPLE_INTERVAL   20
#define TEMP_CTRL_INTERVAL          (1000 / TEMP_CTRL_SAMPLE_INTERVAL)

#define TEMP_CTRL_ADC_DEFAULT_VREF  1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define TEMP_CTRL_ADC_BUFFER_LEN    64
#define TEMP_CTRL_ADC_UNIT          ADC_UNIT_1
#define TEMP_CTRL_ADC_WIDTH_BIT     ADC_WIDTH_BIT_12
#define TEMP_CTRL_ADC_ATTEN         ADC_ATTEN_DB_11
#define TEMP_CTRL_ADC_CHL           ADC1_CHANNEL_0 //ADC1_CHANNEL_3

#define TEMP_OVER_LIMIT             64
#define TEMP_OVER_CANCEL            40
#define TEMP_CTRL_FAN_OPEN_LIMIT    45
#define TEMP_CTRL_FAN_CLOSE_LIMIT   30
#define GPIO_CHAN_FAN               GPIO_NUM_2
#define TEMP_CTRL_FAN_OPEN_LEVEL    1
#define TEMP_CTRL_FAN_CLOSE_LEVEL   0

#define TEMP_MAX                    (120)
#define TEMP_MIN                    (-40)
#define TEMP_MAP_SIZE               (TEMP_MAX - (TEMP_MIN) + 1)

void temp_ctrl_tsk(void * parm);

const adc1_channel_t   _sensorConf[TEMP_SENSOR_COUNT] = {  
    ADC1_CHANNEL_7, 
    ADC1_CHANNEL_6 
};
const uint32_t _tempMapTable[TEMP_MAP_SIZE] = {
    264279,  250344,  237130,  224603,  212733,  201487,  190836,  180750,  171201,  162163, 
    153610,  145516,  137858,  130614,  123761,  117280,  111149,  105351,  99867,   94681, 
    89776,   85137,   80750,   76600,   72676,   68963,   65451,   62129,   58986,   56012, 
    53198,   50534,   48013,   45627,   43368,   41229,   39204,   37285,   35468,   33747, 
    32116,   30570,   29105,   27716,   26399,   25150,   23965,   22842,   21776,   20764, 
    19783,   18892,   18026,   17204,   16423,   15681,   14976,   14306,   13669,   13063, 
    12487,   11939,   11418,   10921,   10449,   10000,   9571,    9164,    8775,    8405, 
    8052,    7716,    7396,    7090,    6798,    6520,    6255,    6002,    5760,    5529, 
    5309,    5098,    4897,    4704,    4521,    4345,    4177,    4016,    3863,    3716, 
    3588,    3440,    3311,    3188,    3069,    2956,    2848,    2744,    2644,    2548, 
    2457,    2369,    2284,    2204,    2126,    2051,    1980,    1911,    1845,    1782, 
    1721,    1663,    1606,    1552,    1500,    1450,    1402,    1356,    1312,    1269, 
    1228,    1188,    1150,    1113,    1078,    1044,    1011,    979,     948,     919, 
    891,     863,     837,     811,     787,     763,     740,     718,     697,     676, 
    657,     637,     619,     601,     584,     567,     551,     535,     520,     505, 
    491,     477,     464,     451,     439,     427,     415,     404,     393,     383, 
    373, 
};
temp_ctrl_tsk_type   _tempCtrlTsk;

void temp_ctrl_tsk_init()
{
    /* initial adc */
    ESP_ERROR_CHECK( adc1_config_width(TEMP_CTRL_ADC_WIDTH_BIT) );
    for (int index = 0; index < TEMP_SENSOR_COUNT; index++) {
        ringbuffer_init( &_tempCtrlTsk.sensor[index], TEMP_CTRL_ADC_BUFFER_LEN );
    }
    _tempCtrlTsk.handle = NULL;
    _tempCtrlTsk.over   = false;
    /* initial gpio */
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_CHAN_FAN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_CHAN_FAN, TEMP_CTRL_FAN_CLOSE_LEVEL);
    /* initial tsk */
}

void temp_ctrl_temperature_resp(cJSON *respone)
{
    cJSON_AddNumberToObject(respone, "temperature", _tempCtrlTsk.temperature);
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
        xTaskCreate(temp_ctrl_tsk, TSK_NAME, TSK_STACK_SIZE, NULL, 3, &_tempCtrlTsk.handle);
    }
}

void temp_ctrl_tsk_stop()
{
    if (_tempCtrlTsk.handle) {
        vTaskDelete(_tempCtrlTsk.handle);
        _tempCtrlTsk.handle = NULL; 
    }
    gpio_set_level(GPIO_CHAN_FAN, TEMP_CTRL_FAN_CLOSE_LEVEL);
}

bool temp_is_over()
{
    return _tempCtrlTsk.over;
}

int16_t SearchTableConvert(const uint32_t r) {
    uint32_t left = 0, right = TEMP_MAP_SIZE - 1, mid = 0;

    if ( r > _tempMapTable[0] ) {
        return 0 + TEMP_MIN;
    } else if ( r < _tempMapTable[TEMP_MAP_SIZE - 1] ) {
        return TEMP_MAX;
    }

    mid = ( left + right ) / 2;
    while( left < right ) {
        if ( _tempMapTable[mid] == r ) {
            break;
        } else if( _tempMapTable[mid] < r ) {
            right = mid - 1;
        } else if( _tempMapTable[mid] > r ) {
            left = mid + 1;
        }
        mid = ( left + right ) / 2;
    }
    return mid + TEMP_MIN;
}

void temp_ctrl_tsk(void * parm)
{
    int temp;
    uint16_t tick = 0, index, value;
    uint32_t Vref, Vntc, Rntc;
    esp_adc_cal_value_t val_type;
    esp_adc_cal_characteristics_t adc_chars;
    float tmp;
    
    ESP_ERROR_CHECK( adc1_config_channel_atten(TEMP_CTRL_ADC_CHL, TEMP_CTRL_ADC_ATTEN) );
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
            temp = adc1_get_raw(_sensorConf[index]);
            ringbuffer_enter(&_tempCtrlTsk.sensor[index], temp);
        }
        if (tick++ < TEMP_CTRL_INTERVAL) 
            goto next;
        
        tick = 0;
        value = ringbuffer_avg(&_tempCtrlTsk.sensor[TEMP_SENSOR_REF]);        
        Vref = esp_adc_cal_raw_to_voltage(value, &adc_chars);
        value = ringbuffer_avg(&_tempCtrlTsk.sensor[TEMP_SENSOR_NTC]);        
        Vntc = esp_adc_cal_raw_to_voltage(value, &adc_chars);
        tmp = (15100.0/10000) * Vref - Vntc;
        if (tmp <= 0) {
            Rntc = 0;
        } else {
            Rntc = (Vntc * 5100) / tmp;
        }
        _tempCtrlTsk.temperature = SearchTableConvert(Rntc);
        if (led_ctrl_is_close()) {
            gpio_set_level(GPIO_CHAN_FAN, TEMP_CTRL_FAN_CLOSE_LEVEL);
        } else if (_tempCtrlTsk.temperature >= TEMP_CTRL_FAN_OPEN_LIMIT) {
            gpio_set_level(GPIO_CHAN_FAN, TEMP_CTRL_FAN_OPEN_LEVEL);
        } else if (_tempCtrlTsk.temperature <= TEMP_CTRL_FAN_CLOSE_LIMIT) {
            gpio_set_level(GPIO_CHAN_FAN, TEMP_CTRL_FAN_CLOSE_LEVEL);
        }

        if (_tempCtrlTsk.temperature >= TEMP_OVER_LIMIT) {
            _tempCtrlTsk.over = true;
        } else if (_tempCtrlTsk.temperature <= TEMP_OVER_CANCEL) {
            _tempCtrlTsk.over = false;
        }
        //ESP_LOGI( TAG, "temperature %d", _tempCtrlTsk.temperature );
next:        
        vTaskDelay(pdMS_TO_TICKS(TEMP_CTRL_SAMPLE_INTERVAL));
    }
}