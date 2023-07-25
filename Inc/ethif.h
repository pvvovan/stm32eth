#ifndef ETHERNET_INTERFACE_H
#define ETHERNET_INTERFACE_H

#include "lwip/err.h"
#include "lwip/netif.h"

enum list_t { LINK_UP, LINK_DOWN, LINK_UNCHANGED, LINK_ERROR };

void ethmac_init(void);
err_t ethernetif_init(struct netif *netif);
struct pbuf *low_level_input(struct netif *netif);
enum list_t ethphy_getlink(void);

#endif /* ETHERNET_INTERFACE_H */
