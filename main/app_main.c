
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include "app_misc.h"
#include "app_wifi.h"
#include "app_udp.h"
#include "app_webserver.h"

#define TAG  "app_main"

void app_init()
{
    /* load application describe */
    load_app_desc();
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* Initialize event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Initialize tcp/ip protocol stack */
    tcpip_adapter_init();
    /* initialize device */
    initialise_device();
    /* Initialize mdns */
    initialise_mdns();
    /* Initialize wifi */
    initialise_wifi();
    /* Initialize udp */
    initialise_udp();
    /* Initialize webserver */
    initialise_webserver();
    /* Initialize application event group */
    _app_event_group = xEventGroupCreate();
}

void app_deinit()
{
    /* deInitialize application event group */
    vEventGroupDelete(_app_event_group);
    /* deInitialize webserver */
    deinitialise_webserver();
    /* deInitialize udp */
    deinitialise_udp();
    /* deInitialize wifi */
    deinitialise_wifi();
    /* deInitialize mdns */
    deinitialise_mdns();
    /* deInitialise device */
    deinitialise_device();
    /* deinitial event loop */
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    /* deInitialize NVS */
    ESP_ERROR_CHECK(nvs_flash_deinit());
}

void app_main()
{
    EventBits_t uxBits;
    const EventBits_t uxBitsToWaitFor =  (APP_REQ_REBOOT_BIT | APP_REQ_RECOVERY_FACTORY_BIT);

    ESP_LOGI( TAG, "device start." );
    /* initialize application */
    app_init();
    /* start application task */
    app_start_tsk();
    while (1) {
        uxBits = xEventGroupWaitBits(_app_event_group, uxBitsToWaitFor, true, false, pdMS_TO_TICKS(1000)); 
        if (uxBits & APP_REQ_RECOVERY_FACTORY_BIT) {
            ESP_LOGI( TAG, "user request recovery factory." );
            break;
        } 
        
        if (uxBits & APP_REQ_REBOOT_BIT) {
            ESP_LOGI( TAG, "user request reboot." );
            break;
        } 
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    /* stop tsk */
    ESP_LOGI( TAG, "stop task." );
    app_stop_tsk();
    /* deinitialize application */
    ESP_LOGI( TAG, "application deinitialize." );
    app_deinit();
    ESP_LOGI( TAG, "device reboot." );
    esp_restart();
}