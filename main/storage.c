#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_spiffs.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include "storage.h"

#define TAG                     "storage"
#define SPIFFS_PARTITION_LABLE  "app_data"
#define NVS_DATA_NAMESPACE      "app_wifi"
#define STORAGE_OPEN_R()        storage_open(NVS_READONLY)
#define STORAGE_OPEN_RW()       storage_open(NVS_READWRITE)
#define STORAGE_CLOSE()         storage_close(&storage_handle)

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t storage_open(nvs_open_mode_t open_mode);
esp_err_t storage_close();
esp_err_t storage_get_u16(const char *key, uint16_t *value);
esp_err_t storage_set_u16(const char *key, uint16_t value);
esp_err_t storage_get_i32(const char *key, int32_t *value);
esp_err_t storage_set_i32(const char *key, int32_t value);
esp_err_t storage_get_blob(const char *key, void *out_value, size_t maxlen, size_t *outlen);
esp_err_t storage_set_blob(const char *key, const void *value, size_t len);

static SemaphoreHandle_t storage_mutex = NULL;
static nvs_handle        storage_handle = 0;

void storage_lock()
{
    while( xSemaphoreTake(storage_mutex, portMAX_DELAY) != pdTRUE );
}

void storage_unlock()
{
    while( xSemaphoreGive(storage_mutex) != pdTRUE );
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

void storage_init()
{
    esp_err_t ret;
    ESP_LOGI( TAG, "initialize" );
    storage_mutex  = xSemaphoreCreateMutex();
    /* Initialize NVS */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    spiffs_init();   
}

void storage_deinit()
{
    ESP_LOGI( TAG, "deinitialize" );
    vSemaphoreDelete(storage_mutex);
    spiffs_deinit();
    ESP_ERROR_CHECK(nvs_flash_deinit());
}

void storage_delete_all()
{
    esp_err_t err;
    /* format nvs */
    
#if 0   
    err = STORAGE_OPEN_RW();
    if (ESP_OK == err) {    
        nvs_erase_key(storage_handle, "wifi_ssid");
        nvs_erase_key(storage_handle, "wifi_pwd");
        nvs_erase_key(storage_handle, "time_tz");
        nvs_erase_key(storage_handle, "fan_duty");
        nvs_commit(storage_handle);
        STORAGE_CLOSE();
    }
#else
    err = nvs_flash_erase();
    if (ESP_OK != err) {
        ESP_LOGE( TAG, "nvs erase failure %d", err);
    } else {
        nvs_flash_init();
    }
#endif
    /* format spiffs */
    err = esp_spiffs_format(NULL);
    if (ESP_OK != err) {
        ESP_LOGE( TAG, "spiffs format failure %d", err);
    } 
}


bool storage_read_appdesc(esp_app_desc_t *app_info)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    //ESP_LOGI( TAG, "Current running partition address 0x%08x", running->address );
    if (esp_ota_get_partition_description(running, app_info) == ESP_OK) {
        ESP_LOGI( TAG, "Running firmware(%s) version: %s", app_info->project_name, app_info->version );
        return true;
    } else {
        ESP_LOGW( TAG, "load firmware version failure" );  
        return false;
    }
}

esp_err_t storage_open(nvs_open_mode_t open_mode)
{
    storage_lock();   
    esp_err_t err = nvs_open(NVS_DATA_NAMESPACE, open_mode, &storage_handle);
    if (err != ESP_OK) {
        ESP_LOGW( TAG, "Error (%s) opening NVS %s(%s)!", NVS_DATA_NAMESPACE, NVS_READONLY == open_mode ? "r": "rw", esp_err_to_name(err) );
        storage_unlock();
    } 
    return err;
}

esp_err_t storage_close()
{
    nvs_close(storage_handle);
    storage_unlock();
    return ESP_OK;
}

esp_err_t storage_get_u16(const char *key, uint16_t *value)
{
    esp_err_t err = nvs_get_u16(storage_handle, key, value);
    if (ESP_OK != err) 
        ESP_LOGW( TAG, "get key(%s) value Error (%s)!", key, esp_err_to_name(err) ); 
    return err;
}

esp_err_t storage_set_u16(const char *key, uint16_t value)
{
    esp_err_t err = nvs_set_u16(storage_handle, key, value);
    if (ESP_OK != err) 
        ESP_LOGW( TAG, "set key(%s) value Error (%s)!", key, esp_err_to_name(err) ); 
    return err;
}

esp_err_t storage_get_i32(const char *key, int32_t *value)
{
    esp_err_t err = nvs_get_i32(storage_handle, key, value);
    if (ESP_OK != err) 
        ESP_LOGW( TAG, "get key(%s) value Error (%s)!", key, esp_err_to_name(err) ); 
    return err;
}

esp_err_t storage_set_i32(const char *key, int32_t value)
{
    esp_err_t err = nvs_set_i32(storage_handle, key, value);
    if (ESP_OK != err) 
        ESP_LOGW( TAG, "set key(%s) value Error (%s)!", key, esp_err_to_name(err) ); 
    return err;
}

esp_err_t storage_get_blob(const char *key, void *out_value, size_t maxlen, size_t *outlen)
{
    size_t len = maxlen;
    esp_err_t err = nvs_get_blob(storage_handle, key, out_value, &len);
    if (ESP_OK != err) 
        ESP_LOGW( TAG, "get key(%s) value Error (%s)!", key, esp_err_to_name(err) );
    return err;    
}

esp_err_t storage_set_blob(const char *key, const void *value, size_t len)
{
    esp_err_t err = nvs_set_blob(storage_handle, key, value, len);
    if (ESP_OK != err) 
        ESP_LOGW( TAG, "set key(%s) value Error (%s)!", key, esp_err_to_name(err) );
    return err;
}

bool storage_read_wifi_sta(char ssid[32], char password[64])
{
    esp_err_t err;
    size_t len = 0;

    err = STORAGE_OPEN_R();
    if (err != ESP_OK)
        return false;

    err = storage_get_blob("wifi_ssid", ssid, 32, &len);
    if (ESP_OK != err)
        goto exit;

    err = storage_get_blob("wifi_pwd", password, 64, &len);
    if (ESP_OK != err)
        goto exit;

exit:    
    STORAGE_CLOSE();
    return ESP_OK == err;
}

bool storage_write_wifi_sta(const char ssid[32], const char password[64])
{
    esp_err_t err;

    if (ESP_OK != STORAGE_OPEN_RW())
        return false;

    err = storage_set_blob("wifi_ssid", ssid, 32);
    if (ESP_OK != err)
        goto exit;

    err = storage_set_blob("wifi_pwd", password, 64);
    if (ESP_OK != err)
        goto exit;

exit:  
    if (ESP_OK == err) 
        ESP_ERROR_CHECK(nvs_commit(storage_handle));    
    STORAGE_CLOSE();
    return ESP_OK == err;
}

bool storage_read_tz(int32_t *tz)
{
    esp_err_t err;

    if (ESP_OK != STORAGE_OPEN_R())
        return false;

    err = storage_get_i32("time_tz", tz);
    if (ESP_OK != err)
        goto exit;

exit:    
    STORAGE_CLOSE();
    return ESP_OK == err; 
}

bool storage_write_tz(int32_t tz)
{
    esp_err_t err;

    if (ESP_OK != STORAGE_OPEN_RW())
        return false;

    err = storage_set_i32("time_tz", tz);
    if (ESP_OK != err)
        goto exit;

exit:    
    if (ESP_OK == err) 
        ESP_ERROR_CHECK(nvs_commit(storage_handle));  
    STORAGE_CLOSE();
    return ESP_OK == err;
}

bool storage_read_fan_duty(uint16_t *duty)
{
    esp_err_t err;

    if (ESP_OK != STORAGE_OPEN_R())
        return false;
        
    err = storage_get_u16("fan_duty", duty);
    if (ESP_OK != err)
        goto exit;

exit:    
    STORAGE_CLOSE();
    return ESP_OK == err ;
}

bool storage_write_fan_duty(uint16_t duty)
{
    esp_err_t err;

    if (ESP_OK != STORAGE_OPEN_RW())
        return false;

    err = storage_set_u16("fan_duty", duty);
    if (ESP_OK != err)
        goto exit;

exit:    
    if (ESP_OK == err) 
        ESP_ERROR_CHECK(nvs_commit(storage_handle));  
    STORAGE_CLOSE();
    return ESP_OK == err;
}

#ifdef __cplusplus
}
#endif