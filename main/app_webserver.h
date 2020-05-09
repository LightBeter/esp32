#ifndef _APP_WEBSERVER_H
#define _APP_WEBSERVER_H

#include <esp_http_server.h>

typedef struct {
    httpd_handle_t      handle;
    httpd_config_t      cfg;
} Webserver_type;

void initialise_webserver(void);
void deinitialise_webserver(void);
void start_webserver(void);
void stop_webserver(void);
esp_err_t webserver_register_handlers(httpd_handle_t);

extern Webserver_type          _webserver;

#endif