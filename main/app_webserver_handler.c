#include <time.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <cJSON.h>
#include <esp_log.h>
#include "app_wifi.h"
#include "app_webserver.h"
#include "app_control.h"
#include "app_misc.h"

#define TAG                 "app_webserver"
#define WS_RESP_OK          "{\"code\":0}"

cJSON* ws_content2json(httpd_req_t *req);

esp_err_t ws_temporary_led_handler(httpd_req_t *req);
esp_err_t ws_info_handler(httpd_req_t *req);
esp_err_t ws_gettime_handler(httpd_req_t *req);
esp_err_t ws_settime_handler(httpd_req_t *req);
esp_err_t ws_setnw_handler(httpd_req_t *req);
esp_err_t ws_reboot_handler(httpd_req_t *req);
esp_err_t ws_recovery_setting_handler(httpd_req_t *req);

const httpd_uri_t _webserver_uri_handlers[] = {
    {
        .uri       = "/temporaryled",
        .method    = HTTP_POST,
        .handler   = ws_temporary_led_handler
    },
    {
        .uri       = "/info",
        .method    = HTTP_POST,
        .handler   = ws_info_handler
    },
    {
        .uri       = "/time",
        .method    = HTTP_POST,
        .handler   = ws_gettime_handler
    },
    {
        .uri       = "/settime",
        .method    = HTTP_POST,
        .handler   = ws_settime_handler
    },
    {
        .uri       = "/setnw",
        .method    = HTTP_POST,
        .handler   = ws_setnw_handler
    },
    {
        .uri       = "/reboot",
        .method    = HTTP_POST,
        .handler   = ws_reboot_handler
    },
        {
        .uri       = "/recoverysetting",
        .method    = HTTP_POST,
        .handler   = ws_recovery_setting_handler
    }
};

const int _webserver_handlers_no = sizeof(_webserver_uri_handlers) / sizeof(httpd_uri_t);

cJSON* ws_content2json(httpd_req_t *req)
{
    char *buf = NULL;
    cJSON *root = NULL;
    int remaining = req->content_len, retry = 0, len;

    ESP_LOGI(TAG, "%s remaining %d.", __FUNCTION__, remaining);

    buf = malloc(remaining);
    while (remaining > 0) {
        len = httpd_req_recv(req, &buf[req->content_len - remaining], remaining);
        if (len == HTTPD_SOCK_ERR_TIMEOUT) {
            if (++retry >= 5) {
                goto exit;
            } else {
                ESP_LOGI(TAG, "%s remaining timeout, retry.", __FUNCTION__);
                continue;
            }
        } else if (len < 0) {
            ESP_LOGI(TAG, "%s remaining failure %d.", __FUNCTION__, len);
            goto exit;
        }
        remaining -= len;
    }
    root = cJSON_Parse(buf);

exit:    
    ESP_LOGI(TAG, "%s remaining end.", __FUNCTION__);
    free(buf); 
    return root;
}

esp_err_t webserver_register_handlers(httpd_handle_t hd)
{
    for (int i = 0; i < _webserver_handlers_no; i++) {
        if (httpd_register_uri_handler(hd, &_webserver_uri_handlers[i]) != ESP_OK) {
            ESP_LOGW(TAG, "webserver register uri failed for %d", i);
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "webserver register uri finished");
    return ESP_OK;
}

esp_err_t ws_temporary_led_handler(httpd_req_t *req)
{
    esp_err_t ret = ESP_FAIL;
    cJSON *root, *item, *pSub;
    uint8_t chl, value[LED_CHL_COUNT], active;

    root = ws_content2json(req);
    if (!root) {
        ESP_LOGI(TAG, "%s parse content failure.", __FUNCTION__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        goto exit;
    }
    /* active */
    item = cJSON_GetObjectItem(root, "active");
    if (!cJSON_IsNumber(item)) {
        ESP_LOGI(TAG, "%s parse active failure.", __FUNCTION__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        goto exit;  
    }
    if (item->valueint < 1 || item->valueint > 180) {
        ESP_LOGI(TAG, "%s active %d failure.", __FUNCTION__, item->valueint);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        goto exit; 
    }
    active = item->valueint;
    /* channel */
    item = cJSON_GetObjectItem(root, "targ");
    if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) !=  LED_CHL_COUNT) {
        ESP_LOGI(TAG, "%s targ failure.", __FUNCTION__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        goto exit; 
    }

    for (chl = 0; chl < LED_CHL_COUNT; chl++) {
        pSub = cJSON_GetArrayItem(item, chl);
        if(NULL == pSub ){ 
            ESP_LOGI(TAG, "%s targ %d failure.", __FUNCTION__, chl);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            goto exit; 
        }

        if (!cJSON_IsNumber(pSub)) {
            ESP_LOGI(TAG, "%s targ %d failure.", __FUNCTION__, chl);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            goto exit; 
        }

        if (pSub->valueint < 0 || pSub->valueint > 100) {
            ESP_LOGI(TAG, "%s channel value %d failure.", __FUNCTION__, item->valueint);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            goto exit; 
        }
        value[chl] = pSub->valueint;
    }
    httpd_resp_send(req, WS_RESP_OK, strlen(WS_RESP_OK));
    ret = ESP_OK;
exit:    
    if (root) {
        cJSON_Delete(root);
    }
    return ret;
}

esp_err_t ws_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    char *out = NULL;

    cJSON_AddStringToObject(root, "module", "esp32");
    cJSON_AddStringToObject(root, "hw", "1.0.0");
    cJSON_AddStringToObject(root, "sw", _running_app_info.version);
    out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, out, -1); 
    cJSON_free(out);
    return ESP_OK;
}

esp_err_t ws_gettime_handler(httpd_req_t *req)
{
    struct  timeval  tv;
    struct  timezone tz;
    cJSON *root = cJSON_CreateObject();
    char *out = NULL;

    gettimeofday(&tv, &tz);
    cJSON_AddNumberToObject(root, "utc", tv.tv_sec);
    cJSON_AddNumberToObject(root, "zone", (-tz.tz_minuteswest / 60));
    out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, out, -1); 
    cJSON_free(out);
    return ESP_OK;
}

esp_err_t ws_settime_handler(httpd_req_t *req)
{
    esp_err_t ret;
    struct  timeval  tv = { 0, 0 };
    struct  timezone tz = { 0, 0 };
    cJSON *root, *utc, *zone;

    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    root = ws_content2json(req);
    if (!root) {
        ESP_LOGI(TAG, "%s parse content failure.", __FUNCTION__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        ret = ESP_FAIL;
        goto exit;
    }
    utc = cJSON_GetObjectItem(root, "utc");
    if (!cJSON_IsNumber(utc)) {
        ESP_LOGI(TAG, "%s parse utc failure.", __FUNCTION__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        ret = ESP_FAIL;
        goto exit;
    } 

    zone = cJSON_GetObjectItem(root, "zone");
    if (!cJSON_IsNumber(zone)) {
        ESP_LOGI(TAG, "%s parse zone failure.", __FUNCTION__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        ret = ESP_FAIL;
        goto exit;
    } 

    tv.tv_sec = utc->valuedouble;
    tz.tz_minuteswest = -(zone->valuedouble * 60);
    settimeofday(&tv, &tz);
    httpd_resp_send(req, WS_RESP_OK, strlen(WS_RESP_OK));
    ret = ESP_OK;
exit:    
    if (root) {
        cJSON_Delete(root);
    }
    return ret;
}

esp_err_t ws_setnw_handler(httpd_req_t *req)
{
    esp_err_t ret;
    cJSON *root, *mode, *ssid = NULL, *password = NULL;
    size_t len;
    bool save = false;

    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    root = ws_content2json(req);
    if (!root) {
        ESP_LOGI(TAG, "%s parse content failure.", __FUNCTION__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        ret = ESP_FAIL;
        goto exit;
    }

    mode = cJSON_GetObjectItem(root, "mode");
    if (!cJSON_IsNumber(mode) || (mode->valueint != WIFI_MODE_STA && mode->valueint != WIFI_MODE_AP)) {
        ESP_LOGI(TAG, "%s parse mode failure.", __FUNCTION__);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        ret = ESP_FAIL;
        goto exit;
    } 

    if (WIFI_MODE_STA == mode->valueint) {
        ssid = cJSON_GetObjectItem(root, "ssid");
        if (!cJSON_IsString(ssid)) {
            ESP_LOGI(TAG, "%s parse ssid failure.", __FUNCTION__);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            ret = ESP_FAIL;
            goto exit;
        } 

        password = cJSON_GetObjectItem(root, "password");
        if (!cJSON_IsString(password)) {
            ESP_LOGI(TAG, "%s parse password failure.", __FUNCTION__);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            ret = ESP_FAIL;
            goto exit;
        } 

        len = strlen(ssid->valuestring);
        if (len <= 0 || len >= 32) {
            ESP_LOGI(TAG, "%s invalid ssid len(%zu) failure.", __FUNCTION__, len);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            ret = ESP_FAIL;
            goto exit;  
        }
        len = strlen(password->valuestring);
        if (len >= 64) {
            ESP_LOGI(TAG, "%s invalid password len(%zu) failure.", __FUNCTION__, len);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            ret = ESP_FAIL;
            goto exit;  
        }
    }

    if (_device_wifi.mode != mode->valueint) {
        save = true;
    } else if (WIFI_MODE_STA == mode->valueint) {
        save  = (0 != strcmp( (char*)_device_wifi.cfg.sta.ssid, ssid->valuestring) );
        save |= (0 != strcmp( (char*)_device_wifi.cfg.sta.password, password->valuestring) );
    }

    if (save) {
        _device_wifi.mode = mode->valueint;
        bzero( &_device_wifi.cfg, sizeof(wifi_config_t) );
        if (WIFI_MODE_STA == _device_wifi.mode) {
            strcpy( (char*)_device_wifi.cfg.sta.ssid, ssid->valuestring );
            strcpy( (char*)_device_wifi.cfg.sta.password, password->valuestring );
        }
        ESP_LOGI(TAG, "%s save network config\r\n\tmode[%d]\r\n\tssid[%s]\r\n\tpassword[%s].", 
                        __FUNCTION__, 
                        _device_wifi.mode, 
                        _device_wifi.cfg.sta.ssid,
                        _device_wifi.cfg.sta.password);
        save_wifi_config();
    }
    httpd_resp_send(req, WS_RESP_OK, strlen(WS_RESP_OK));
    ret = ESP_OK;
exit:    
    if (root) {
        cJSON_Delete(root);
    }
    return ret;
}

esp_err_t ws_reboot_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, WS_RESP_OK, strlen(WS_RESP_OK));
    xEventGroupSetBits(_app_event_group, APP_REQ_REBOOT_BIT);
    return ESP_OK;
}

esp_err_t ws_recovery_setting_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, WS_RESP_OK, strlen(WS_RESP_OK));
    xEventGroupSetBits(_app_event_group, APP_REQ_RECOVERY_FACTORY_BIT);
    return ESP_OK;
}