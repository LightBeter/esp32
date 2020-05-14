#include <string.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include "storage.h"

#define TAG                         "app_storage"
#define NVS_WIFI_NAMESPACE          "app_wifi"

#ifdef __cplusplus
extern "C" {
#endif

void storage_init()
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void storage_deinit()
{
    ESP_ERROR_CHECK(nvs_flash_deinit());
}

void storage_read_appdesc(esp_app_desc_t *app_info)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGW(TAG, "Current running partition address 0x%08x", running->address);
    if (esp_ota_get_partition_description(running, app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware(%s) version: %s", app_info->project_name, app_info->version);
    } else {
        ESP_LOGW(TAG, "load firmware version failure");  
    }
}

void storage_read_wificonf(wifi_config_t *conf)
{
    esp_err_t err;
    nvs_handle handle;
    size_t len;

    err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "%s Error (%s) opening NVS handle!\n", __FUNCTION__, esp_err_to_name(err));
        return;
    }
    /* ssid */
    len = sizeof(conf->sta.ssid);
    err = nvs_get_blob(handle, "wifi_ssid", conf->sta.ssid, &len);
    if (ESP_OK != err || len != sizeof(conf->sta.ssid)) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_ssid!\n", __FUNCTION__, esp_err_to_name(err));
        goto default_handle;
    }
    /* password */
    len = sizeof(conf->sta.password);
    err = nvs_get_blob(handle, "wifi_pwd", conf->sta.password, &len);
    if (ESP_OK != err || len != sizeof(conf->sta.password)) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_pwd!\n", __FUNCTION__, esp_err_to_name(err));
        goto default_handle;
    }
    /* bssid_set */
    err = nvs_get_u8(handle, "wifi_bssid_set", (uint8_t*)&conf->sta.bssid_set);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_bssid_set!\n", __FUNCTION__, esp_err_to_name(err));
        goto default_handle;
    }
    /* bssid */
    if (conf->sta.bssid_set) {
        len = sizeof(conf->sta.bssid);
        err = nvs_get_blob(handle, "wifi_bssid", conf->sta.bssid, &len);
        if (ESP_OK != err || len != sizeof(conf->sta.bssid)) {
            ESP_LOGW( TAG, "%s Error (%s) bssid!\n", __FUNCTION__, esp_err_to_name(err));
            goto default_handle;
        }
    }
    nvs_close(handle);
    return;
default_handle:
    nvs_close(handle);
    memset(conf, 0x00, sizeof(wifi_config_t));
    return;    
}

void storage_write_wificonf(const wifi_config_t *conf)
{
    esp_err_t err;
    nvs_handle handle;

    err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "%s Error (%s) opening NVS handle!\n", __FUNCTION__, esp_err_to_name(err));
        return;
    }
    err = nvs_set_blob(handle, "wifi_ssid", conf->sta.ssid, sizeof(conf->sta.ssid));
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_ssid!\n", __FUNCTION__, esp_err_to_name(err));
    }
    err = nvs_set_blob(handle, "wifi_pwd", conf->sta.password, sizeof(conf->sta.password));
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_pwd!\n", __FUNCTION__, esp_err_to_name(err));
    }
    err = nvs_set_u8(handle, "wifi_bssid_set", conf->sta.bssid_set);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_bssid_set!\n", __FUNCTION__, esp_err_to_name(err));
    }
    err = nvs_set_blob(handle, "wifi_bssid", conf->sta.bssid, 6);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) wifi_bssid!\n", __FUNCTION__, esp_err_to_name(err));
    }
    err = nvs_commit(handle);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) nvs_commit!\n", __FUNCTION__, esp_err_to_name(err));
    }
    ESP_ERROR_CHECK( nvs_commit(handle) );
    nvs_close(handle);
    return;  
}

#ifdef __cplusplus
}
#endif