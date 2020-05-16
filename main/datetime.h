#ifndef _DATETIME_H
#define _DATETIME_H

#include <time.h>
#include <esp_sntp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    time_t  reqTime;
    time_t  doneTime;
} datetime_sync_type;

typedef struct {
    datetime_sync_type sync;
} datetime_type;

/* datetime Api */
void datetime_init();
void datetime_deinit();
void datetime_set(time_t t, int tz);
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

#ifdef __cplusplus
}
#endif
#endif