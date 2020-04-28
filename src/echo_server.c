#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "main.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/mem.h"
#include "echo_server.h"

#define RX_BUF_LEN          (1024)

static uint8_t rx_buf[RX_BUF_LEN];

void echo_server(void * const arg)
{
    int listenfd;
    int clientfd;
    int err;
    int read_len;
    struct sockaddr_in server_addr;
    struct sockaddr_storage client_addr;
    socklen_t client_len;
    struct netif *netif = (struct netif *)arg;

    memset(&server_addr, 0, sizeof(server_addr));

    /* Notify init task that echo server task has been started */
    xEventGroupSetBits(eg_task_started, EG_ECHO_SERVER_STARTED);

    listenfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    LWIP_ASSERT("echo_server(): Socket create failed", listenfd >= 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = PP_HTONL(INADDR_ANY);
    server_addr.sin_port = lwip_htons(ECHO_SERVER_PORT);

    err = lwip_bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err < 0) {
        LWIP_ASSERT("echo_server(): Socket bind failed", 0);
    }

    err = lwip_listen(listenfd, MAX_CLIENTS);
    if (err < 0) {
        LWIP_ASSERT("echo_server(): Socket listen failed", 0);
    }

    for (;;)
    {
        clientfd = lwip_accept(
                    listenfd,
                    (struct sockaddr *)&client_addr,
                    &client_len);

        if (clientfd < 0)
            continue;

        memset(rx_buf, 0, RX_BUF_LEN);

        read_len = lwip_read(clientfd, rx_buf, RX_BUF_LEN);
        if (read_len <= 0) {
            lwip_close(clientfd);
            continue;
        }

        lwip_write(clientfd, rx_buf, read_len);

        lwip_close(clientfd);
    }
}

