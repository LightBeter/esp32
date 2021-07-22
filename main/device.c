#include <stdio.h>
#include <string.h>
#include <mdns.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include "device.h"
#include "storage.h"
#include "datetime.h"
#include "wifi_tsk.h"
#include "wifi_ap_scan_tsk.h"
#include "wss_tsk.h"
#include "udp_multicast_tsk.h"
#include "temp_ctrl_tsk.h"
#include "led_ctrl_tsk.h"
#include "ota.h"

#define TAG                      "Device"
#define TSK_NAME                 "DeviceTsk"
#define DEVICE_TSK_STACK_SIZE    2048
#define GPIO_CHAN_LED_1          GPIO_NUM_23
#define GPIO_CHAN_LED_2          GPIO_NUM_22
#define LED_OPEN_LEVEL           1
#define LED_CLOSE_LEVEL          0

void mdns_start();
void mdns_stop();
void device_main_tsk(void * parm);

device_type _device;

const esp_app_desc_t* device_appdesc()
{
    return &_device.appdesc;
}

const char* device_serialnum()
{
    return _device.serialnum;
}

const char* device_hostname()
{
    return _device.hostname;
}

void device_set_led(device_led_t led)
{
    _device.curLed = led;
}

void device_set_led_mode(device_led_mode_t mode)
{
    _device.curLedMode = mode;
}

const wifi_config_t* device_wificonf()
{
    return &_device.wificonf; 
}

void device_save_wificonf(const wifi_config_t* conf)
{
    if (0 == memcmp(&_device.wificonf, conf, sizeof(wifi_config_t))) {
        return;
    }
    memcpy(&_device.wificonf, conf, sizeof(wifi_config_t));
    storage_write_wificonf(&_device.wificonf);
}

void device_set_event(const EventBits_t uxBitsToSet)
{
    xEventGroupSetBits(_device.event_group, uxBitsToSet);
}

void mdns_start()
{ 
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set(_device.hostname) );
    ESP_ERROR_CHECK( mdns_instance_name_set(DEVICE_MODEL) );
    ESP_ERROR_CHECK( mdns_service_add(_device.hostname, "_websocket", "_tcp", 80, NULL, 0) );
}

void mdns_stop()
{
    mdns_free();
}

void device_init()
{
    /* Initialize tcp/ip protocol stack */
    esp_netif_init();
    /* Initialize event loop */
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    /* Initialize storage */
    storage_init();
    /* Initialize application desc */
    storage_read_appdesc(&_device.appdesc);
    /* Initialize wifi configure */
    storage_read_wificonf(&_device.wificonf);
    /* Initialize datetime */
    datetime_init();
    /* Initialize ota */
    ota_init();
    /* Initialize Wifi Task */
    wifi_tsk_init();
    /* Initialize Wifi AP Scan Task */
    wifi_ap_scan_tsk_init();
    /* Initialize WSS Task */
    wss_tsk_init();
    /* Initialize Udp Multicast Task */
    udp_multicast_tsk_init();
    /* Initialize Temperature Control Task */
    temp_ctrl_tsk_init();
    /* Initialize Led Control Task */
    led_ctrl_tsk_init();
    /* Initialize hostname */
    uint8_t mac[6];
    ESP_ERROR_CHECK( esp_wifi_get_mac(ESP_IF_WIFI_AP , mac) );
    sprintf( _device.serialnum, "%02X%02X%02X%02X%02X%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
    sprintf( _device.hostname, DEVICE_MODEL "_%02X%02X%02X", mac[3], mac[4], mac[5] );
    /* device event group */
    _device.event_group = xEventGroupCreate();
    /* initial LEDs */
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_CHAN_LED_1) | (1ULL << GPIO_CHAN_LED_2);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_CHAN_LED_1, LED_CLOSE_LEVEL);
    gpio_set_level(GPIO_CHAN_LED_2, LED_CLOSE_LEVEL);
    _device.curLed = DEVICE_LED_1;
    _device.curLedMode = DEVICE_LED_MODE_CLOSE;
}

void  device_run()
{
    xTaskCreate(device_main_tsk, TSK_NAME, DEVICE_TSK_STACK_SIZE, NULL, 3, &_device.handle);
}

void device_main_tsk(void * parm)
{
    //size_t freeHeapSize;
    EventBits_t uxBits;
    gpio_num_t  led;
    uint32_t    level;
    const EventBits_t uxBitsToWaitFor = (DEVICE_REQ_REBOOT_BIT | DEVICE_REQ_RECOVERY_FACTORY_BIT);
    /* Start Task */
    ESP_LOGI( TAG, "Start mDNS." );
    mdns_start();
    ESP_LOGI( TAG, "Start Temperature Control Task." );
    temp_ctrl_tsk_start();
    ESP_LOGI( TAG, "Start Led Control Task." );
    led_ctrl_tsk_start();
    ESP_LOGI( TAG, "Start Wifi Task." );
    wifi_tsk_start();
    ESP_LOGI( TAG, "Start Wifi AP Scan Task." );
    wifi_ap_scan_tsk_start();
    ESP_LOGI( TAG, "Start Websocket Task." );
    wss_tsk_start();
    vTaskDelay(pdMS_TO_TICKS(10000));
    while (1) {
        uxBits = xEventGroupWaitBits( _device.event_group, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000) ); 
        if (uxBits & DEVICE_REQ_RECOVERY_FACTORY_BIT) {
            ESP_LOGI( TAG, "user request recovery factory." );
            break;
        } 
        if (uxBits & DEVICE_REQ_REBOOT_BIT) {
            ESP_LOGI( TAG, "user request reboot." );
            break;
        } 

        if (DEVICE_LED_1 == _device.curLed) {
            gpio_set_level(GPIO_CHAN_LED_2, LED_CLOSE_LEVEL);
            led = GPIO_CHAN_LED_1;
        } else {
            gpio_set_level(GPIO_CHAN_LED_1, LED_CLOSE_LEVEL);
            led = GPIO_CHAN_LED_2;
        }

        if (DEVICE_LED_MODE_OPEN == _device.curLedMode) {
            level = LED_OPEN_LEVEL;              
        } else if (DEVICE_LED_MODE_CLOSE == _device.curLedMode) {
            level = LED_CLOSE_LEVEL;
        } else {
            level = gpio_get_level(led);
            level = ~level & 0x1;
        }
        gpio_set_level(led, level);   
        // freeHeapSize = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        // ESP_LOGI( TAG, "heap free size %zu.", freeHeapSize );
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    /* stop Task */
    ESP_LOGI( TAG, "Stop Udp Multicast Task." );
    udp_multicast_tsk_stop();
    ESP_LOGI( TAG, "Stop Websocket Task." );
    wss_tsk_stop();
    ESP_LOGI( TAG, "Stop Wifi AP Scan Task." );
    wifi_ap_scan_tsk_stop();
    ESP_LOGI( TAG, "Stop Wifi Task." );
    wifi_tsk_stop();
    ESP_LOGI( TAG, "Stop Led Control Task." );
    led_ctrl_tsk_stop();
    ESP_LOGI( TAG, "Stop Temperature Control Task." );
    temp_ctrl_tsk_stop();
    ESP_LOGI( TAG, "Stop mDNS." );
    mdns_stop();
    /* deInitialize */
    ESP_LOGI( TAG, "Deinitialise Udp Multicast Task." );
    udp_multicast_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise Websocket Task." );
    wss_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise Wifi AP Scan Task." );
    wifi_ap_scan_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise Wifi Task." );
    wifi_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise Led Control Task." );
    led_ctrl_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise Temperature Control Task." );
    temp_ctrl_tsk_deinit();
    ESP_LOGI( TAG, "Deinitialise ota." );
    ota_deinit();
    ESP_LOGI( TAG, "Deinitialise datetime." );
    datetime_deinit();
    ESP_LOGI( TAG, "Deinitialise Storage." );
    storage_deinit();
    /* reboot */
    ESP_LOGI( TAG, "Device reboot." );
    esp_restart();
    _device.handle = NULL;
    vTaskDelete(NULL);
}


