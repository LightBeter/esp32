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

#define TSK_PRIORITY_LEDCTRL           8
#define TSK_PRIORITY_TEMPCTRL          8
#define TSK_PRIORITY_WIFI              7
#define TSK_PRIORITY_WIFISCAN          7
#define TSK_PRIORITY_WSS               6
#define TSK_PRIORITY_MULTICAST         5
#define TSK_PRIORITY_KEY               2

#define GLOBAL_EVENT_REBOOT               BIT0
#define GLOBAL_EVENT_RECOVERY_FACTORY     BIT1

typedef enum {
   INDICATOR_CHAN_MANUAL = 0,
   INDICATOR_CHAN_AUTO,
} Indicator_chen_t;

void app_version_init();
const char* app_version();

void serialnum_init();
const char* serialnum();

void hostname_init();
const char* hostname();

void mdns_start();
void mdns_stop();

void indicator_init();
void indicator_update();
void indicator_set_chan(Indicator_chen_t chan);
void indicator_set_flash(bool flash);
void indicator_set_state(bool state);

void global_event_init();
void global_event_set(const EventBits_t uxBitsToSet);
EventBits_t global_event_wait(const EventBits_t uxBitsToWaitFor, 
                              const BaseType_t xClearOnExit,
                              const BaseType_t xWaitForAllBits,
                              TickType_t xTicksToWait);

void hearbeat_update();

#ifdef __cplusplus
}
#endif
#endif