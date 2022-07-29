
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_log.h>
#include "tsk_wifi.h"
#include "tsk_wifi_scan.h"
#include "tsk_ws.h"
#include "tsk_udp_multicast.h"
#include "tsk_fanctrl.h"
#include "tsk_ledctrl.h"
#include "tsk_key.h"
#include "storage.h"
#include "datetime.h"
#include "ota.h"
#include "misc.h"

#define TAG  "app_main"

void recovery_factory_configure()
{
    ESP_LOGI( TAG, "recovery factory configure" );
    storage_delete_all();
    indicator_set_flash(false);
    indicator_set_state(true);
    for (int index = 0; index < 3; index++) {
        indicator_set_chan(INDICATOR_CHAN_MANUAL);
        indicator_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
        indicator_set_chan(INDICATOR_CHAN_AUTO);
        indicator_update();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    indicator_set_state(false);
    indicator_update();
}

void app_main() {
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    storage_init();
    datetime_init();
    tsk_wifi_init();
    tsk_wifi_scan_init();
    tsk_ws_init();
    tsk_udp_multicast_init();
    tsk_ledctrl_init();
    tsk_fanctrl_init();
    tsk_key_init();
    app_version_init();
    serialnum_init();
    hostname_init();
    indicator_init();
    global_event_init();
    ota_init();
    /* Start Service & Task */
    //mdns_start();
    tsk_ledctrl_start();
    tsk_fanctrl_start();
    tsk_wifi_start();
    tsk_wifi_scan_start();
    tsk_ws_start();
    tsk_key_start();
    while (1) {
        uxBits = global_event_wait( 0xFFFF, true, false, pdMS_TO_TICKS(1000) );
        if (uxBits & GLOBAL_EVENT_RECOVERY_FACTORY) {
            ESP_LOGI( TAG, "user request recovery factory." );
            break;
        } else if (uxBits & GLOBAL_EVENT_REBOOT) {
            ESP_LOGI( TAG, "user request reboot." );
            break;
        } 
        indicator_update();
    }
    tsk_key_stop();
    if (uxBits & GLOBAL_EVENT_REBOOT)
        vTaskDelay(pdMS_TO_TICKS(2000));
    /* Stop Service & Task */
    tsk_udp_multicast_stop();
    tsk_ws_stop();
    tsk_wifi_scan_stop();
    tsk_wifi_stop();
    tsk_fanctrl_stop();
    tsk_ledctrl_stop();
    //mdns_stop();
    /* Deinitialise */
    tsk_key_deinit();
    tsk_udp_multicast_deinit();
    tsk_ws_deinit();
    tsk_deinit_wifi_scan();
    tsk_wifi_deinit();
    tsk_fanctrl_deinit();
    tsk_ledctrl_deinit();
    ota_deinit();
    datetime_deinit();
    if (uxBits & GLOBAL_EVENT_RECOVERY_FACTORY)
        recovery_factory_configure();
    storage_deinit();
    /* Reboot */    
    esp_restart();
}