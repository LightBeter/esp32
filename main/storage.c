#include <string.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <nvs_flash.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include "storage.h"
#include "temp_ctrl_tsk.h"

#define TAG                         "storage"
#define NVS_WIFI_NAMESPACE          "app_wifi"
#define NVS_TZ_DEFAILT              (-8 * 60)
#define NVS_FAN_DUTY_DEFAILT        (80)

#ifdef __cplusplus
extern "C" {
#endif

void spiffs_init();
void spiffs_deinit();

void storage_init()
{
    esp_err_t ret;
    /* Initialize NVS */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* Initialize spiffs */
    spiffs_init();
}

void storage_deinit()
{
    /* Deinitialize spiffs */
    spiffs_deinit();
    ESP_ERROR_CHECK(nvs_flash_deinit());
}

void spiffs_init()
{
    esp_err_t ret;
    size_t total = 0, used = 0;
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
    }
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS Partition size: total: %d, used: %d", total, used);
    }
}
void spiffs_deinit()
{
    esp_vfs_spiffs_unregister(NULL);
    ESP_LOGI(TAG, "SPIFFS unmounted");
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
    ESP_ERROR_CHECK( nvs_commit(handle) );
    nvs_close(handle);
    return;  
}

void storage_read_tz(int *tz)
{
    esp_err_t err;
    nvs_handle handle;

    err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "%s Error (%s) opening NVS handle!\n", __FUNCTION__, esp_err_to_name(err));
        return;
    }
    err = nvs_get_i32(handle, "time_tz", tz);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) time_tz!\n", __FUNCTION__, esp_err_to_name(err));
        *tz = NVS_TZ_DEFAILT;
    } 
    nvs_close(handle);
    return;
}

void storage_write_tz(int tz)
{
    esp_err_t err;
    nvs_handle handle;
    
    err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "%s Error (%s) opening NVS handle!\n", __FUNCTION__, esp_err_to_name(err));
        return;
    }
    err = nvs_set_i32(handle, "time_tz", tz);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) time_tz!\n", __FUNCTION__, esp_err_to_name(err));
    } 
    ESP_ERROR_CHECK( nvs_commit(handle) );
    nvs_close(handle);
}

void storage_read_fan_duty(uint16_t *duty)
{
    esp_err_t err;
    nvs_handle handle;

    err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "%s Error (%s) opening NVS handle!\n", __FUNCTION__, esp_err_to_name(err));
        return;
    }
    err = nvs_get_u16(handle, "fan_duty", duty);
    if (ESP_OK != err || INVALID_FAN_DUTY(*duty)) {
        ESP_LOGW( TAG, "%s Error (%s) fan duty!\n", __FUNCTION__, esp_err_to_name(err));
        *duty = NVS_FAN_DUTY_DEFAILT;
    } 
    nvs_close(handle);
    return;
}

void storage_write_fan_duty(uint16_t duty)
{
    esp_err_t err;
    nvs_handle handle;
    
    err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "%s Error (%s) opening NVS handle!\n", __FUNCTION__, esp_err_to_name(err));
        return;
    }
    err = nvs_set_u16(handle, "fan_duty", duty);
    if (ESP_OK != err) {
        ESP_LOGW( TAG, "%s Error (%s) fan duty!\n", __FUNCTION__, esp_err_to_name(err));
    } 
    ESP_ERROR_CHECK( nvs_commit(handle) );
    nvs_close(handle);
}

#ifdef __cplusplus
}
#endif