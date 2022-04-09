#ifndef _MISC_H_
#define _MISC_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_DEVICE_MODEL_LB150) && CONFIG_DEVICE_MODEL_LB150
#define DEVICE_MODEL                      "LB150"
#elif defined(CONFIG_DEVICE_MODEL_LM25) && CONFIG_DEVICE_MODEL_LM25
#define DEVICE_MODEL                      "LM25"
#elif defined(CONFIG_DEVICE_MODEL_LM15) && CONFIG_DEVICE_MODEL_LM15
#define DEVICE_MODEL                      "LM15"
#else
#error "Error, The device model is not defined!!"
#endif

#define GLOBAL_EVENT_REBOOT               BIT0
#define GLOBAL_EVENT_RECOVERY_FACTORY     BIT1

typedef enum {
   INDICATOR_CHAN_MANUAL = 0,
   INDICATOR_CHAN_AUTO,
} Indicator_chen_t;

typedef enum {
    INDICATOR_MODE_OPEN = 0,
    INDICATOR_MODE_CLOSE,
    INDICATOR_MODE_FLASH,
} Indicator_mode_t;

void app_version_init();
const char* app_version();

void serialnum_init();
const char* serialnum();

void hostname_init();
const char* hostname();

void mdns_start();
void mdns_stop();

void led_init();
void led_update();
void led_set_chan(Indicator_chen_t chan);
void led_set_mode(Indicator_mode_t mode);



void global_event_init();
void global_event_set(const EventBits_t uxBitsToSet);
EventBits_t global_event_wait(const EventBits_t uxBitsToWaitFor, 
                              const BaseType_t xClearOnExit,
                              const BaseType_t xWaitForAllBits,
                              TickType_t xTicksToWait);

#ifdef __cplusplus
}
#endif
#endif