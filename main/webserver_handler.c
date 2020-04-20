#include <time.h>
#include <esp_http_server.h>
#include <cJSON.h>
#include "app_global.h"

#define TAG  "app_webserver"

esp_err_t ws_reboot_handler(httpd_req_t *req);
esp_err_t ws_version_handler(httpd_req_t *req);
esp_err_t ws_time_handler(httpd_req_t *req);
esp_err_t ws_synctime_handler(httpd_req_t *req);

const httpd_uri_t _webserver_uri_handlers[] = {
    {
        .uri       = "/reboot",
        .method    = HTTP_POST,
        .handler   = ws_reboot_handler
    },
    {
        .uri       = "/version",
        .method    = HTTP_POST,
        .handler   = ws_version_handler
    },
    {
        .uri       = "/time",
        .method    = HTTP_POST,
        .handler   = ws_time_handler
    },
    {
        .uri       = "/synctime",
        .method    = HTTP_POST,
        .handler   = ws_synctime_handler
    }
};

const int _webserver_handlers_no = sizeof(_webserver_uri_handlers) / sizeof(httpd_uri_t);

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

esp_err_t ws_reboot_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    return ESP_OK;
}

esp_err_t ws_version_handler(httpd_req_t *req)
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
    free(out);
    return ESP_OK;
}

esp_err_t ws_synctime_handler(httpd_req_t *req)
{
    char *buf;
    cJSON *root = NULL, *item = NULL;
    int remaining = req->content_len, ret;

    buf = malloc(remaining);
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf, remaining);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        } else if (ret < 0) {
            free(buf);
            return ESP_FAIL;
        }
        break;
    }
    root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        return ESP_FAIL;
    }
    item = cJSON_GetObjectItem(root, "time");
    if (!cJSON_IsNumber(item)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        return ESP_FAIL; 
    } 
    //stime(item->valuedouble);
    return ESP_OK;
}

esp_err_t ws_time_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    char *out = NULL;

    cJSON_AddNumberToObject(root, "time", time(NULL));
    out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    httpd_resp_send(req, out, -1); 
    free(out);
    return ESP_OK;
}