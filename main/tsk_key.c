#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <time.h>
#include "tsk_key.h"
#include "misc.h"
#include "tsk_ledctrl_misc.h"

#define TAG                     "TSK_KEY"
#define TSK_NAME                TAG
#define TSK_STACK_SIZE          2048
#define KEY_GPIO_PIN            21
#define KEY_INTR_FLAG_DEFAULT   0

void key_tsk(void * parm);

static xTaskHandle  tsk_handle = NULL;
static xQueueHandle gpio_evt_queue = NULL;

void key_isr_handler(void* arg)
{
    uint8_t level = KEY_STATE_DOWN;//gpio_get_level(KEY_GPIO_PIN);
    xQueueSendFromISR(gpio_evt_queue, &level, NULL);
}

void tsk_key_init()
{
    gpio_config_t io_conf;
    ESP_LOGI( TAG, "initialize" );
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << KEY_GPIO_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_evt_queue = xQueueCreate(2, sizeof(uint8_t));
}

void tsk_key_deinit()
{
    ESP_LOGI( TAG, "deinitialize" );
    if (gpio_evt_queue) {
        vQueueDelete(gpio_evt_queue);
    }
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << KEY_GPIO_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
}

void tsk_key_start()
{
    ESP_LOGI( TAG, "start" );
    if (!tsk_handle) {
        xTaskCreate(key_tsk, TAG, TSK_STACK_SIZE, NULL, TSK_PRIORITY_KEY, &tsk_handle);
    }
    gpio_install_isr_service(KEY_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(KEY_GPIO_PIN, key_isr_handler, NULL);
}

void tsk_key_stop()
{
    ESP_LOGI( TAG, "stop" );
    gpio_isr_handler_remove(KEY_GPIO_PIN);
    gpio_uninstall_isr_service();
    if (tsk_handle) {
        vTaskDelete(tsk_handle);
        tsk_handle = NULL; 
    }
}

void key_tsk(void * parm)
{
    uint8_t preLevel = KEY_STATE_UP, level;
    clock_t tstart, tend;

    tstart = tend = clock();
    while (1) {    
        if (pdTRUE != xQueueReceive(gpio_evt_queue, &level, pdMS_TO_TICKS(600))) {
            level = gpio_get_level(KEY_GPIO_PIN);
        }  
        tend = clock();
        if (KEY_STATE_UP == level) {
            tstart = tend;
            if (level != preLevel)
                ledctrl_set_mode(!ledctrl_switch(), ledctrl_mode());
        } else {
            if ((tend - tstart) > (5 * CLOCKS_PER_SEC))
                break;
        }
        preLevel = level;
    }
    global_event_set(GLOBAL_EVENT_RECOVERY_FACTORY);
    tsk_handle = NULL;    
    vTaskDelete(NULL);
}