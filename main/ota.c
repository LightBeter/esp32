#include <string.h>
#include <mbedtls/base64.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include "ota.h"

#define TAG                 "OTA"
#define CIPHER_CODE_LEN     128

void ota_decode(const char* cipherText, uint32_t len, uint32_t key, char* cleanText);
void ota_lock();
void ota_unlock();

static esp_ota_handle_t        handle;
static SemaphoreHandle_t       mutex = NULL;
static const esp_partition_t  *partition = NULL;
static ota_state_t ota_state = OTA_IDLE;
static int       ota_client = 0;
static uint32_t  ota_total_size = 0;
static uint32_t  ota_recv_size = 0;

void ota_init()
{
    ESP_LOGI( TAG, "initialize" );
    mutex  = xSemaphoreCreateMutex();
    partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "partition[%d] offset(0x%X) size(%zu)", partition->subtype, partition->address, partition->size);
}

void ota_deinit()
{
    ESP_LOGI( TAG, "deinitialize" );
    ota_lock();
    if (ota_state == OTA_IN_PROGRESS) {
        esp_ota_end(handle);
        ota_state = OTA_DONE;
    }
    ota_unlock();
}

bool is_ota_client(int client)
{
    bool ret;
    ota_lock();
    ret = (ota_state == OTA_IN_PROGRESS) && (ota_client == client);
    ota_unlock(); 
    return ret;
}

bool ota_begin(int client, uint32_t img_size)
{
    bool ret = false;
    esp_err_t err;

    ota_lock();

    if (ota_state == OTA_IN_PROGRESS) {
        ESP_LOGE(TAG, "ota is in progress");
        goto exit; 
    } 

    err = esp_ota_begin(partition, img_size, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        goto exit; 
    }

    ota_total_size = img_size;
    ota_recv_size = 0;
    ota_client = client;
    ota_state = OTA_IN_PROGRESS;
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
    if (ota_state != OTA_IN_PROGRESS) {
        ESP_LOGE(TAG, "ota no begin");
        goto exit;    
    }

    if (client != ota_client) {
        ESP_LOGE(TAG, "current ota client is %d, no %d", ota_client, client);
        goto exit;    
    }
    
    ota_state = OTA_DONE;
    err = esp_ota_end(handle);
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
            goto exit; 
        }
    } else {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        goto exit; 
    }
    ESP_LOGI(TAG, "update done, bin file size is %d", ota_total_size);
    ret = true;
exit:
    ota_unlock();
    return ret;
}

bool ota_write(int client, char *data, uint32_t data_len)
{
    bool ret = false;
    esp_err_t err;
 
    ota_lock();
    
    if (ota_state != OTA_IN_PROGRESS) {
        ESP_LOGE(TAG, "ota no begin");
        goto exit;    
    }

    if (client != ota_client) {
        ESP_LOGE(TAG, "current ota client is %d, no %d", ota_client, client);
        goto exit;    
    }

    if (data_len + ota_recv_size > ota_total_size) {
        ESP_LOGE(TAG, "recv data beyond firmware size");
        goto exit;    
    }

    ota_decode(data, data_len, ota_recv_size, data);
    err = esp_ota_write(handle, data, data_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_write failed %d", err);
        goto exit; 
    } 

    if (data_len > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) && ota_recv_size == 0) {
        esp_app_desc_t new_app_info;
        memcpy(&new_app_info, &data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
        if (ESP_APP_DESC_MAGIC_WORD == new_app_info.magic_word) {
            ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);
        } else {
            ESP_LOGI(TAG, "New firmware format abnormal");
        }
    }

    ESP_LOGD(TAG, "Written binary file offset[%d] length[%d]", ota_recv_size, data_len);
    ota_recv_size += data_len;
    ret = true;
exit:
    ota_unlock();
    return ret;
}

void ota_decode(const char* cipherText, uint32_t len, uint32_t key, char* cleanText) 
{
    const uint8_t  cipher_code[CIPHER_CODE_LEN] = {
        0x40, 0x5b, 0xca, 0x4c, 0xba, 0xc8, 0xa1, 0x0d,
        0x26, 0x72, 0xe5, 0xd1, 0x44, 0x94, 0x27, 0x18,
        0xb5, 0xcb, 0xb4, 0x5b, 0xc6, 0x07, 0x40, 0x4a,
        0xfa, 0xeb, 0xfb, 0x86, 0x79, 0xd4, 0x1e, 0xd8,
        0x3d, 0x27, 0x43, 0x7e, 0x2b, 0x6b, 0x0f, 0xda,
        0xc3, 0xe3, 0xcc, 0xa4, 0x19, 0x2d, 0xb5, 0x67,
        0x24, 0x5d, 0x4b, 0x4e, 0x6e, 0x86, 0x1c, 0xdd,
        0xe3, 0x75, 0x16, 0xd4, 0x1e, 0xeb, 0x2c, 0xee,
        0x32, 0x40, 0x79, 0x9d, 0xc2, 0xe2, 0x34, 0x68,
        0x7d, 0x70, 0xb5, 0xf6, 0x65, 0x44, 0x13, 0xa1,
        0x49, 0xa7, 0x93, 0xad, 0xd3, 0x24, 0x84, 0x33,
        0x33, 0x10, 0xef, 0x64, 0x1a, 0x5d, 0xa9, 0xbf,
        0x9e, 0x10, 0x4e, 0xcd, 0x1e, 0x8a, 0x4c, 0xb3,
        0x38, 0x19, 0xd5, 0x1a, 0xd1, 0x30, 0xb4, 0x75,
        0x75, 0xed, 0xdf, 0xd7, 0xf0, 0x12, 0xd4, 0xf0,
        0x89, 0x33, 0xe2, 0xd3, 0x51, 0xbb, 0x75, 0xa1,
    };
    for (uint32_t i = 0; i < len; i++, key++)
        cleanText[i] = cipherText[i] ^ cipher_code[key % CIPHER_CODE_LEN];
}

void ota_lock()
{
    while( xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE );
}

void ota_unlock()
{
    while( xSemaphoreGive(mutex) != pdTRUE );
}