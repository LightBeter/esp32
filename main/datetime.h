#ifndef _DATETIME_H
#define _DATETIME_H

#include <stdint.h>
#include <time.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/* datetime Api */
void datetime_init();
void datetime_deinit();
void datetime_set_tz(int tz);
void datetime_set(time_t t);
void datetime_get(time_t *t, int *tz);
void datetime_sync_sntp();
void datetime_sync_extrtc();
/* SNTP Api */
void datetime_sntp_init();
void datetime_sntp_start();
void datetime_sntp_stop();
/* Extern Rtc Api */
void datetime_extrtc_init(void);
void datetime_extrtc_deinit(void);
void datetime_extrtc_write(time_t t);
esp_err_t datetime_extrtc_read(time_t *t);
void datetime_extrtc_enable_charge();

#ifdef __cplusplus
}
#endif
#endif