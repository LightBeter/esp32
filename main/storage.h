#ifndef _STORAGE_H
#define _STORAGE_H

#include <esp_image_format.h>
#include <esp_wifi.h>
#include <freertos/semphr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct storage_tag {
    SemaphoreHandle_t   mutex;
} storage_t;

void storage_init();
void storage_deinit();
void storage_read_appdesc(esp_app_desc_t *app_info);
void storage_read_wificonf(wifi_config_t *conf);
void storage_write_wificonf(const wifi_config_t *conf);
void storage_read_tz(int *tz);
void storage_write_tz(int tz);
void storage_read_fan_duty(uint16_t *duty);
void storage_write_fan_duty(uint16_t duty);

#ifdef __cplusplus
}
#endif
#endif