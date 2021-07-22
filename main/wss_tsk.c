#include <string.h>
#include <esp_log.h>
#include <lwip/api.h>
#include <cJSON.h>
#include "wss_tsk.h"
#include "wss_cmd_handle.h"
#include "ota.h"

#define TAG                         "WSS"
#define TSK_NAME                     TAG
#define SERVER_TSK_STACK_SIZE       3072
#define HANDLE_TSK_STACK_SIZE       4096
#define WS_CLIENT_QUEUE_SIZE         10 
#define WS_CLIENT_LIFETIME           (48 * 1000)
#define WS_CLIENT_RECV_TIME          1000 

void wss_client_reset(int num);
void wss_server_tsk(void * parm);
void wss_handle_tsk(void * parm);
void wss_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len);
void wss_callback_bin(uint8_t num, char *msg, uint64_t len);
void wss_callback_text(uint8_t num, char* msg, uint64_t len);
void wss_callback_disconnect(uint8_t num);

wss_tsk_type _wss_tsk;

void wss_client_reset(int num)
{
    _wss_tsk.clients[num].valid = false;
    _wss_tsk.clients[num].lifetime = 0;  
}

void wss_tsk_init()
{
    _wss_tsk.serverTsk = NULL;
    _wss_tsk.handleTsk = NULL;
    _wss_tsk.clientQueue = xQueueCreate(WS_CLIENT_QUEUE_SIZE, sizeof(struct netconn*));
    for(int i = 0; i < WEBSOCKET_SERVER_MAX_CLIENTS; i++) {
        wss_client_reset(i);
    }
}

void wss_tsk_deinit()
{

}

void wss_tsk_start()
{
    if (!_wss_tsk.serverTsk) {
        xTaskCreate(wss_server_tsk, "ws_server_tsk", SERVER_TSK_STACK_SIZE, NULL, 3, &_wss_tsk.serverTsk);
    }
    if (!_wss_tsk.handleTsk) {
        xTaskCreate(wss_handle_tsk, "ws_handle_tsk", HANDLE_TSK_STACK_SIZE, NULL, 3, &_wss_tsk.handleTsk);
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
    netconn_bind( conn, NULL, 80 );
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
    _wss_tsk.serverTsk = NULL;
    vTaskDelete(NULL);
}

void wss_handle_tsk(void* parm) 
{
    struct netconn* client;
    struct netbuf* inbuf;
    char* buf;
    uint16_t buflen;
    err_t err;
    int client_num = 0;
    bool isWebSocket;

    ESP_LOGI(TAG, "WebSocket Server Start handle");
    while (1) {
        client = NULL;
        xQueueReceive(_wss_tsk.clientQueue, &client, pdMS_TO_TICKS(2000));
        if (!client) {
            for (client_num = 0; client_num < WEBSOCKET_SERVER_MAX_CLIENTS; client_num++) {
                if (_wss_tsk.clients[client_num].valid) {
                    _wss_tsk.clients[client_num].lifetime -= 2000;
                    if (_wss_tsk.clients[client_num].lifetime < 0) {
                        ESP_LOGI( TAG, "websocket client %d lifetime timeover, remove", client_num );
                        ws_server_remove_client(client_num); 
                    }
                }
            }
            continue;
        }
        /* recv request head */
        isWebSocket = false;
        netconn_set_recvtimeout( client, WS_CLIENT_RECV_TIME ); 
        err = netconn_recv( client, &inbuf );
        if (ERR_OK != err) {
            ESP_LOGI(TAG,"error on read, closing connection");
            goto next;
        }
        buf = NULL;
        buflen = 0;
        netbuf_data( inbuf, (void**)&buf, &buflen );
        if (!buf) {
            ESP_LOGI(TAG,"Unknown request (empty?...)");
            goto next;
        }
        /* match request */        
        if(strstr(buf,"GET / ") && strstr(buf,"Upgrade: websocket")) {
            ESP_LOGI( TAG, "Requesting websocket on /" );
            client_num = ws_server_add_client( client, buf, buflen, "/", wss_callback);
            if (client_num >= 0) {
                _wss_tsk.clients[client_num].valid = true;
                _wss_tsk.clients[client_num].lifetime = WS_CLIENT_LIFETIME;
                netbuf_delete(inbuf);
                isWebSocket = true;
            } else {
                ESP_LOGI(TAG,"ws_server_add_client failure");  
            }
        } else {
            ESP_LOGI(TAG,"Unknown request");
        }    
next:    
        if (false == isWebSocket) {
            netconn_close( client );
            netconn_delete( client );
            netbuf_delete( inbuf ); 
        }
    }
    _wss_tsk.handleTsk = NULL;
    vTaskDelete(NULL);
}

void wss_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len) 
{
    switch(type) {
    case WEBSOCKET_CONNECT:
        ESP_LOGI(TAG, "client[%i] connected!", num);
        _wss_tsk.clients[num].lifetime = WS_CLIENT_LIFETIME;
        return;
    case WEBSOCKET_BIN:
        //ESP_LOGI(TAG, "client[%i] sent binary message of size %i", num, (uint32_t)len);
        wss_callback_bin(num, msg, len);
        return;
    case WEBSOCKET_PING:
        //ESP_LOGI(TAG, "client[%i] pinged us with message of size %i", num, (uint32_t)len);
        _wss_tsk.clients[num].lifetime = WS_CLIENT_LIFETIME;
        return;
    case WEBSOCKET_PONG:
        //ESP_LOGI(TAG,"client[%i] responded to the ping",num);
        return;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
        ESP_LOGI(TAG, "client[%i] sent a disconnect message", num);
        wss_callback_disconnect(num);
        break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
        ESP_LOGI(TAG, "client[%i] was disconnected", num);
        wss_callback_disconnect(num);
        break;
    case WEBSOCKET_DISCONNECT_ERROR:
        ESP_LOGI(TAG, "client[%i] was disconnected due to an error", num);
        wss_callback_disconnect(num);
        break;
    case WEBSOCKET_TEXT:
        ESP_LOGI(TAG, "client[%i] msg(%i): %s", num, (uint32_t)len, msg);
        wss_callback_text(num, msg, len);
        break;
    }
}

void wss_callback_disconnect(uint8_t client)
{
    wss_client_reset(client);
    if (is_ota_client(client)) 
        ota_end(client);
}

void wss_callback_bin(uint8_t client, char *msg, uint64_t len)
{
    char  resp[2];

    if (wss_cmd_e_ota_write != msg[0]) {
        resp[1] = 1;
    } else if (len <= 1) {
        resp[1] = 2;
    } else {
        if (true != ota_write(client, &msg[1], len - 1)) {
            ESP_LOGI(TAG, "ota_write OK");
            resp[1] = 3;
        } else {
            resp[1] = 0;
        }
    }
    _wss_tsk.clients[client].lifetime = WS_CLIENT_LIFETIME;  
    resp[0] = msg[0];
    ws_server_send_bin_client_from_callback( client, resp, 2 );
}

void wss_callback_text(uint8_t client, char* msg, uint64_t len)
{
    cJSON *request, *respone, *item;
    wss_respond_code_t code = wss_respond_code_ok;
    uint32_t cmd = 0;
    char *out = NULL, *token = NULL;

    msg[len] = 0;
    respone = cJSON_CreateObject();
    request = cJSON_Parse( msg );
    if ( !request ) {
        ESP_LOGI(TAG, "parse client %i sent message failure", client); 
        code = wss_respond_code_invalid_format;
    } else {
        item = cJSON_GetObjectItem( request, "cmd" );
        if ( !cJSON_IsNumber(item) ) {
            ESP_LOGI(TAG, "get client %i sent message cmd failure", client); 
            code = wss_respond_code_invalid_cmd;
        } else {
            cmd = item->valueint;
        }
        item = cJSON_GetObjectItem( request, "token" );
        if (!cJSON_IsString(item)) {
            ESP_LOGI(TAG, "parse client %i sent message token failure", client); 
            code = wss_respond_code_invalid_token;
        } else {
            token = item->valuestring;
        }
    }

    if (wss_respond_code_ok != code) {
        goto send_respond;
    }
    switch (cmd) {
    case wss_cmd_r_heartbeat:
        code = wss_respond_code_ok;
        break;
    case wss_cmd_r_dev_info:
        code = read_dev_info_handle(request, respone);
        break;
    case wss_cmd_r_dev_time:
        code = read_dev_time_handle(request, respone);
        break;
    case wss_cmd_w_dev_time:
        code = write_dev_time_handle(request, respone);
        break;
    case wss_cmd_w_dev_network:
        code = write_dev_network_handle(request, respone);
        break;
    case wss_cmd_e_wifi_ap_scan:
        code = exec_wifi_ap_scan_handle(client, request, respone);
        break;
    case wss_cmd_r_fan_duty:
        code = read_fan_duty_handle(request, respone);
        break;
    case wss_cmd_w_fan_duty:
        code = write_fan_duty_handle(request, respone);
        break; 
    case wss_cmd_e_dev_reboot:
        code = exec_reboot_handle(request, respone);
        break;
    case wss_cmd_e_dev_recovery:
        code = exec_recovery_factory_handle(request, respone);
        break;
    case wss_cmd_r_ledctrl_state:
        code = read_ledctrl_state_handle(request, respone);
        break;  
    case wss_cmd_r_ledctrl_mode:
        code = read_ledctrl_mode_handle(request, respone);
        break;
    case wss_cmd_w_ledctrl_mode:
        code = write_ledctrl_mode_handle(request, respone);
        break;
    case wss_cmd_r_ledctrl_current_output:
        code = read_ledctrl_current_output_handle(request, respone);
        break;
    case wss_cmd_w_ledctrl_demo_output:
        code = write_ledctrl_demo_output_handle(request, respone);
        break;  
    case wss_cmd_r_ledctrl_manual:
        code = read_ledctrl_manual_handle(request, respone);
        break;  
    case wss_cmd_w_ledctrl_manual:
        code = write_ledctrl_manual_handle(request, respone);
        break;
    case wss_cmd_r_ledctrl_schedule:
        code = read_ledctrl_schedule_handle(request, respone);
        break;  
    case wss_cmd_w_ledctrl_schedule:
        code = write_ledctrl_schedule_handle(request, respone);
        break;   
    case wss_cmd_e_ota_begin:
        code = exec_ota_begin_handle(client, request, respone);
        break;   
    case wss_cmd_e_ota_done:
        code = exec_ota_done_handle(client, request, respone);
        break;     
    default:
        code = wss_respond_code_invalid_cmd;
        break;
    }
    _wss_tsk.clients[client].lifetime = WS_CLIENT_LIFETIME;
send_respond: 
    cJSON_AddStringToObject(respone, "token", token);   
    cJSON_AddNumberToObject(respone, "cmd",   cmd);
    cJSON_AddNumberToObject(respone, "code",  code);
    out = cJSON_PrintUnformatted(respone);
    cJSON_Delete(request);
    cJSON_Delete(respone);
    //ESP_LOGI(TAG, "send client %i message:\n%s", client, out);
    ws_server_send_text_client_from_callback( client, out, strlen(out));
    cJSON_free(out);
}