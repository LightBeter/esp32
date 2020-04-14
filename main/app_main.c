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

#define TAG   "app main"

EventGroupHandle_t  _app_event_group;
wifi_init_config_t _wifi_init_cfg;
wifi_config_t      _wifi_cfg = {
    .sta = {
        .ssid = "AMware",
        .password = "Thd242707am"
    },
};

void app_init()
{
    /* initial application event group */
    _app_event_group = xEventGroupCreate();
    /* initial tcp/ip protocol stack */
    tcpip_adapter_init();
    /* initial event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* initial wifi sta mode */
    wifi_init_sta();
}

void app_deinit()
{
    /* deinitial wifi sta mode */
    wifi_deinit_sta();
    /* deinitial event loop */
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    /* deinitial  application event group */
    vEventGroupDelete(_app_event_group);
}

void app_main()
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);
    ESP_LOGI(TAG, "%dMB %s flash", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    /* application initial */
    app_init();
    /* wait exit */
    xEventGroupWaitBits(_app_event_group, APP_REQ_REBOOT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    app_deinit();
    ESP_LOGI(TAG, "Restarting now");
    fflush(stdout);
    esp_restart();
}
