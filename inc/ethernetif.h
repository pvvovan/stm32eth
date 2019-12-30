#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "stm32f4xx_hal.h"

#define ETH_INT_PRIO    (8)

/* MAC ADDRESS: MAC_ADDR0:MAC_ADDR1:MAC_ADDR2:MAC_ADDR3:MAC_ADDR4:MAC_ADDR5 */
#define MAC_ADDR0   2U
#define MAC_ADDR1   0U
#define MAC_ADDR2   0U
#define MAC_ADDR3   0U
#define MAC_ADDR4   0U
#define MAC_ADDR5   0U

/* Definition of the Ethernet driver buffers size and count */
#define ETH_RX_BUF_SIZE                ETH_MAX_PACKET_SIZE /* buffer size for receive               */
#define ETH_TX_BUF_SIZE                ETH_MAX_PACKET_SIZE /* buffer size for transmit              */
#define ETH_RX_BUF_NUM                 4U                  /* 4 Rx buffers of size ETH_RX_BUF_SIZE  */
#define ETH_TX_BUF_NUM                 4U                  /* 4 Tx buffers of size ETH_TX_BUF_SIZE  */

err_t ethernetif_init(struct netif *netif);
void ethernetif_phy_irq();
void ethernetif_link_update(struct netif *netif);

#endif /* ETHERNETIF_H */
