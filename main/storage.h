#ifndef _STORAGE_H
#define _STORAGE_H

#include <esp_image_format.h>
#include <esp_wifi.h>

#ifdef __cplusplus
extern "C" {
#endif

void storage_init();
void storage_deinit();
void storage_read_appdesc(esp_app_desc_t *app_info);
void storage_read_wificonf(wifi_config_t *conf);
void storage_write_wificonf(const wifi_config_t *conf);

#ifdef __cplusplus
}
#endif
#endif