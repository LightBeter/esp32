#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <esp_log.h>
#include "udp_multicast_tsk.h"
#include "led_ctrl_tsk.h"
#include "led_ctrl_misc.h"
#include "temp_ctrl_tsk.h"
#include "wifi_tsk.h"
#include "wss_cmd_handle.h"
#include "multicast.pb.h"
#include "pb_decode.h"
#include "device.h"

#define TAG                                    "UDP_MULTICAST"
#define LISTEN_ALL_IF                          0
#define UDP_MULTICAST_ADDR                     "238.0.0.8"
#define UDP_MULTICAST_PORT                     9966
#define UDP_MULTICAST_RECV_TSK_STACK_SIZE      2048
#define UDP_MULTICAST_HANDLE_TSK_STACK_SIZE    3072
#define UDP_MULTICAST_DIST_MAX_SIZE            256

void udp_multicast_recv_tsk(void * parm);
void udp_multicast_handle_tsk(void * parm);
bool udp_multicast_match_name(const char *list);
bool read_string(pb_istream_t *stream, const pb_field_t *field, void **arg);

udp_multicast_tsk_type _udp_multicast_tsk;
udp_multicast_packet*  _udp_multicast_pool[UDP_MULTICAST_PACKET_POOL_SIZE] =  { 
    NULL, NULL, NULL, NULL, NULL, NULL
};
char _udp_multicast_dist[UDP_MULTICAST_DIST_MAX_SIZE];

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

void udp_multicast_tsk_init()
{
    _udp_multicast_tsk.recvTsk = NULL;
    _udp_multicast_tsk.handleTsk = NULL;
    _udp_multicast_tsk.sock = -1;
    _udp_multicast_tsk.handleQueue = xQueueCreate(UDP_MULTICAST_PACKET_POOL_SIZE, sizeof(udp_multicast_packet*));
    _udp_multicast_tsk.freeQueue = xQueueCreate(UDP_MULTICAST_PACKET_POOL_SIZE, sizeof(udp_multicast_packet*));
    for (int i = 0; i < UDP_MULTICAST_PACKET_POOL_SIZE; i++) {
        _udp_multicast_pool[i] =  (udp_multicast_packet*)malloc(sizeof(udp_multicast_packet));
        xQueueSend(_udp_multicast_tsk.freeQueue, &_udp_multicast_pool[i], portMAX_DELAY);
    }
}

void udp_multicast_tsk_deinit()
{
    xQueueReset(_udp_multicast_tsk.handleQueue);
    xQueueReset(_udp_multicast_tsk.freeQueue);
    for (int i = 0; i < UDP_MULTICAST_PACKET_POOL_SIZE; i++) {
        if (_udp_multicast_pool[i]) {
            free(_udp_multicast_pool[i]);
            _udp_multicast_pool[i] = NULL;
        }
    }
}

void udp_multicast_tsk_start()
{
    _udp_multicast_tsk.sock = create_multicast_ipv4_socket();
    if (_udp_multicast_tsk.sock < 0)
        return;

    if (!_udp_multicast_tsk.handleTsk) {
        xTaskCreate(udp_multicast_handle_tsk, 
                    "udp_multicast_handle_tsk", 
                    UDP_MULTICAST_HANDLE_TSK_STACK_SIZE, 
                    NULL, 
                    3, 
                    &_udp_multicast_tsk.handleTsk);
    }
    if (!_udp_multicast_tsk.recvTsk) {
        xTaskCreate(udp_multicast_recv_tsk, 
                    "udp_multicast_recv_tsk", 
                    UDP_MULTICAST_RECV_TSK_STACK_SIZE,
                    NULL, 
                    3, 
                    &_udp_multicast_tsk.recvTsk);
    }
}

void udp_multicast_tsk_stop()
{
    if (_udp_multicast_tsk.recvTsk) {
        vTaskDelete(_udp_multicast_tsk.recvTsk);
        _udp_multicast_tsk.recvTsk = NULL;
    }
    if (_udp_multicast_tsk.handleTsk) {
        vTaskDelete(_udp_multicast_tsk.handleTsk);
        _udp_multicast_tsk.handleTsk = NULL;
    }

    if (_udp_multicast_tsk.sock >= 0) {
        ESP_LOGE(TAG, "shutdown socket");
        shutdown(_udp_multicast_tsk.sock, 0);
        // ESP_LOGE(TAG, "close socket");
        // close(_udp_multicast_tsk.sock);
        _udp_multicast_tsk.sock = -1;
    }
}

void udp_multicast_recv_tsk(void* parm) 
{
    struct timeval tv = { 5, 0 };
    fd_set rfds;
    int s;
    struct sockaddr_storage raddr;
    socklen_t socklen;
    udp_multicast_packet *packet;

    ESP_LOGI(TAG, "Udp Multicase Start Recv");
    while (1) {
        if (!packet)
            xQueueReceive(_udp_multicast_tsk.freeQueue, &packet, portMAX_DELAY);

        if (!packet)
            continue;

        FD_ZERO(&rfds);
        FD_SET(_udp_multicast_tsk.sock, &rfds);
        s = select(_udp_multicast_tsk.sock + 1, &rfds, NULL, NULL, &tv);
        if (s < 0) {
            ESP_LOGE(TAG, "Select failed: errno %d", errno);
            continue;
        } else if (0 == s) {
            continue;
        }
        
        if (!FD_ISSET(_udp_multicast_tsk.sock, &rfds))
            continue;
  
        socklen = sizeof(raddr);
        packet->len = recvfrom(_udp_multicast_tsk.sock, packet->data, UDP_MULTICAST_PACKET_MAX_SIZE, 0, (struct sockaddr*)&raddr, &socklen);
        if (packet->len <= 0 || packet->len > UDP_MULTICAST_PACKET_MAX_SIZE) {
            ESP_LOGE(TAG, "multicast recvfrom failed, size(%d) errno %d", packet->len, errno );
            continue;
        }

        xQueueSend(_udp_multicast_tsk.handleQueue, &packet, portMAX_DELAY);
        packet = NULL;
    }
    _udp_multicast_tsk.recvTsk = NULL;
    vTaskDelete(NULL);
}

void udp_multicast_handle_tsk(void* parm) 
{
    udp_multicast_packet *rawPacket = NULL;
    pb_istream_t    stream;
    led_value_t values[LED_CHAN_COUNT];

    while (1) { 
        xQueueReceive(_udp_multicast_tsk.handleQueue, &rawPacket, portMAX_DELAY);
        if (NULL == rawPacket)
            continue; 

        MulticastPacket packet = MulticastPacket_init_default;
        packet.dist.arg = _udp_multicast_dist;
        packet.dist.funcs.decode = read_string;
        stream = pb_istream_from_buffer(rawPacket->data, rawPacket->len);
        if (!pb_decode(&stream, MulticastPacket_fields, &packet)) {
            ESP_LOGE(TAG, "decode buffer(%d) failure: %s", rawPacket->len, PB_GET_ERROR(&stream));
            goto Next;
        }
#if 1
        ESP_LOGI(TAG, "multicast cmd[%d](%d): %s", packet.cmd, rawPacket->len, _udp_multicast_dist);
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
        if (!udp_multicast_match_name(_udp_multicast_dist))
            goto Next;

        if (wss_cmd_w_ledctrl_mode == packet.cmd && packet.has_modeParam) {
            led_ctrl_set_mode_ex(packet.modeParam.sw, (led_ctrl_mode_t)packet.modeParam.mode);
        }

        if (wss_cmd_w_ledctrl_manual == packet.cmd && packet.has_manualParam) {
            values[0] = packet.manualParam.values.chan1;
            values[1] = packet.manualParam.values.chan2;
            values[2] = packet.manualParam.values.chan3;
            values[3] = packet.manualParam.values.chan4;
            values[4] = packet.manualParam.values.chan5;
            values[5] = packet.manualParam.values.chan6;
            values[6] = packet.manualParam.values.chan7;
            if (led_ctrl_set_manual_ex(values))
                led_ctrl_set_mode_ex(true, LED_CTRL_MODE_MANUAL);
        }

        if (wss_cmd_w_ledctrl_demo_output == packet.cmd && packet.has_playParam) {
            values[0] = packet.playParam.values.chan1;
            values[1] = packet.playParam.values.chan2;
            values[2] = packet.playParam.values.chan3;
            values[3] = packet.playParam.values.chan4;
            values[4] = packet.playParam.values.chan5;
            values[5] = packet.playParam.values.chan6;
            values[6] = packet.playParam.values.chan7;
            led_ctrl_set_demo_ex(packet.playParam.play, values);
        }
#if defined(FAN_PWM_CTRL) && (FAN_PWM_CTRL == 1)
        if (wss_cmd_w_fan_duty == packet.cmd && packet.has_fanParam) {
            if (packet.fanParam.duty > -1)
                temp_ctrl_fan_duty_set(packet.fanParam.duty);
            
            if (packet.fanParam.play > -1)
                temp_ctrl_fan_play(packet.fanParam.play);
        }
#endif
Next:
        xQueueSend(_udp_multicast_tsk.freeQueue, &rawPacket, portMAX_DELAY);
        rawPacket = NULL;
    }
}

bool read_string(pb_istream_t *stream, const pb_field_t *field, void **arg)
{    
    memset(_udp_multicast_dist, 0x00, UDP_MULTICAST_DIST_MAX_SIZE);
    if (stream->bytes_left > UDP_MULTICAST_DIST_MAX_SIZE - 1)
        return false;
     
    if (!pb_read(stream, (pb_byte_t*)_udp_multicast_dist, stream->bytes_left))
        return false;
    
    return true;
}

bool udp_multicast_match_name(const char *list)
{
    const char *name = device_hostname();
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