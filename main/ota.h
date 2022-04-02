#ifndef _OTA_UPDATE_H
#define _OTA_UPDATE_H

#include <stdint.h>

typedef enum {
    OTA_IDLE,
    OTA_IN_PROGRESS,
    OTA_DONE,
} ota_state_t;

void ota_init();
void ota_deinit();
bool is_ota_client(int client);
bool ota_begin(int client, uint32_t img_size);
bool ota_write(int client, char *data, uint32_t data_len);
bool ota_end(int client);

#endif