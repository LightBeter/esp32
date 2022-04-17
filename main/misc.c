#include <string.h>
#include <driver/gpio.h>
#include <esp_wifi.h>
#include <mdns.h>
#include <esp_log.h>
#include "misc.h"
#include "storage.h"

#define TAG                     "MISC"
#define DEVICE_SERIAL_NUM_LEN   12  
#define LED_1_GPIO_PIN          GPIO_NUM_23
#define LED_2_GPIO_PIN          GPIO_NUM_22
#define LED_3_GPIO_PIN          GPIO_NUM_5
#define LED_OPEN_LEVEL          1
#define LED_CLOSE_LEVEL         0

static char _version[32]  = "unknown";           /*!< Application version */
static char _serialnum[DEVICE_SERIAL_NUM_LEN + 1];
static char _hostname[32]; 
static EventGroupHandle_t  global_event_group; 
static Indicator_chen_t  led_chan = INDICATOR_CHAN_MANUAL;
static bool led_state = false;
static bool led_flash = false;
static uint32_t led_level = LED_CLOSE_LEVEL;

void app_version_init()
{
    esp_app_desc_t app_info;
    if (storage_read_appdesc(&app_info)) {
        strcpy(_version, app_info.version);
    }
}

const char* app_version()
{
    return _version;
}

void serialnum_init() 
{
    uint8_t mac[6];
    ESP_ERROR_CHECK( esp_wifi_get_mac(ESP_IF_WIFI_AP , mac) );
    sprintf( _serialnum, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
}

const char* serialnum()
{
    return _serialnum;
}

void hostname_init()
{
    uint8_t mac[6];
    ESP_ERROR_CHECK( esp_wifi_get_mac(ESP_IF_WIFI_AP , mac) );
    #if defined(CONFIG_DEVICE_MODEL_LB150) && CONFIG_DEVICE_MODEL_LB150
        sprintf( _hostname, DEVICE_MODEL "_%02X%02X%02X", mac[3], mac[4], mac[5] );
    #else
        sprintf( _hostname, DEVICE_MODEL "-%02X%02X%02X", mac[3], mac[4], mac[5] );
    #endif
}

const char* hostname()
{
    return _hostname;
}

void mdns_start()
{ 
    ESP_LOGI( TAG, "Start mDNS" );
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set(hostname()) );
    ESP_ERROR_CHECK( mdns_instance_name_set(DEVICE_MODEL) );
    ESP_ERROR_CHECK( mdns_service_add(hostname(), "_websocket", "_tcp", 80, NULL, 0) );
}

void mdns_stop()
{
    ESP_LOGI( TAG, "Stop mDNS" );
    mdns_free();
}

void global_event_init()
{
    global_event_group = xEventGroupCreate();
}

void global_event_set(const EventBits_t uxBitsToSet)
{
    xEventGroupSetBits(global_event_group, uxBitsToSet);
}

EventBits_t global_event_wait(const EventBits_t uxBitsToWaitFor, 
                              const BaseType_t xClearOnExit,
                              const BaseType_t xWaitForAllBits,
                              TickType_t xTicksToWait)
{
    return xEventGroupWaitBits( global_event_group, uxBitsToWaitFor, xClearOnExit, xWaitForAllBits, xTicksToWait ); 
}

void indicator_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED_1_GPIO_PIN) | (1ULL << LED_2_GPIO_PIN) | (1ULL << LED_3_GPIO_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(LED_1_GPIO_PIN, LED_CLOSE_LEVEL);
    gpio_set_level(LED_2_GPIO_PIN, LED_CLOSE_LEVEL);
}

void indicator_set_chan(Indicator_chen_t chan)
{
    led_chan = chan;
}

void indicator_set_flash(bool flash)
{
    led_flash = flash;
}

void indicator_set_state(bool state)
{
    led_state = state;
}

void indicator_update()
{    
    static uint8_t heartbeat = 0;
    if (true == led_flash) {
        led_level = ~led_level & 0x1;
    } else if (true == led_state) {
        led_level = LED_OPEN_LEVEL;              
    } else {
        led_level = LED_CLOSE_LEVEL;
    }

    if (INDICATOR_CHAN_MANUAL == led_chan) {
        gpio_set_level(LED_2_GPIO_PIN, LED_CLOSE_LEVEL);
        gpio_set_level(LED_1_GPIO_PIN, led_level);
    } else {
        gpio_set_level(LED_1_GPIO_PIN, LED_CLOSE_LEVEL);
        gpio_set_level(LED_2_GPIO_PIN, led_level);
    }

    heartbeat = ~heartbeat & 0x1;
    gpio_set_level(LED_3_GPIO_PIN, heartbeat);
}