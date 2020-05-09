#ifndef _APP_MISC_H
#define _APP_MISC_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_app_format.h>

#define APP_REQ_REBOOT_BIT               BIT0
#define APP_REQ_RECOVERY_FACTORY_BIT     BIT1

#define APP_TSK_STACK_SIZE               2048

typedef enum {
    APP_TSK_WIFI_SC = 0x00,
    APP_TSK_COUNT
} app_tsk_type;

void load_app_desc(void);
void initialise_device(void);
void deinitialise_device(void);
void initialise_mdns(void);
void deinitialise_mdns(void);
void app_start_tsk();
void app_stop_tsk();

extern esp_app_desc_t          _running_app_info;
extern char*                   _device_hostname;
extern EventGroupHandle_t      _app_event_group;
extern TaskHandle_t            _app_task_handles[APP_TSK_COUNT];

#endif