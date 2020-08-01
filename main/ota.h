#ifndef _OTA_UPDATE_H
#define _OTA_UPDATE_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_ota_ops.h>

typedef enum {
    OTA_IDLE,
    OTA_IN_PROGRESS,
    OTA_DONE,
} ota_state_t;

typedef struct {
    SemaphoreHandle_t mutex;
    ota_state_t       state;
    esp_ota_handle_t        handle;
    const esp_partition_t  *partition;
    char              new_fw_ver[32];           /*!< Application version */
    int               new_fw_size;
    int               new_fw_recv_size;
    int               client;
} ota_t;

void ota_init();
void ota_deinit();
bool is_ota_client(int client);
bool ota_begin(int client, size_t img_size);
bool ota_write(int client, char *data, size_t data_len);
bool ota_end(int client);

#endif