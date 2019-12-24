#include <stdint.h>
#include <string.h>
#include "ksz8081rnd.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "ethernetif.h"

/* Network interface name */
#define IFNAME0 'G'
#define IFNAME1 'L'

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
static err_t low_level_output(struct netif *netif, struct pbuf *p);
static struct pbuf * low_level_input(struct netif *netif);
__weak void ethernetif_notify_conn_changed(struct netif *netif);

/**
  * @brief In this function, the hardware should be initialized.
  * Called from ethernetif_init().
  *
  * @param netif the already initialized lwip network interface structure
  *        for this ethernetif
  */
static void low_level_init(struct netif *netif)
{
    PHY_STATUS_E status = E_PHY_STATUS_ERROR;
    uint32_t regval = 0;
    uint8_t macaddress[6]= { MAC_ADDR0, MAC_ADDR1, MAC_ADDR2, MAC_ADDR3, MAC_ADDR4, MAC_ADDR5 };

    status = ksz8081_init((ETH_HandleTypeDef *)&h_eth, macaddress);
    if (status == E_PHY_STATUS_ERROR)
    {
        return;
    }
    else if (status == E_PHY_STATUS_OK)
    {
        netif->flags |= NETIF_FLAG_LINK_UP;
    }

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
    HAL_ETH_ReadPHYRegister((ETH_HandleTypeDef *)&h_eth, PHY_MICR, &regval);

    regval |= PHY_MISR_LINK_INT_EN;

    /* Enable Interrupt on change of link status */
    HAL_ETH_WritePHYRegister((ETH_HandleTypeDef *)&h_eth, PHY_MISR, regval);
}

/**
  * @brief Should be called at the beginning of the program to set up the
  * network interface. It calls the function low_level_init() to do the
  * actual setup of the hardware.
  *
  * This function should be passed as a parameter to netif_add().
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return ERR_OK if the loopif is initialized
  *         ERR_MEM if private data couldn't be allocated
  *         any other err_t on error
  */
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    #if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwip";
    #endif /* LWIP_NETIF_HOSTNAME */

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    /* We directly use etharp_output() here to save a function call.
    * You can instead declare your own function an call etharp_output()
    * from it if you have to do some checks before sending (e.g. if link
    * is available...) */
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    /* initialize the hardware */
    low_level_init(netif);

    return ERR_OK;
}

/**
  * @brief This function should do the actual transmission of the packet. The packet is
  * contained in the pbuf that is passed to the function. This pbuf
  * might be chained.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
  * @return ERR_OK if the packet could be sent
  *         an err_t value if the packet couldn't be sent
  *
  * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
  *       strange results. You might consider waiting for space in the DMA queue
  *       to become availale since the stack doesn't retry to send a packet
  *       dropped because of memory failure (except for the TCP timers).
  */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    err_t errval;
    struct pbuf *q;
    uint8_t *buffer = (uint8_t *)(h_eth.TxDesc->Buffer1Addr);
    __IO ETH_DMADescTypeDef *dma_tx_desc;
    uint32_t framelength = 0;
    uint32_t bufferoffset = 0;
    uint32_t byteslefttocopy = 0;
    uint32_t payloadoffset = 0;

    dma_tx_desc = h_eth.TxDesc;
    bufferoffset = 0;

    /* copy frame from pbufs to driver buffers */
    for(q = p; q != NULL; q = q->next)
    {
        /* Is this buffer available? If not, goto error */
        if((dma_tx_desc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
        {
            errval = ERR_USE;
            goto error;
        }

        /* Get bytes in current lwIP buffer */
        byteslefttocopy = q->len;
        payloadoffset = 0;

        /* Check if the length of data to copy is bigger than Tx buffer size*/
        while ((byteslefttocopy + bufferoffset) > ETH_TX_BUF_SIZE)
        {
            /* Copy data to Tx buffer*/
            memcpy( (uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), (ETH_TX_BUF_SIZE - bufferoffset) );

            /* Point to next descriptor */
            dma_tx_desc = (ETH_DMADescTypeDef *)(dma_tx_desc->Buffer2NextDescAddr);

            /* Check if the buffer is available */
            if((dma_tx_desc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
            {
                errval = ERR_USE;
                goto error;
            }

            buffer = (uint8_t *)(dma_tx_desc->Buffer1Addr);

            byteslefttocopy = byteslefttocopy - (ETH_TX_BUF_SIZE - bufferoffset);
            payloadoffset = payloadoffset + (ETH_TX_BUF_SIZE - bufferoffset);
            framelength = framelength + (ETH_TX_BUF_SIZE - bufferoffset);
            bufferoffset = 0;
        }

        /* Copy the remaining bytes */
        memcpy((uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), byteslefttocopy);
        bufferoffset = bufferoffset + byteslefttocopy;
        framelength = framelength + byteslefttocopy;
    }

    /* Prepare transmit descriptors to give to DMA */
    HAL_ETH_TransmitFrame((ETH_HandleTypeDef *)&h_eth, framelength);

    errval = ERR_OK;

error:
    /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
    if ((h_eth.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET)
    {
        /* Clear TUS ETHERNET DMA flag */
        h_eth.Instance->DMASR = ETH_DMASR_TUS;

        /* Resume DMA transmission*/
        h_eth.Instance->DMATPDR = 0;
    }

    return errval;
}

/**
  * @brief Should allocate a pbuf and transfer the bytes of the incoming
  * packet from the interface into the pbuf.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return a pbuf filled with the received packet (including MAC header)
  *         NULL on memory error
  */
static struct pbuf * low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;
    struct pbuf *q;
    uint16_t len;
    uint8_t *buffer;
    __IO ETH_DMADescTypeDef *dma_rx_desc;
    uint32_t bufferoffset = 0;
    uint32_t payloadoffset = 0;
    uint32_t byteslefttocopy = 0;
    uint32_t i = 0;

    if (HAL_ETH_GetReceivedFrame((ETH_HandleTypeDef *)&h_eth) != HAL_OK)
    {
        return NULL;
    }

    /* Obtain the size of the packet and put it into the "len" variable. */
    len = h_eth.RxFrameInfos.length;
    buffer = (uint8_t *)h_eth.RxFrameInfos.buffer;

    if (len > 0)
    {
        /* We allocate a pbuf chain of pbufs from the Lwip buffer pool */
        p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    }

    if (p != NULL)
    {
        dma_rx_desc = h_eth.RxFrameInfos.FSRxDesc;
        bufferoffset = 0;

        for (q = p; q != NULL; q = q->next)
        {
            byteslefttocopy = q->len;
            payloadoffset = 0;

            /* Check if the length of bytes to copy in current pbuf is bigger than Rx buffer size */
            while ((byteslefttocopy + bufferoffset) > ETH_RX_BUF_SIZE)
            {
                /* Copy data to pbuf */
                memcpy( (uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), (ETH_RX_BUF_SIZE - bufferoffset));

                /* Point to next descriptor */
                dma_rx_desc = (ETH_DMADescTypeDef *)(dma_rx_desc->Buffer2NextDescAddr);
                buffer = (uint8_t *)(dma_rx_desc->Buffer1Addr);

                byteslefttocopy = byteslefttocopy - (ETH_RX_BUF_SIZE - bufferoffset);
                payloadoffset = payloadoffset + (ETH_RX_BUF_SIZE - bufferoffset);
                bufferoffset = 0;
            }

            /* Copy remaining data in pbuf */
            memcpy( (uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), byteslefttocopy);
            bufferoffset = bufferoffset + byteslefttocopy;
        }
    }

    /* Release descriptors to DMA */
    /* Point to first descriptor */
    dma_rx_desc = h_eth.RxFrameInfos.FSRxDesc;
    /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
    for (i = 0; i < h_eth.RxFrameInfos.SegCount; i++)
    {
        dma_rx_desc->Status |= ETH_DMARXDESC_OWN;
        dma_rx_desc = (ETH_DMADescTypeDef *)(dma_rx_desc->Buffer2NextDescAddr);
    }

    /* Clear Segment_Count */
    h_eth.RxFrameInfos.SegCount =0;

    /* When Rx Buffer unavailable flag is set: clear it and resume reception */
    if ((h_eth.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)
    {
        /* Clear RBUS ETHERNET DMA flag */
        h_eth.Instance->DMASR = ETH_DMASR_RBUS;
        /* Resume DMA reception */
        h_eth.Instance->DMARPDR = 0;
    }

    return p;
}

/**
  * @brief This function should be called when a packet is ready to be read
  * from the interface. It uses the function low_level_input() that
  * should handle the actual reception of bytes from the network
  * interface. Then the type of the received packet is determined and
  * the appropriate input function is called.
  *
  * @param netif the lwip network interface structure for this ethernetif
  */
void ethernetif_input(struct netif *netif)
{
    err_t err;
    struct pbuf *p;

    /* move received packet into a new pbuf */
    p = low_level_input(netif);

    /* no packet could be read, silently ignore this */
    if (p == NULL)
    {
        return;
    }

    /* entry point to the LwIP stack */
    err = netif->input(p, netif);

    if (err != ERR_OK)
    {
        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
        pbuf_free(p);
        p = NULL;
    }
}

/**
  * @brief  This function sets the netif link status.
  * @param  netif: the network interface
  * @retval None
  */
void ethernetif_set_link(struct netif *netif)
{
    uint32_t regval = 0;

    /* Read PHY_MISR*/
    HAL_ETH_ReadPHYRegister((ETH_HandleTypeDef *)&h_eth, PHY_MISR, &regval);

    if ((regval & PHY_LINK_INT_UP_MASK) != (uint16_t)RESET)
    {
        netif_set_link_up(netif);
    }
    else if ((regval & PHY_LINK_INT_DOWN_MASK) != (uint16_t)RESET)
    {
        netif_set_link_down(netif);
    }
}

/**
  * @brief  Link callback function, this function is called on change of link status
  *         to update low level driver configuration.
* @param  netif: The network interface
  * @retval None
  */
void ethernetif_update_config(struct netif *netif)
{
    __IO uint32_t tickstart = 0;
    uint32_t regval = 0;

    if (netif_is_link_up(netif))
    {
        /* Restart the auto-negotiation */
        if (h_eth.Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE)
        {
            /* Enable Auto-Negotiation */
            HAL_ETH_WritePHYRegister((ETH_HandleTypeDef *)&h_eth, PHY_BCR, PHY_AUTONEGOTIATION);

            /* Get tick */
            tickstart = HAL_GetTick();

            /* Wait until the auto-negotiation will be completed */
            do
            {
                HAL_ETH_ReadPHYRegister((ETH_HandleTypeDef *)&h_eth, PHY_BSR, &regval);

                /* Check for the Timeout ( 1s ) */
                if ((HAL_GetTick() - tickstart ) > 1000)
                {
                    /* In case of timeout */
                    goto error;
                }

            } while (((regval & PHY_AUTONEGO_COMPLETE) != PHY_AUTONEGO_COMPLETE));

            /* Read the result of the auto-negotiation */
            HAL_ETH_ReadPHYRegister((ETH_HandleTypeDef *)&h_eth, PHY_SR, &regval);

            /* Configure the MAC with the Duplex Mode fixed by the auto-negotiation process */
            if ((regval & PHY_DUPLEX_STATUS) != (uint32_t)RESET)
            {
                /* Set Ethernet duplex mode to Full-duplex following the auto-negotiation */
                h_eth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
            }
            else
            {
                /* Set Ethernet duplex mode to Half-duplex following the auto-negotiation */
                h_eth.Init.DuplexMode = ETH_MODE_HALFDUPLEX;
            }
            /* Configure the MAC with the speed fixed by the auto-negotiation process */
            if (regval & PHY_SPEED_STATUS)
            {
                /* Set Ethernet speed to 10M following the auto-negotiation */
                h_eth.Init.Speed = ETH_SPEED_10M;
            }
            else
            {
                /* Set Ethernet speed to 100M following the auto-negotiation */
                h_eth.Init.Speed = ETH_SPEED_100M;
            }
        }
        else /* AutoNegotiation Disable */
        {
error :
            /* Check parameters */
            assert_param(IS_ETH_SPEED(h_eth.Init.Speed));
            assert_param(IS_ETH_DUPLEX_MODE(h_eth.Init.DuplexMode));

            /* Set MAC Speed and Duplex Mode to PHY */
            HAL_ETH_WritePHYRegister(
                (ETH_HandleTypeDef *)&h_eth,
                PHY_BCR,
                ((uint16_t)(h_eth.Init.DuplexMode >> 3) | (uint16_t)(h_eth.Init.Speed >> 1)));
        }

        /* ETHERNET MAC Re-Configuration */
        HAL_ETH_ConfigMAC((ETH_HandleTypeDef *)&h_eth, (ETH_MACInitTypeDef *) NULL);

        /* Restart MAC interface */
        HAL_ETH_Start((ETH_HandleTypeDef *)&h_eth);
    }
    else
    {
        /* Stop MAC interface */
        HAL_ETH_Stop((ETH_HandleTypeDef *)&h_eth);
    }

    ethernetif_notify_conn_changed(netif);
}

/**
  * @brief  This function notify user about link status changement.
  * @param  netif: the network interface
  * @retval None
  */
__weak void ethernetif_notify_conn_changed(struct netif *netif)
{
  /* NOTE : This is function clould be implemented in user file
            when the callback is needed,
  */
}

/**
  * @brief  Returns the current time in milliseconds
  *         when LWIP_TIMERS == 1 and NO_SYS == 1
  * @param  None
  * @retval Current Time value
  */
u32_t sys_now(void)
{
    return HAL_GetTick();
}
