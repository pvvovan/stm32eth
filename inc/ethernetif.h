#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "stm32f4xx_hal.h"

/* Definition of the Ethernet driver buffers size and count */
#define ETH_RX_BUF_SIZE                ETH_MAX_PACKET_SIZE /* buffer size for receive               */
#define ETH_TX_BUF_SIZE                ETH_MAX_PACKET_SIZE /* buffer size for transmit              */
#define ETH_RX_BUF_NUM                 4U                  /* 4 Rx buffers of size ETH_RX_BUF_SIZE  */
#define ETH_TX_BUF_NUM                 4U                  /* 4 Tx buffers of size ETH_TX_BUF_SIZE  */

#endif /* ETHERNETIF_H */
