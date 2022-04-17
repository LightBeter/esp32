#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include <esp_log.h>

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
    uint8_t level = gpio_get_level(KEY_GPIO_PIN) & 0x1;
    xQueueSendFromISR(gpio_evt_queue, &level, NULL);
}

void tsk_key_init()
{
    gpio_config_t io_conf;
    ESP_LOGI( TAG, "initialize" );
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << KEY_GPIO_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_evt_queue = xQueueCreate(12, sizeof(uint8_t));
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
        xTaskCreate(key_tsk, TAG, TSK_STACK_SIZE, NULL, 5, &tsk_handle);
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
    uint8_t hold_time = 0;
    uint8_t state = KEY_STATE_UP, level;

    while(hold_time < 5) {
        if(xQueueReceive(gpio_evt_queue, &level, pdMS_TO_TICKS(1000))) {
            hold_time = 0;
            if (level == state)
                continue;

            state = level;
            if (KEY_STATE_UP == state) {
                /* key up */
                ledctrl_set_mode(!ledctrl_switch(), ledctrl_mode());
            } 
        } else {
            hold_time = (KEY_STATE_DOWN == state) ? (hold_time + 1) : 0;
        }
    }

    global_event_set(GLOBAL_EVENT_RECOVERY_FACTORY);
    tsk_handle = NULL;    
    vTaskDelete(NULL);
}