#include "app_global.h"

#define TAG  "app_webserver"

extern const unsigned char cacert_pem_start[]  asm("_binary_cacert_pem_start");
extern const unsigned char cacert_pem_end[]    asm("_binary_cacert_pem_end");
extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");

device_webserver_type _device_webserver = {
    .handle = NULL,
};

const httpd_uri_t webserver_uri_handlers = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

void initialise_webserver(void)
{
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.cacert_pem  = cacert_pem_start;
    conf.cacert_len  = cacert_pem_end - cacert_pem_start;
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;
    _device_webserver.cfg = conf;
    _device_webserver.handle = NULL;
}

void deinitialise_webserver(void)
{

}

void start_webserver(void)
{
    if (_device_webserver.handle) {
        ESP_LOGI( TAG, "webserver has started." );
        return;
    }

    ESP_ERROR_CHECK( httpd_ssl_start(&_device_webserver.handle, &_device_webserver.cfg) );
    ESP_ERROR_CHECK( httpd_register_uri_handler(_device_webserver.handle, &webserver_uri_handlers) );
    ESP_LOGI( TAG, "start webserver finished." );
}

void stop_webserver(void)
{
    if (!_device_webserver.handle) {
        ESP_LOGI( TAG, "webserver has stopped ." );
        return;
    }
    httpd_ssl_stop(_device_webserver.handle);
    _device_webserver.handle = NULL;
    ESP_LOGI( TAG, "stop webserver finished." );
}
