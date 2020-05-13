#include <string.h>
#include <esp_log.h>
#include <lwip/api.h>
#include "websocket_server.h"
#include "wss_tsk.h"

#define TAG                         "WSS"
#define TSK_NAME                     TAG
#define WS_SERVER_TSK_STACK_SIZE     3072
#define WS_HANDLE_TSK_STACK_SIZE     4096
#define WS_CLIENTS_QUEUE_SIZE        10 
#define WS_CLIENT_RECV_TIME          1000 

void wss_server_tsk(void * parm);
void wss_handle_tsk(void * parm);
void wss_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len);

wss_tsk_type _wss_tsk;

void wss_tsk_init()
{
    _wss_tsk.serverTsk = NULL;
    _wss_tsk.handleTsk = NULL;
    _wss_tsk.clientQueue = xQueueCreate(WS_CLIENTS_QUEUE_SIZE, sizeof(struct netconn*));
}

void wss_tsk_deinit()
{

}

void wss_tsk_start()
{
    if (!_wss_tsk.serverTsk) {
        xTaskCreate(wss_server_tsk, "ws_server_tsk", WS_SERVER_TSK_STACK_SIZE, NULL, 3, &_wss_tsk.serverTsk);
    }
    if (!_wss_tsk.handleTsk) {
        xTaskCreate(wss_handle_tsk, "ws_handle_tsk", WS_HANDLE_TSK_STACK_SIZE, NULL, 3, &_wss_tsk.handleTsk);
    }
    ws_server_start();
}

void wss_tsk_stop()
{
    if (_wss_tsk.handleTsk) {
        vTaskDelete(_wss_tsk.handleTsk);
        _wss_tsk.handleTsk = NULL;
    }
    if (_wss_tsk.serverTsk) {
        vTaskDelete(_wss_tsk.serverTsk);
        _wss_tsk.serverTsk = NULL;
    }
    ws_server_stop();
}

void wss_server_tsk(void * parm)
{
    struct netconn *conn, *client;
    err_t err;

    conn = netconn_new( NETCONN_TCP );
    netconn_bind( conn, NULL,80 );
    netconn_listen(conn);
    ESP_LOGI (TAG, "WebSocket Server Start listening" );
    while (1) {
        err = netconn_accept(conn, &client);
        if(err == ERR_OK) {
            xQueueSendToBack(_wss_tsk.clientQueue, &client, portMAX_DELAY);
        } else {
            ESP_LOGW( TAG, "%s netconn_accept failure %d", __FUNCTION__, err);
        }
    }
    netconn_close(conn);
    netconn_delete(conn);
    vTaskDelete(NULL);
    _wss_tsk.serverTsk = NULL;
}

void wss_handle_tsk(void* parm) 
{
    struct netconn* client;
    struct netbuf* inbuf;
    char* buf;
    uint16_t buflen;
    err_t err;
    bool isWebSocket;

    ESP_LOGI(TAG, "WebSocket Server Start handle");
    while (1) {
        xQueueReceive(_wss_tsk.clientQueue, &client, portMAX_DELAY);
        if(!client) 
            continue;

        isWebSocket = false;
        netconn_set_recvtimeout( client, WS_CLIENT_RECV_TIME ); 
        err = netconn_recv( client, &inbuf );
        if( err==ERR_OK ) {
            netbuf_data( inbuf, (void**)&buf, &buflen );
            if(buf) {
                if(strstr(buf,"GET / ") && strstr(buf,"Upgrade: websocket")) {
                    ESP_LOGI(TAG,"Requesting websocket on /");
                    ws_server_add_client( client, buf, buflen, "/", wss_callback);
                    netbuf_delete(inbuf);
                    isWebSocket = true;
                } else {
                    ESP_LOGI(TAG,"Unknown request");
                }
            } else {
                ESP_LOGI(TAG,"Unknown request (empty?...)");
            }
        } else { // if err==ERR_OK
            ESP_LOGI(TAG,"error on read, closing connection");
        }
    
        if (false == isWebSocket) {
            netconn_close( client );
            netconn_delete( client );
            netbuf_delete( inbuf ); 
        }
    }
    vTaskDelete(NULL);
    _wss_tsk.handleTsk = NULL;
}

void wss_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len) 
{
  switch(type) {
    case WEBSOCKET_CONNECT:
        ESP_LOGI(TAG, "client %i connected!", num);
        break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
        ESP_LOGI(TAG, "client %i sent a disconnect message", num);
        break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
        ESP_LOGI(TAG, "client %i was disconnected", num);
        break;
    case WEBSOCKET_DISCONNECT_ERROR:
        ESP_LOGI(TAG, "client %i was disconnected due to an error", num);
        break;
    case WEBSOCKET_TEXT:
        ESP_LOGI(TAG, "client %i sent test message of size %i:\n%s",num, (uint32_t)len, msg);
        break;
    case WEBSOCKET_BIN:
        ESP_LOGI(TAG, "client %i sent binary message of size %i:\n%s",num, (uint32_t)len, msg);
        break;
    case WEBSOCKET_PING:
        ESP_LOGI(TAG, "client %i pinged us with message of size %i:\n%s",num, (uint32_t)len, msg);
        break;
    case WEBSOCKET_PONG:
        ESP_LOGI(TAG,"client %i responded to the ping",num);
        break;
  }
}