#include <esp_log.h>
#include "app_webserver.h"

#define TAG  "app_webserver"

Webserver_type _webserver = { .handle = NULL } ;

void initialise_webserver(void)
{
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    _webserver.cfg = conf;
    _webserver.handle = NULL;
}

void deinitialise_webserver(void)
{

}

void start_webserver(void)
{
    if (_webserver.handle) {
        ESP_LOGI( TAG, "webserver has started." );
        return;
    }
    ESP_ERROR_CHECK( httpd_start(&_webserver.handle, &_webserver.cfg) );
    ESP_ERROR_CHECK( webserver_register_handlers(_webserver.handle) );
    ESP_LOGI( TAG, "start webserver finished." );
}

void stop_webserver(void)
{
    if (!_webserver.handle) {
        ESP_LOGI( TAG, "webserver has stopped." );
        return;
    }
    httpd_stop(_webserver.handle);
    _webserver.handle = NULL;
    ESP_LOGI( TAG, "stop webserver finished." );
}
