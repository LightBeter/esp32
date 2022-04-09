
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_log.h>
#include "tsk_wifi.h"
#include "tsk_wifi_scan.h"
#include "tsk_wss.h"
#include "tsk_udp_multicast.h"
#include "tsk_tempctrl.h"
#include "tsk_ledctrl.h"
#include "storage.h"
#include "datetime.h"
#include "ota.h"
#include "misc.h"

#define TAG  "app_main"

void app_main() {
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    storage_init();
    datetime_init();
    tsk_wifi_init();
    tsk_init_wifi_scan();
    tsk_wss_init();
    tsk_udp_multicast_init();
    tsk_tempctrl_init();
    tsk_ledctrl_init();
    app_version_init();
    serialnum_init();
    hostname_init();
    led_init();
    global_event_init();
    ota_init();
    /* Start Service & Task */
    mdns_start();
    tsk_tempctrl_start();
    tsk_ledctrl_start();
    tsk_wifi_start();
    tsk_start_wifi_scan();
    tsk_wss_start();

    EventBits_t uxBits;
    while (1) {
        uxBits = global_event_wait( 0xFFFF, true, false, pdMS_TO_TICKS(1000) ); 
        if (uxBits & GLOBAL_EVENT_REBOOT) {
            ESP_LOGI( TAG, "user request reboot." );
            break;
        } 
        led_update();
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    /* Stop Service & Task */
    tsk_udp_multicast_stop();
    tsk_wss_stop();
    tsk_stop_wifi_scan();
    tsk_wifi_stop();
    tsk_ledctrl_stop();
    tsk_tempctrl_stop();
    mdns_stop();
    /* Deinitialise */
    tsk_udp_multicast_deinit();
    tsk_wss_deinit();
    tsk_deinit_wifi_scan();
    tsk_wifi_deinit();
    tsk_ledctrl_deinit();
    tsk_tempctrl_deinit();
    ota_deinit();
    datetime_deinit();
    storage_deinit();
    /* Reboot */    
    esp_restart();
}