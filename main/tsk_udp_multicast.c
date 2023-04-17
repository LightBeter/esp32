#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <time.h>
#include <esp_log.h>
#include "tsk_udp_multicast.h"
#include "tsk_ledctrl.h"
#include "tsk_ledctrl_misc.h"
#include "tsk_fanctrl.h"
#include "tsk_wifi.h"
#include "ws_cmd_handle.h"
#include "multicast.pb.h"
#include "pb_decode.h"
#include "misc.h"

#define TAG                                    "Multicast"
#define LISTEN_ALL_IF                          0
#define UDP_MULTICAST_ADDR                     "238.0.0.8"
#define UDP_MULTICAST_PORT                     9966
#define UDP_MULTI_RECV_TSK_STACK_SIZE          (4 * 1024)
#define UDP_MULTI_TSK_STACK_SIZE               (6 * 1024)
#define UDP_MULTICAST_DIST_MAX_SIZE            256

void multicast_recv_tsk(void * parm);
void multicast_handle_tsk(void * parm);
bool multicast_match_name(const char *list);
bool read_string(pb_istream_t *stream, const pb_field_t *field, void **arg);

static xTaskHandle     recv_tsk_handle = NULL;
static xTaskHandle     tsk_handle = NULL;
static QueueHandle_t   freeQueue = NULL;
static QueueHandle_t   handleQueue = NULL;
static bool udp_run_flag = false;
static char multicast_dist_list[UDP_MULTICAST_DIST_MAX_SIZE] = {0};
static udp_multicast_packet_t multicast_buffer[UDP_MULTICAST_PACKET_POOL_SIZE] = {0};


int socket_add_ipv4_multicast_group(int sock)
{
    struct ip_mreq imreq = { 0 };
    struct in_addr iaddr = { 0 };
    int err = 0;
    // Configure source interface
#if LISTEN_ALL_IF    
    imreq.imr_interface.s_addr = IPADDR_ANY;
#else
    esp_netif_ip_info_t ip_info = { 0 };
    err = esp_netif_get_ip_info(wifi_netif(), &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP address info. Error 0x%x", err);
        goto exit;
    }
    inet_addr_from_ip4addr(&iaddr, &ip_info.ip);
#endif // LISTEN_ALL_IF
    // Configure multicast address to listen to
    err = inet_aton(UDP_MULTICAST_ADDR, &imreq.imr_multiaddr.s_addr);
    if (err != 1) {
        ESP_LOGE(TAG, "Configured IPV4 multicast address '%s' is invalid.", UDP_MULTICAST_ADDR);
        // Errors in the return value have to be negative
        err = -1;
        goto exit;
    }

    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(TAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", UDP_MULTICAST_ADDR);
    }

    err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr, sizeof(struct in_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_MULTICAST_IF. Error %d", errno);
        goto exit;
    }

    err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d(%d)", err, errno);
        goto exit;
    }
 exit:
    return err;
}

int create_multicast_ipv4_socket(void)
{
    struct sockaddr_in saddr = { 0 };
    int sock, err;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(UDP_MULTICAST_PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
        goto err_handle;
    }

    // Assign multicast TTL (set separately from normal interface TTL)
    // uint8_t ttl = 5;
    // setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
    // if (err < 0) {
    //     ESP_LOGE(TAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
    //     goto err_handle;
    // }

    // this is also a listening socket, so add it to the multicast
    // group for listening...
    err = socket_add_ipv4_multicast_group(sock);
    if (err < 0) {
        goto err_handle;
    }
    return sock;

err_handle:
    close(sock);
    return -1;
}

void multicast_send_heartbeat(int sock)
{
    struct addrinfo hints = { .ai_flags = AI_PASSIVE, .ai_socktype = SOCK_DGRAM };
    struct addrinfo *res;
    char sendbuf[1] = { 0x00 };
    int err = getaddrinfo(UDP_MULTICAST_ADDR, NULL, &hints, &res);

    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(UDP_MULTICAST_PORT);
    ESP_LOGI(TAG, "Sending to multicast ...");

    err = sendto(sock, sendbuf, 1, 0, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (err < 0) {
        ESP_LOGE(TAG, "sendto failed. errno: %d", errno);
    }
}

void tsk_udp_multicast_init()
{
    handleQueue = xQueueCreate(UDP_MULTICAST_PACKET_POOL_SIZE, sizeof(udp_multicast_packet_t*));
    freeQueue = xQueueCreate(UDP_MULTICAST_PACKET_POOL_SIZE, sizeof(udp_multicast_packet_t*));
}

void tsk_udp_multicast_deinit()
{
    tsk_udp_multicast_stop();

    xQueueReset(handleQueue);
    xQueueReset(freeQueue);

    vQueueDelete(handleQueue);
    handleQueue = NULL;

    vQueueDelete(freeQueue);
    freeQueue = NULL;
}

void tsk_udp_multicast_start()
{
    udp_multicast_packet_t *packet = NULL;

    if (NULL == handleQueue || NULL == freeQueue) {
        ESP_LOGW(TAG, "UDP Multicase Tasks uninitialized");
        return;
    }

    if (tsk_handle || recv_tsk_handle || udp_run_flag) {
        ESP_LOGW(TAG, "UDP Multicase Tasks is running");
        return;
    }

    xQueueReset(handleQueue);
    xQueueReset(freeQueue);
    for (int i = 0; i < UDP_MULTICAST_PACKET_POOL_SIZE; i++) {
        packet = &multicast_buffer[i];
        xQueueSend(freeQueue, &packet, portMAX_DELAY);
    }

    udp_run_flag = true;
    if (!tsk_handle) {
        xTaskCreatePinnedToCore(multicast_handle_tsk, 
                    "multi_tsk", 
                    UDP_MULTI_TSK_STACK_SIZE,
                    NULL, 
                    TSK_PRIO_MULTI_HANDLE, 
                    &tsk_handle,
                    TSK_COREID_MULTI_HANDLE);
    }
    if (!recv_tsk_handle) {
        xTaskCreatePinnedToCore(multicast_recv_tsk, 
                    "multi_recv_tsk", 
                    UDP_MULTI_RECV_TSK_STACK_SIZE,
                    NULL, 
                    TSK_PRIO_MULTI_RECV, 
                    &recv_tsk_handle,
                    TSK_COREID_MULTI_RECV);
    }
}

void tsk_udp_multicast_stop()
{
    udp_run_flag = false;
    while (recv_tsk_handle || recv_tsk_handle) {
        ESP_LOGI(TAG, "waiting UDP Multicase Task Exit");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void multicast_recv_tsk(void* parm) 
{
    struct timeval tv = { 1, 0 };
    fd_set rfds;
    int s, socket = -1;
    struct sockaddr_storage raddr;
    socklen_t socklen;
    udp_multicast_packet_t *packet = NULL;
    time_t heartbeat_time = time(NULL);
    double diff = 0;

    socket = create_multicast_ipv4_socket();
    if (socket < 0)
        goto exit;

    ESP_LOGI(TAG, "Udp Multicase Start Recv");
    while (udp_run_flag) {
        
        if (!packet) {
            xQueueReceive(freeQueue, &packet, pdMS_TO_TICKS(1000));
        }

        if (!packet) {
            goto send_hearbeat;
        }

        FD_ZERO(&rfds);
        FD_SET(socket, &rfds);
        s = select(socket + 1, &rfds, NULL, NULL, &tv);
        if (s < 0) {
            ESP_LOGE(TAG, "Select failed: errno %d", errno);
            goto send_hearbeat;
        } else if (0 == s) {
            goto send_hearbeat;
        }
        
        if (!FD_ISSET(socket, &rfds)) {
            goto send_hearbeat;
        }
  
        socklen = sizeof(raddr);
        packet->len = recvfrom(socket, packet->data, UDP_MULTICAST_PACKET_MAX_SIZE, 0, (struct sockaddr*)&raddr, &socklen);
        if (packet->len <= 0 || packet->len > UDP_MULTICAST_PACKET_MAX_SIZE) {
            ESP_LOGE(TAG, "multicast recvfrom failed, size(%d) errno %d", packet->len, errno);
            goto send_hearbeat;
        } 
        
        if (1 == packet->len && 0x00 == packet->data[0]) {
            ESP_LOGI(TAG, "multicast recvfrom HeartBeat");
            goto send_hearbeat;
        }

        xQueueSend(handleQueue, &packet, portMAX_DELAY);
        packet = NULL;
        continue;

send_hearbeat:
        diff = difftime(time(NULL), heartbeat_time);
        if (diff >= 60) {
            heartbeat_time = time(NULL);
            multicast_send_heartbeat(socket);
        }
    }

    ESP_LOGI(TAG, "shutdown socket");
    shutdown(socket, 0);
    ESP_LOGI(TAG, "close socket");
    close(socket);
    socket = -1;
exit:
    recv_tsk_handle = NULL;
    vTaskDelete(NULL);
}

void multicast_handle_tsk(void* parm) 
{
    led_value_t values[LED_CHAN_COUNT] = { 0 };
    udp_multicast_packet_t *rawPacket = NULL;
    pb_istream_t stream;
    MulticastPacket packet = MulticastPacket_init_default;

    while (udp_run_flag) { 
        xQueueReceive(handleQueue, &rawPacket, pdMS_TO_TICKS(1000));
        if (NULL == rawPacket)
            continue; 

        memset(&packet, 0x00, sizeof(MulticastPacket));
        packet.dist.arg = multicast_dist_list;
        packet.dist.funcs.decode = read_string;
        stream = pb_istream_from_buffer(rawPacket->data, rawPacket->len);
        if (!pb_decode(&stream, MulticastPacket_fields, &packet)) {
            ESP_LOGE(TAG, "decode buffer(%d) failure: %s", rawPacket->len, PB_GET_ERROR(&stream));
            goto Next;
        }

#if defined(CONFIG_LOG_DEFAULT_LEVEL_INFO) && CONFIG_LOG_DEFAULT_LEVEL_INFO
        ESP_LOGI(TAG, "cmd[%d](%d): %s", packet.cmd, rawPacket->len, multicast_dist_list);
        if (packet.has_modeParam) 
            ESP_LOGI(TAG, "has modeParam sw[%u] mode[%d]", packet.modeParam.sw, packet.modeParam.mode);
        if (packet.has_manualParam) 
            ESP_LOGI(TAG, "has manualParam [%u,%u,%u,%u,%u,%u,%u]",
                            packet.manualParam.values.chan1, packet.manualParam.values.chan2,
                            packet.manualParam.values.chan3, packet.manualParam.values.chan4,
                            packet.manualParam.values.chan5, packet.manualParam.values.chan6,
                            packet.manualParam.values.chan7 );
        if (packet.has_playParam)
            ESP_LOGI(TAG, "has playParam [%u,%u,%u,%u,%u,%u,%u] play(%u)",
                            packet.playParam.values.chan1, packet.playParam.values.chan2,
                            packet.playParam.values.chan3, packet.playParam.values.chan4,
                            packet.playParam.values.chan5, packet.playParam.values.chan6,
                            packet.playParam.values.chan7, packet.playParam.play);
        if (packet.has_fanParam)
            ESP_LOGI(TAG, "has fanParam play(%d) duty(%d)", packet.fanParam.play, packet.fanParam.duty);
#endif

        if (!multicast_match_name(multicast_dist_list))
            goto Next;

        if (ws_cmd_w_ledctrl_mode == packet.cmd && packet.has_modeParam) {
            ledctrl_set_mode(packet.modeParam.sw, (ledctrl_mode_t)packet.modeParam.mode);
        }

        if (ws_cmd_w_ledctrl_manual == packet.cmd && packet.has_manualParam) {
            values[0] = packet.manualParam.values.chan1;
            values[1] = packet.manualParam.values.chan2;
            values[2] = packet.manualParam.values.chan3;
            values[3] = packet.manualParam.values.chan4;
            values[4] = packet.manualParam.values.chan5;
            values[5] = packet.manualParam.values.chan6;
            values[6] = packet.manualParam.values.chan7;
            if (ledctrl_set_manual(values))
                ledctrl_set_mode(true, LED_CTRL_MODE_MANUAL);
        }

        if (ws_cmd_w_ledctrl_demo_output == packet.cmd && packet.has_playParam) {
            values[0] = packet.playParam.values.chan1;
            values[1] = packet.playParam.values.chan2;
            values[2] = packet.playParam.values.chan3;
            values[3] = packet.playParam.values.chan4;
            values[4] = packet.playParam.values.chan5;
            values[5] = packet.playParam.values.chan6;
            values[6] = packet.playParam.values.chan7;
            ledctrl_set_demo(packet.playParam.play, values);
        }

        if (ws_cmd_w_fan_duty == packet.cmd && packet.has_fanParam) {
            if (packet.fanParam.duty > -1)
                fanctrl_set_duty(packet.fanParam.duty);
            
            if (packet.fanParam.play > -1)
                fanctrl_set_play(packet.fanParam.play);
        }
Next:
        xQueueSend(freeQueue, &rawPacket, portMAX_DELAY);
        rawPacket = NULL;
    }

    tsk_handle = NULL;
    vTaskDelete(NULL);
}

bool read_string(pb_istream_t *stream, const pb_field_t *field, void **arg)
{    
    memset(multicast_dist_list, 0x00, UDP_MULTICAST_DIST_MAX_SIZE);
    if (stream->bytes_left > UDP_MULTICAST_DIST_MAX_SIZE - 1)
        return false;
     
    if (!pb_read(stream, (pb_byte_t*)multicast_dist_list, stream->bytes_left))
        return false;
    
    return true;
}

bool multicast_match_name(const char *list)
{
    const char *name = hostname();
    const char *ptr  = list;
    uint32_t nameLen = strlen(name);
    uint32_t listLen  = strlen(list);

    if (listLen < nameLen) {
        return false;
    } else if (memcmp(ptr, name, nameLen) == 0) {
        return true;
    } else {
        ptr += 1;
        listLen -= 1;
    }
    while( listLen > nameLen ) {
        if (*ptr == ';') {
            if (memcmp(ptr + 1, name, nameLen) == 0)
                return true;
        } 
        ptr += 1;
        listLen -= 1;
    } 
    return false;
}