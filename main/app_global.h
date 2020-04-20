#ifndef _APP_GLOBAL_H
#define _APP_GLOBAL_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#define APP_REQ_REBOOT_BIT      BIT0

#define APP_TSK_STACK_SIZE      2048

typedef enum {
    APP_TSK_WIFI_SC = 0x00,
    APP_TSK_COUNT
} app_tsk_type;

typedef enum {
    device_wifi_state_idle = 0x00,
    device_wifi_state_sc,
    device_wifi_state_sta,
    device_wifi_state_ap,
    device_wifi_state_stop,
} device_wifi_state;

typedef struct {
    wifi_mode_t         mode;
    wifi_init_config_t  init_cfg;
    wifi_config_t       cfg;
    device_wifi_state   state;
    EventGroupHandle_t  event_group;
} device_wifi_type;

typedef struct {
    httpd_handle_t      handle;
    httpd_config_t      cfg;
} Webserver_type;

extern void initialise_wifi(void);
extern void deinitialise_wifi(void);
extern void start_wifi_smartconfig();
extern void start_wifi_sta();
extern void start_wifi_ap();
extern void stop_wifi();

extern void initialise_webserver(void);
extern void deinitialise_webserver(void);
extern void start_webserver(void);
extern void stop_webserver(void);
extern esp_err_t webserver_register_handlers(httpd_handle_t);

extern void wifi_sc_task(void * parm);

extern EventGroupHandle_t      _app_event_group;
extern TaskHandle_t            _app_task_handles[APP_TSK_COUNT];
extern device_wifi_type        _device_wifi;
extern Webserver_type          _webserver;

#endif