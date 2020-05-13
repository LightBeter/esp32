
#include <freertos/FreeRTOS.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include "device.h"

#define TAG  "app_main"

void app_main() {
    device_init();
    device_run();
}