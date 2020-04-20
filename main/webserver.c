#include "app_global.h"

#define TAG  "app_webserver"

// extern const unsigned char cacert_pem_start[]  asm("_binary_cacert_pem_start");
// extern const unsigned char cacert_pem_end[]    asm("_binary_cacert_pem_end");
// extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
// extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");

Webserver_type _webserver = { .handle = NULL } ;

void initialise_webserver(void)
{
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    // conf.cacert_pem  = cacert_pem_start;
    // conf.cacert_len  = cacert_pem_end - cacert_pem_start;
    // conf.prvtkey_pem = prvtkey_pem_start;
    // conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;
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
        ESP_LOGI( TAG, "webserver has stopped ." );
        return;
    }
    httpd_stop(_webserver.handle);
    _webserver.handle = NULL;
    ESP_LOGI( TAG, "stop webserver finished." );
}
