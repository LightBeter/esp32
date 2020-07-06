#include <string.h>
#include <mbedtls/base64.h>
#include <esp_log.h>
#include "device.h"
#include "ota.h"

#define TAG      "OTA"

ota_t  ota; 

void ota_lock()
{
    while( xSemaphoreTake(ota.mutex, portMAX_DELAY) != pdTRUE );
}

void ota_unlock()
{
    while( xSemaphoreGive(ota.mutex) != pdTRUE );
}

void ota_init()
{
    ota.mutex  = xSemaphoreCreateMutex();
    ota.state = OTA_IDLE;
    ota.partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "next application partition[%d] at offset 0x%X", 
             ota.partition->subtype, ota.partition->address);
}

void ota_deinit()
{
    ota_lock();
    if (ota.state == OTA_IN_PROGRESS) {
        esp_ota_end(ota.handle);
        ota.state = OTA_DONE;
    }
    ota_unlock();
}

bool is_ota_client(int client)
{
    bool ret = false;
    ota_lock();
    if (ota.state == OTA_IN_PROGRESS && ota.client == client) 
        ret = true;
    ota_unlock(); 
    return ret;
}

bool ota_begin(int client, size_t img_size)
{
    bool ret = false;
    esp_err_t err;

    ota_lock();

    if (ota.state == OTA_IN_PROGRESS) {
        ESP_LOGE(TAG, "ota is in progress");
        goto exit; 
    } 

    err = esp_ota_begin(ota.partition, img_size, &ota.handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        goto exit; 
    }

    memset(ota.new_fw_ver, 0x00, sizeof(ota.new_fw_ver));
    ota.new_fw_size = img_size;
    ota.new_fw_recv_size = 0;
    ota.client = client;
    ota.state = OTA_IN_PROGRESS;
    ret = true;
exit:
    ota_unlock();
    return ret;
}

bool ota_end(int client)
{
    bool ret = false;
    esp_err_t err;

    ota_lock();
    if (ota.state != OTA_IN_PROGRESS) {
        ESP_LOGE(TAG, "ota no begin");
        goto exit;    
    }

    if (client != ota.client) {
        ESP_LOGE(TAG, "current ota client is %d, no %d", ota.client, client);
        goto exit;    
    }
    
    ota.state = OTA_DONE;
    err = esp_ota_end(ota.handle);
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(ota.partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
            goto exit; 
        }
    } else {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        goto exit; 
    }
    ESP_LOGI(TAG, "update done, bin file size is %d", ota.new_fw_size);
    ret = true;
exit:
    ota_unlock();
    return ret;
}

bool ota_write(int client, const char *data, size_t data_len)
{
    bool ret = false;
    esp_err_t err;
 
    ota_lock();
    
    if (ota.state != OTA_IN_PROGRESS) {
        ESP_LOGE(TAG, "ota no begin");
        goto exit;    
    }

    if (client != ota.client) {
        ESP_LOGE(TAG, "current ota client is %d, no %d", ota.client, client);
        goto exit;    
    }

    if (data_len + ota.new_fw_recv_size > ota.new_fw_size) {
        ESP_LOGE(TAG, "recv data beyond firmware size");
        goto exit;    
    }

    err = esp_ota_write(ota.handle, data, data_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_write failed %d", err);
        goto exit; 
    } 

    if (data_len > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) && ota.new_fw_recv_size == 0) {
        esp_app_desc_t new_app_info;
        memcpy(&new_app_info, &data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
        if (ESP_APP_DESC_MAGIC_WORD == new_app_info.magic_word) {
            memcpy(ota.new_fw_ver, new_app_info.version, sizeof(ota.new_fw_ver));
            ESP_LOGI(TAG, "New firmware version: %s", ota.new_fw_ver);
        } else {
            ESP_LOGI(TAG, "New firmware format abnormal");
        }
    }

    ESP_LOGD(TAG, "Written binary file offset[%d] length[%d]", ota.new_fw_recv_size, data_len);
    ota.new_fw_recv_size += data_len;
    ret = true;
exit:
    ota_unlock();
    return ret;
}