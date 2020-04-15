/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "app_global.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#define TAG   "app main"

EventGroupHandle_t  _app_event_group;
TaskHandle_t        _app_task_handles[APP_TSK_COUNT];

void app_init()
{
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
    /* Initialize wifi mode */
    initialise_wifi();
    /* Initialize application event group */
    _app_event_group = xEventGroupCreate();
}

void app_deinit()
{
    /* deInitialize application event group */
    vEventGroupDelete(_app_event_group);
    /* deInitialize wifi mode */
    deinitialise_wifi();
    /* deinitial event loop */
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    /* deInitialize NVS */
    ESP_ERROR_CHECK(nvs_flash_deinit());
}

void app_start_tsk()
{
    xTaskCreate(wifi_sc_task, "wifi_sc_task", APP_TSK_STACK_SIZE, NULL, 3, &_app_task_handles[APP_TSK_WIFI_SC]);
}

void app_stop_tsk()
{
    for(int i = 0; i < APP_TSK_COUNT; i++){
        vTaskDelete(_app_task_handles[i]);   //Delete user task first (prevents the resetting of an unsubscribed task)
    }
}

void app_main()
{
    /* Print chip information */
    // esp_chip_info_t chip_info;
    // esp_chip_info(&chip_info);
    // ESP_LOGI(TAG, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
    //         chip_info.cores,
    //         (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
    //         (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    // ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);
    // ESP_LOGI(TAG, "%dMB %s flash", spi_flash_get_chip_size() / (1024 * 1024),
    //         (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    /* initialize application */
    app_init();
    /* start application task */
    app_start_tsk();
    // /* wait exit */
    // xEventGroupWaitBits(_app_event_group, APP_REQ_REBOOT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    // /* stop application task */
    // app_stop_tsk();
    // /* deinitialize application */
    // app_deinit();
    // ESP_LOGI(TAG, "Restarting now");
    // fflush(stdout);
    // esp_restart();
}
