#ifndef _MISC_H_
#define _MISC_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_DEVICE_MODEL_LB150) && CONFIG_DEVICE_MODEL_LB150
#define DEVICE_MODEL                      "LB150"
#elif defined(CONFIG_DEVICE_MODEL_LM12) && CONFIG_DEVICE_MODEL_LM12
#define DEVICE_MODEL                      "LM12"
#elif defined(CONFIG_DEVICE_MODEL_LM25) && CONFIG_DEVICE_MODEL_LM25
#define DEVICE_MODEL                      "LM25"
#elif defined(CONFIG_DEVICE_MODEL_LM15) && CONFIG_DEVICE_MODEL_LM15
#define DEVICE_MODEL                      "LM15"
#elif defined(CONFIG_DEVICE_MODEL_LM30) && CONFIG_DEVICE_MODEL_LM30
#define DEVICE_MODEL                      "LM30"
#elif defined(CONFIG_DEVICE_MODEL_LM35) && CONFIG_DEVICE_MODEL_LM35
#define DEVICE_MODEL                      "LM35"
#else
#error "Error, The device model is not defined!!"
#endif
/* Task Core Id */
#define TSK_COREID_KEY                0
#define TSK_COREID_WIFI               0
#define TSK_COREID_WIFISCAN           0
#define TSK_COREID_TEMPCTRL           1
#define TSK_COREID_LEDCTRL            1
#define TSK_COREID_MULTI_RECV         0
#define TSK_COREID_MULTI_HANDLE       1
#define TSK_COREID_WS_ACCEPT          0
#define TSK_COREID_WS_CONNECT         1

/* Task Priority */
#define TSK_PRIO_WIFI                 5
#define TSK_PRIO_KEY                  6
#define TSK_PRIO_WIFISCAN             6
#define TSK_PRIO_WS_ACCEPT            7
#define TSK_PRIO_WS_CONNECT           7
#define TSK_PRIO_MULTI_RECV           8
#define TSK_PRIO_MULTI_HANDLE         8
//CONFIG_WEBSOCKET_SERVER_TASK_PRIORITY 8
#define TSK_PRIO_TEMPCTRL             10
#define TSK_PRIO_LEDCTRL              11


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