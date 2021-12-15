#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <driver/i2c.h>
#include "esp_log.h"
#include "datetime.h"
#include "storage.h"

#define TAG                 "DateTime"
#define SNTP_SERVER         "pool.ntp.org"
#define SYNC_CYCLE          (60 * 60 * 24)
#define SYNC_NTP_TIMEOUT    (1 * 60)

#define EXT_RTC_I2C_NUM     I2C_NUM_0
#define EXT_RTC_SCL_PIN     17
#define EXT_RTC_SDA_PIN     16
#define DS1307_ADDRESS      0x68


void sntp_sync_cb(struct timeval *tv);
uint8_t intToBCD(uint8_t num);
uint8_t bcdToInt(uint8_t bcd);

datetime_type  _datetime = { 0, { 0, 0} };

void datetime_init()
{
    time_t t = 0;
    /* initial sntp */
    datetime_sntp_init();
    /* initial extern rtc */
    datetime_extrtc_init();
    /* initial timezone */
    storage_read_tz(&_datetime.tz);
    datetime_set_tz(_datetime.tz);
    /* initial datetime */
    datetime_extrtc_read(&t);
    datetime_set(t);
    /* initial sync status */
    _datetime.sync.reqTime = 0;
    _datetime.sync.doneTime = 0;
}

void datetime_deinit()
{
    /* initial extern rtc */
    datetime_extrtc_deinit();
}

void datetime_set_tz(int tz)
{
    char str_buf[64];
    uint8_t tz_hh, tz_mm;
    const char *tz_symbol;
    /* update timezone */
    if (tz > 0) {
        tz_symbol = "+";
        tz_hh = tz / 60;
        tz_mm = tz % 60;
    } else {
        tz_symbol = "-";
        tz_hh = (-tz) / 60;
        tz_mm = (-tz) % 60;
    }
    snprintf(str_buf, sizeof(str_buf), "UTC%s%d:%02d", tz_symbol, tz_hh, tz_mm);
    setenv("TZ", str_buf, 1);
    tzset();
    /* save timezone */
    if (_datetime.tz != tz) {
        storage_write_tz(tz);
        _datetime.tz = tz;
    }
}

void datetime_set(time_t t)
{
    struct  timeval  tv = { 0, 0 };
	struct tm *tm;
    /* update datatime */
    tv.tv_sec = t;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    /* update ext rtc */
    datetime_extrtc_write(t);
    /* output new datetime */
    tm = localtime(&t);
    ESP_LOGI( TAG, "%s: set new date/time %s", __FUNCTION__, asctime(tm) );
}

void datetime_get(time_t *t, int *tz)
{    
    *t = time(NULL);
    *tz = _datetime.tz;
}

void datetime_resp(cJSON *respone)
{
    time_t  utc;
    int tz;
    datetime_get(&utc, &tz);
    cJSON_AddNumberToObject(respone, "utc", utc);
    cJSON_AddNumberToObject(respone, "tz", tz);
}

void datetime_sntp_init()
{
    sntp_setoperatingmode( SNTP_OPMODE_POLL );
    sntp_setservername( 0, SNTP_SERVER );
    sntp_set_time_sync_notification_cb(sntp_sync_cb);
}

void datetime_sntp_start(void)
{
    sntp_init();
}

void datetime_sntp_stop()
{
    sntp_stop();
}

void sntp_sync_cb(struct timeval *tv) 
{
    struct tm *tm;
    time_t  t = tv->tv_sec;

    datetime_extrtc_write(t);
    _datetime.sync.doneTime = t;
    _datetime.sync.reqTime = 0;
    tm = localtime(&t);
    ESP_LOGI(TAG, "sntp complete, New York date/time is: %s", asctime(tm));
}

void datetime_sync_sntp()
{
    time_t now, diff;
    now = time(NULL);
    if (_datetime.sync.reqTime != 0) {
        diff = difftime(now, _datetime.sync.reqTime);
        if (diff > SYNC_NTP_TIMEOUT) {
            _datetime.sync.reqTime = now;
            datetime_sntp_stop();
            datetime_sntp_start();
            ESP_LOGI( TAG, "retry sntp......." );
        }
    } else if (_datetime.sync.reqTime == 0) {
        diff = difftime(now, _datetime.sync.doneTime);
        if (diff > SYNC_CYCLE || 0 == _datetime.sync.doneTime) {
            _datetime.sync.reqTime = now;
            datetime_sntp_start();
            ESP_LOGI( TAG, "sntp......." );
        }
    }
}

void datetime_extrtc_init(void)
{
	i2c_config_t conf;
	conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = EXT_RTC_SDA_PIN;
	conf.scl_io_num = EXT_RTC_SCL_PIN;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	conf.master.clk_speed = 100000;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;
	ESP_ERROR_CHECK( i2c_param_config(EXT_RTC_I2C_NUM, &conf) );
	ESP_ERROR_CHECK( i2c_driver_install(EXT_RTC_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0) );
}

void datetime_extrtc_deinit(void)
{
	ESP_ERROR_CHECK( i2c_driver_delete(I2C_NUM_0) );
}


uint8_t intToBCD(uint8_t num) {
	return ((num / 10) << 4) | (num % 10);
}

uint8_t bcdToInt(uint8_t bcd) {
	return ((bcd >> 4) * 10) + (bcd & 0x0f);;
}

esp_err_t datetime_extrtc_read(time_t *t)
{	
    uint8_t data[7];
	esp_err_t errRc;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	ESP_ERROR_CHECK( i2c_master_start(cmd) );
	ESP_ERROR_CHECK( i2c_master_write_byte(cmd, (DS1307_ADDRESS << 1) | I2C_MASTER_WRITE, 1 /* expect ack */) );
	ESP_ERROR_CHECK( i2c_master_write_byte(cmd, 0x0, 1));
	ESP_ERROR_CHECK( i2c_master_start(cmd));
	ESP_ERROR_CHECK( i2c_master_write_byte(cmd, (DS1307_ADDRESS << 1) | I2C_MASTER_READ, 1 /* expect ack */) );
	ESP_ERROR_CHECK( i2c_master_read(cmd, data, 7, 0) );
	ESP_ERROR_CHECK( i2c_master_stop(cmd) );
    errRc = i2c_master_cmd_begin( EXT_RTC_I2C_NUM, cmd, 1000/portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    if (errRc != ESP_OK) {
		ESP_LOGE( TAG, "%s: i2c_master_cmd_begin: %d", __FUNCTION__, errRc );
        return errRc;
	}

	struct tm tm;
	tm.tm_sec  = bcdToInt(data[0]);
	tm.tm_min  = bcdToInt(data[1]);
	tm.tm_hour = bcdToInt(data[2]);
	tm.tm_mday = bcdToInt(data[4]);
	tm.tm_mon  = bcdToInt(data[5]) - 1; // 0-11 - Note: The month on the DS1307 is 1-12.
	tm.tm_year = bcdToInt(data[6]) + 100; // Years since 1900
	*t = mktime(&tm) - (_datetime.tz * 60);
    ESP_LOGI( TAG, "%s: read external rtc date/time is %s", __FUNCTION__, asctime(&tm) );
	return ESP_OK;
}

void datetime_extrtc_write(time_t t)
{
	struct tm tm;
	gmtime_r(&t, &tm);

	esp_err_t errRc;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	ESP_ERROR_CHECK(i2c_master_start(cmd));
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, (DS1307_ADDRESS << 1) | I2C_MASTER_WRITE, 1 /* expect ack */));
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, 0x0, 1));
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, intToBCD(tm.tm_sec), 1) );      // seconds
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, intToBCD(tm.tm_min), 1) );     // minutes
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, intToBCD(tm.tm_hour), 1) );    // hours
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, intToBCD(tm.tm_wday+1), 1) );  // week day
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, intToBCD(tm.tm_mday), 1) );     // date of month
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, intToBCD(tm.tm_mon+1), 1) );    // month
	ESP_ERROR_CHECK(i2c_master_write_byte( cmd, intToBCD(tm.tm_year-100), 1 )); // year
	ESP_ERROR_CHECK(i2c_master_stop(cmd));
	errRc = i2c_master_cmd_begin(EXT_RTC_I2C_NUM, cmd, 1000/portTICK_PERIOD_MS);
	if (errRc != ESP_OK) {
		ESP_LOGE( TAG, "%s: i2c_master_cmd_begin: %d", __FUNCTION__, errRc );
	} else {
        ESP_LOGI( TAG, "%s: write external rtc date/time is %s", __FUNCTION__, asctime(&tm) );
    }
	i2c_cmd_link_delete(cmd);
}

void datetime_sync_extrtc()
{
    struct timeval  tv = { 0, 0 };
    struct tm *tm;
    time_t diff = difftime(time(NULL), _datetime.sync.doneTime);
    if (diff < SYNC_CYCLE) {
        return;
    }
    /* update datatime */
    if (ESP_OK == datetime_extrtc_read( &tv.tv_sec)) {
        settimeofday(&tv, NULL);
        /* output new datetime */
        tm = localtime(&tv.tv_sec);
        ESP_LOGI( TAG, "%s: set new date/time %s", __FUNCTION__, asctime(tm) );
    }
    _datetime.sync.doneTime = tv.tv_sec;
}