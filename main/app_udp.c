#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/err.h>
#include <esp_log.h>
#include "app_udp.h"

#define TAG                     "app_udpserver"
#define UDP_SERVER_PORT         37788
#define UDP_SERVER_RX_BUF_LEN   512
#define UDP_SERVER_TX_BUF_LEN   64

char _udp_server_rx_buf[UDP_SERVER_RX_BUF_LEN] = { 0 };
char _udp_server_tx_buf[UDP_SERVER_TX_BUF_LEN] = { 0 };

void initialise_udp(void)
{

}

void deinitialise_udp(void)
{

}

void app_udp_tsk(void * parm)
{
    int sock, err, len;
    struct sockaddr_in dest_addr;
    struct sockaddr_in6 source_addr; 
    socklen_t socklen;
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_SERVER_PORT);
    inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto exit;
    }

    err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto exit;
    }
    ESP_LOGI(TAG, "Socket bound %s:%d", addr_str, UDP_SERVER_PORT);

    while (1) {
        socklen = sizeof(source_addr);
        len = recvfrom(sock, _udp_server_rx_buf, UDP_SERVER_RX_BUF_LEN - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }
        // Get the sender's ip address as string
        if (source_addr.sin6_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        } else if (source_addr.sin6_family == PF_INET6) {
            inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
        _udp_server_rx_buf[len] = 0; // Null-terminate whatever we received and treat like a string...
        ESP_LOGI(TAG, "Received %d bytes from %s:%s", len, addr_str, _udp_server_rx_buf);
        int err = sendto(sock, _udp_server_rx_buf, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
        }
    }
exit:
    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);    
}