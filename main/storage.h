#ifndef _STORAGE_H
#define _STORAGE_H

#include <esp_image_format.h>

#ifdef __cplusplus
extern "C" {
#endif

void storage_init();
void storage_deinit();
void storage_delete_all();
bool storage_read_appdesc(esp_app_desc_t *app_info);
bool storage_read_wifi_sta(char ssid[32], char password[64]);
bool storage_write_wifi_sta(const char ssid[32], const char password[64]);
bool storage_read_tz(int32_t *tz);
bool storage_write_tz(int32_t tz);
bool storage_read_fan_duty(uint16_t *duty);
bool storage_write_fan_duty(uint16_t duty);

#ifdef __cplusplus
}
#endif
#endif