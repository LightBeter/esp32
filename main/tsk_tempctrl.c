#include <string.h>
#include <driver/ledc.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "tsk_tempctrl.h"
#include "tsk_ledctrl_misc.h"
#include "storage.h"
#include "misc.h"
#include "ringbuffer.h"

#define TAG                         "TempCtrlTsk"
#define TSK_NAME                    TAG
#define TSK_STACK_SIZE              4096
#define TEMP_CTRL_SAMPLE_INTERVAL   200
#define TEMP_CTRL_INTERVAL          (1000 / TEMP_CTRL_SAMPLE_INTERVAL)

#define ADC_HW_DEFAULT_VREF         1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define ADC_HW_UNIT                 ADC_UNIT_1
#define ADC_HW_WIDTH_BIT            ADC_WIDTH_BIT_12
#define ADC_HW_ATTEN                ADC_ATTEN_DB_11
#define ADC_BUFFER_LEN              100
#define GPIO_CHAN_FAN               GPIO_NUM_2

#define TEMP_MAX                    (120)
#define TEMP_MIN                    (-40)
#define TEMP_MAP_SIZE               (TEMP_MAX - (TEMP_MIN) + 1)
#define TEMP_OVER_LIMIT             68
#define TEMP_OVER_CANCEL            40
#define TEMP_CTRL_T1                38
#define TEMP_CTRL_T2                45
#define TEMP_CTRL_T3                50
#define TEMP_CTRL_T4                56

#define FAN_PWM_SPEED_MODE          LEDC_HIGH_SPEED_MODE
#define FAN_PWM_TIMER               LEDC_TIMER_1
#define FAN_PWM_GPIO                GPIO_CHAN_FAN
#define FAN_PWM_CHANNEL             LEDC_CHANNEL_7
#define FAN_PWM_DUTY_RESOLUTION     LEDC_TIMER_11_BIT
#define FAN_PWM_FREQ                (32 * 1000)
#define FAN_PWM_DUTY_MAX            ((0x1 << FAN_PWM_DUTY_RESOLUTION) - 1)
#define FAN_DUTY_DEFAILT            (30)

void temp_ctrl_fan_update();
void temp_ctrl_tsk(void * parm);

const adc1_channel_t ADC_HW_CFG[ADC_CHAN_COUNT] = { ADC1_CHANNEL_6,  ADC1_CHANNEL_7 };
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

static xTaskHandle      tsk_handle = NULL;
static ringbuffer_type  adcBufs[ADC_CHAN_COUNT];
static uint16_t     playTime = 0;
static uint16_t     fanDuty = 0;
static int16_t      curTemperature = 0;
static bool         isTemperatureOver = false;
static bool         isTemperatureHigh = false;
static uint32_t     curSaveToken = 0;
static uint32_t     preSaveToken = 0;


void tsk_tempctrl_init()
{
    /* initial adc */
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = FAN_PWM_DUTY_RESOLUTION, 
        .freq_hz    = FAN_PWM_FREQ,    
        .speed_mode = FAN_PWM_SPEED_MODE,
        .timer_num  = FAN_PWM_TIMER
    };

    ledc_channel_config_t fan_chan_cfg = {
        .gpio_num   = FAN_PWM_GPIO,            
        .channel    = FAN_PWM_CHANNEL,           
        .speed_mode = FAN_PWM_SPEED_MODE,      
        .intr_type  = LEDC_INTR_DISABLE,       
        .timer_sel  = FAN_PWM_TIMER,           
        .duty       = 0,                       
        .hpoint     = 0,                       
    };
    
    ESP_LOGI( TAG, "initialize" );
    ESP_ERROR_CHECK( adc1_config_width(ADC_HW_WIDTH_BIT) );
    ESP_ERROR_CHECK( ledc_timer_config(&ledc_timer) );
    ESP_ERROR_CHECK( ledc_channel_config(&fan_chan_cfg) );

    for (int index = 0; index < ADC_CHAN_COUNT; index++)
        ringbuffer_init( &adcBufs[index], ADC_BUFFER_LEN );

    if (!storage_read_fan_duty(&fanDuty)) {
        fanDuty = FAN_DUTY_DEFAILT;
        storage_write_fan_duty(fanDuty);
    } 

    if (INVALID_FAN_DUTY(fanDuty)) {
        ESP_LOGW( TAG, "invalid fan duty %d\n", fanDuty);
        fanDuty = FAN_DUTY_DEFAILT;  
    }

    ledc_set_duty(FAN_PWM_SPEED_MODE, FAN_PWM_CHANNEL, 0);
    ledc_update_duty(FAN_PWM_SPEED_MODE, FAN_PWM_CHANNEL);
}

void tsk_tempctrl_deinit()
{
    ESP_LOGI( TAG, "deinitialize" );
    for (int index = 0; index < ADC_CHAN_COUNT; index++)
        ringbuffer_deinit( &adcBufs[index] );
    
}

int16_t tempctrl_get_temperature()
{
    return curTemperature;
}

void tsk_tempctrl_start()
{
    ESP_LOGI( TAG, "start" );
    if (!tsk_handle) 
        xTaskCreatePinnedToCore(temp_ctrl_tsk, TSK_NAME, TSK_STACK_SIZE, NULL, TSK_PRIORITY_TEMPCTRL, &tsk_handle, 0);
}

void tsk_tempctrl_stop()
{
    ESP_LOGI( TAG, "stop" );
    if (tsk_handle) {
        vTaskDelete(tsk_handle);
        tsk_handle = NULL; 
    }
    ledc_set_duty(FAN_PWM_SPEED_MODE, FAN_PWM_CHANNEL, 0);
    ledc_update_duty(FAN_PWM_SPEED_MODE, FAN_PWM_CHANNEL);
}

bool tempctrl_is_over()
{
    return isTemperatureOver;
}

int16_t SearchTableConvert(const uint32_t r) 
{
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

uint16_t tempctrl_get_fanduty()
{
    return fanDuty;
}

void tempctrl_set_fanduty(uint16_t value)
{
    if (!INVALID_FAN_DUTY(value) && (fanDuty != value)) {
        fanDuty = value;
        curSaveToken++;
    }
}

void tempctrl_set_fanplay(uint16_t value)
{
    if (!INVALID_FAN_PLAY(value))
        playTime = value;
}

void temp_ctrl_fan_update()
{
    static uint16_t output = 0;

    if (preSaveToken != curSaveToken && 0 == playTime) {
        storage_write_fan_duty(fanDuty);
        preSaveToken = curSaveToken;
    }

    if (playTime >  0) {
        playTime -= 1;
        output = fanDuty;
    } else if (false == ledctrl_switch()) {
        isTemperatureHigh = false;
        output = 0;
    } else {
        if (curTemperature >= TEMP_CTRL_T4) {
            output = 100;
            isTemperatureHigh = true;
        } else if (isTemperatureHigh && curTemperature > TEMP_CTRL_T3) {
            output = 100;  
        } else {
            isTemperatureHigh = false; 
            if (curTemperature >= TEMP_CTRL_T2) {
                output = fanDuty;
            } else if (curTemperature <= TEMP_CTRL_T1) {
                output = 0;
            }
        }
    }

    if (output > 0 && output < 40)
        output = 40;

    ledc_set_duty(FAN_PWM_SPEED_MODE, FAN_PWM_CHANNEL, (output / 100.0) * FAN_PWM_DUTY_MAX );
    ledc_update_duty(FAN_PWM_SPEED_MODE, FAN_PWM_CHANNEL);
}

void temp_ctrl_tsk(void * parm)
{
    int temp;
    uint16_t tick = 0, index, value;
    uint32_t Vref, Vntc, Rntc;
    esp_adc_cal_value_t val_type;
    esp_adc_cal_characteristics_t adc_chars;
    float tmp;
    
    ESP_ERROR_CHECK( adc1_config_channel_atten(ADC_HW_CFG[ADC_CHAN_REF], ADC_HW_ATTEN) );
    ESP_ERROR_CHECK( adc1_config_channel_atten(ADC_HW_CFG[ADC_CHAN_NTC], ADC_HW_ATTEN) );
    //Characterize ADC
    memset(&adc_chars, 0x00, sizeof(esp_adc_cal_characteristics_t));
    val_type = esp_adc_cal_characterize(ADC_HW_UNIT, ADC_HW_ATTEN, ADC_HW_WIDTH_BIT, ADC_HW_DEFAULT_VREF, &adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI( TAG, "Characterized using Two Point Value" );
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI( TAG, "Characterized using eFuse Vref" );
    } else {
        ESP_LOGI( TAG, "Characterized using Default Vref" );
    }
  
    while (1) {
        for (temp = 0; temp < 10; temp++) {
            for (index = 0; index < ADC_CHAN_COUNT; index++) {
                temp = adc1_get_raw(ADC_HW_CFG[index]);
                ringbuffer_enter(&adcBufs[index], temp);
            }
        }
        if (tick++ < TEMP_CTRL_INTERVAL) 
            goto next;
        
        tick = 0;
        value = ringbuffer_avg(&adcBufs[ADC_CHAN_REF]);        
        Vref = esp_adc_cal_raw_to_voltage(value, &adc_chars);
        value = ringbuffer_avg(&adcBufs[ADC_CHAN_NTC]);        
        Vntc = esp_adc_cal_raw_to_voltage(value, &adc_chars);
        tmp = (15100.0/10000) * Vref - Vntc;
        if (tmp <= 0) {
            Rntc = 0;
        } else {
            Rntc = (Vntc * 5100) / tmp;
        }
        curTemperature = SearchTableConvert(Rntc);
        if (curTemperature >= TEMP_OVER_LIMIT) {
            isTemperatureOver = true;
        } else if (curTemperature <= TEMP_OVER_CANCEL) {
            isTemperatureOver = false;
        }
        temp_ctrl_fan_update();
next:        
        vTaskDelay( pdMS_TO_TICKS(TEMP_CTRL_SAMPLE_INTERVAL) );
    }
}