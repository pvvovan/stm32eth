#include <stdint.h>
#include "ksz8021rnl.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "ethernetif.h"

static volatile ETH_HandleTypeDef h_eth;

/* Ethernet Receive Buffer */
__ALIGN_BEGIN uint8_t rx_buf[ETH_RX_BUF_NUM][ETH_RX_BUF_SIZE] __ALIGN_END;

/* Ethernet Rx DMA Descriptor */
__ALIGN_BEGIN ETH_DMADescTypeDef  dma_rx_desc_tab[ETH_RX_BUF_NUM] __ALIGN_END;

/* Ethernet Transmit Buffer */
__ALIGN_BEGIN uint8_t tx_buf[ETH_TX_BUF_NUM][ETH_TX_BUF_SIZE] __ALIGN_END;

/* Ethernet Tx DMA Descriptor */
__ALIGN_BEGIN ETH_DMADescTypeDef  dma_tx_desc_tab[ETH_TX_BUF_NUM] __ALIGN_END;




static void low_level_init(struct netif *netif);






/**
  * @brief In this function, the hardware should be initialized.
  * Called from ethernetif_init().
  *
  * @param netif the already initialized lwip network interface structure
  *        for this ethernetif
  */
static void low_level_init(struct netif *netif)
{
    int status = -1;
    uint32_t regvalue = 0;
    uint8_t macaddress[6]= { MAC_ADDR0, MAC_ADDR1, MAC_ADDR2, MAC_ADDR3, MAC_ADDR4, MAC_ADDR5 };

    status = ksz8021_init((ETH_HandleTypeDef *)&h_eth, macaddress);
    if (status < 0)
    {
        return;
    }

    netif->flags |= NETIF_FLAG_LINK_UP;

    /* Initialize Rx Descriptors list: Chain Mode  */
    HAL_ETH_DMARxDescListInit((ETH_HandleTypeDef *)&h_eth, dma_rx_desc_tab, &rx_buf[0][0], ETH_RX_BUF_NUM);

    /* Initialize Tx Descriptors list: Chain Mode */
    HAL_ETH_DMATxDescListInit((ETH_HandleTypeDef *)&h_eth, dma_tx_desc_tab, &tx_buf[0][0], ETH_RX_BUF_NUM);

    /* set MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* set MAC hardware address */
    netif->hwaddr[0] =  MAC_ADDR0;
    netif->hwaddr[1] =  MAC_ADDR1;
    netif->hwaddr[2] =  MAC_ADDR2;
    netif->hwaddr[3] =  MAC_ADDR3;
    netif->hwaddr[4] =  MAC_ADDR4;
    netif->hwaddr[5] =  MAC_ADDR5;

    /* maximum transfer unit */
    netif->mtu = 1500;

    /* device capabilities */
    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    /* Enable MAC and DMA transmission and reception */
    HAL_ETH_Start((ETH_HandleTypeDef *)&h_eth);

    /**** Configure PHY to generate an interrupt when Eth Link state changes ****/
    /* Read Register Configuration */
    HAL_ETH_ReadPHYRegister((ETH_HandleTypeDef *)&h_eth, PHY_MICR, &regvalue);

    regvalue |= PHY_MISR_LINK_INT_EN;

    /* Enable Interrupt on change of link status */
    HAL_ETH_WritePHYRegister((ETH_HandleTypeDef *)&h_eth, PHY_MISR, regvalue);
}
