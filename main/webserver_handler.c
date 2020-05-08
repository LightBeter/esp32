#include <time.h>
#include <sys/time.h>
#include <esp_http_server.h>
#include <cJSON.h>
#include "app_global.h"

#define TAG  "app_webserver"

cJSON* ws_content2json(httpd_req_t *req);

esp_err_t ws_temporary_led_handler(httpd_req_t *req);
esp_err_t ws_info_handler(httpd_req_t *req);
esp_err_t ws_gettime_handler(httpd_req_t *req);
esp_err_t ws_settime_handler(httpd_req_t *req);
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

    buf = malloc(remaining);
    while (remaining > 0) {
        len = httpd_req_recv(req, &buf[req->content_len - remaining], remaining);
        if (len == HTTPD_SOCK_ERR_TIMEOUT && retry < 5) {
            retry++;
            continue;
        } else if (len < 0) {
            free(buf);
            return NULL;
        }
        remaining -= len;
    }
    root = cJSON_Parse(buf);
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
    cJSON *root, *item;
    char  name[2] = { 0, 0 };
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
    for (chl = 0; chl < LED_CHL_COUNT; chl++) {
        name[0] = '0' + chl;
        item = cJSON_GetObjectItem(root, name);
        if (!cJSON_IsNumber(item)) {
            ESP_LOGI(TAG, "%s parse channel %u failure.", __FUNCTION__, chl);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            goto exit;  
        }
        if (item->valueint < 0 || item->valueint > 100) {
            ESP_LOGI(TAG, "%s channel value %d failure.", __FUNCTION__, item->valueint);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
            goto exit; 
        }
        value[chl] = item->valueint;
    }
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
    cJSON_AddStringToObject(root, "hw", "0.0.1");
    cJSON_AddStringToObject(root, "sw", "0.0.1");
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
    cJSON_AddNumberToObject(root, "utc", tv.tv_sec + (tz.tz_minuteswest * 60));
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

    tv.tv_sec = utc->valuedouble + zone->valuedouble * 60 * 60;
    tz.tz_minuteswest = -(zone->valuedouble * 60);
    //settimeofday(&tv, &tz);
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
    xEventGroupSetBits(_app_event_group, APP_REQ_REBOOT_BIT);
    return ESP_OK;
}

esp_err_t ws_recovery_setting_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    xEventGroupSetBits(_app_event_group, APP_REQ_RECOVERY_FACTORY_BIT);
    return ESP_OK;
}