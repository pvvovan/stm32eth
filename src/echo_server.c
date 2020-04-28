#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "main.h"


void echo_server(void * const arg)
{
    struct netif *netif = (struct netif *)arg;

    /* Notify init task that echo server task has been started */
    xEventGroupSetBits(eg_task_started, EG_ECHO_SERVER_STARTED);

    for (;;)
    {
    }
}

