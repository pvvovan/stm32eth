#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/snmp.h"
#include "netif/ethernet.h"

#include "stm32f4xx_hal.h"

#include "hw_delay.h"
#include "ethif.h"


#define ETH_DMA_TRANSMIT_TIMEOUT		20U

typedef struct
{
	struct pbuf_custom pbuf_custom;
	uint8_t buff[(ETH_RX_BUF_SIZE + 31) & ~31] __ALIGNED(32);
} RxBuff_t;

typedef enum
{
	RX_ALLOC_OK		= 0x00,
	RX_ALLOC_ERROR		= 0x01
} RxAllocStatusTypeDef;

/* Memory Pool Declaration */
#define ETH_RX_BUFFER_CNT		12U
LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "Zero-copy RX PBUF pool")

static uint8_t RxAllocStatus;
static ETH_HandleTypeDef s_heth;
static ETH_TxPacketConfig TxConfig;


#define RMII_PHY_RST_PORT			GPIOD
#define RMII_PHY_RST_PIN			GPIO_PIN_10
#define RMII_CSR_DV_PORT			GPIOA
#define RMII_CSR_DV_PIN				GPIO_PIN_7
#define KSZ8081_RESET_ASSERT_DELAY_US		500
#define KSZ8081_BOOTUP_DELAY_US			100

static void ksz8081_bootstrap(void)
{
	GPIO_InitTypeDef gpio = { 0 };

	/* Enable GPIOs clocks */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	gpio.Speed = GPIO_SPEED_MEDIUM;
	gpio.Mode = GPIO_MODE_OUTPUT_OD;
	gpio.Pull = GPIO_NOPULL;
	gpio.Pin = RMII_PHY_RST_PIN;
	HAL_GPIO_Init(RMII_PHY_RST_PORT, &gpio);

	gpio.Speed = GPIO_SPEED_MEDIUM;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Pin = RMII_CSR_DV_PIN;
	HAL_GPIO_Init(RMII_CSR_DV_PORT, &gpio);

	/* Reset PHY */
	HAL_GPIO_WritePin(RMII_PHY_RST_PORT, RMII_PHY_RST_PIN, GPIO_PIN_RESET);
	/* Set PHY address to 0x03 */
	HAL_GPIO_WritePin(RMII_CSR_DV_PORT, RMII_CSR_DV_PIN, GPIO_PIN_SET);
	/* Reset pin should be asserted for minimum 500 us */
	delay_us(KSZ8081_RESET_ASSERT_DELAY_US);
	/* Bootup PHY */
	HAL_GPIO_WritePin(RMII_PHY_RST_PORT, RMII_PHY_RST_PIN, GPIO_PIN_SET);
	/* Bootup delay should be minimum 100 us */
	delay_us(KSZ8081_BOOTUP_DELAY_US);

	HAL_GPIO_DeInit(RMII_CSR_DV_PORT, RMII_CSR_DV_PIN);
}


/* PHY registers */
#define PHY_BASIC_CONTROL			((uint16_t)0x0000)
#define PHY_BASIC_STATUS			((uint16_t)0x0001)
#define PHY_IDENTIFIER1				((uint16_t)0x0002)
#define PHY_IDENTIFIER2				((uint16_t)0x0003)
#define PHY_AUTONEG_ADVERTISEMENT		((uint16_t)0x0004)
#define PHY_AUTONEG_LINK_PARTNER_ABILITY	((uint16_t)0x0005)
#define PHY_AUTONEG_EXPANSION			((uint16_t)0x0006)
#define PHY_AUTONEG_NEXT_PAGE			((uint16_t)0x0007)
#define PHY_LINK_PARTNER_NEXT_PAGE_ABILITY	((uint16_t)0x0008)
#define PHY_DIGITAL_RESERVED_CONTROL		((uint16_t)0x0010)
#define PHY_AFE_CONTROL1			((uint16_t)0x0011)
#define PHY_RX_ERROR_COUNTER			((uint16_t)0x0015)
#define PHY_OPERATION_MODE_STRAP_OVERRIDE	((uint16_t)0x0016)
#define PHY_OPERATION_MODE_STRAP_STATUS		((uint16_t)0x0017)
#define PHY_EXPANDED_CONTROL			((uint16_t)0x0018)
#define PHY_INTERRUPT_CONTROL			((uint16_t)0x001B)
#define PHY_INTERRUPT_STATUS			((uint16_t)0x001B)
#define PHY_LINKMD_CONTROL			((uint16_t)0x001D)
#define PHY_LINKMD_STATUS			((uint16_t)0x001D)
#define PHY_CONTROL1				((uint16_t)0x001E)
#define PHY_CONTROL2				((uint16_t)0x001F)

/* PHY masks */
#define PHY_REF_CLOCK_SELECT_MASK		((uint16_t)0x0080)
#define PHY_REF_CLOCK_SELECT_25MHZ		((uint16_t)0x0080)
#define PHY_LINK_INT_UP_OCCURRED		((uint16_t)0x0001)
#define PHY_LINK_INT_DOWN_OCCURED		((uint16_t)0x0004)


void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
	ksz8081_bootstrap();

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* Enable Peripheral clock */
	__HAL_RCC_ETH_CLK_ENABLE();
	__HAL_RCC_ETHMAC_CLK_ENABLE();
	__HAL_RCC_ETHMACTX_CLK_ENABLE();
	__HAL_RCC_ETHMACRX_CLK_ENABLE();

	/* ETH GPIO Configuration
		PC1	------> ETH_MDC
		PA1	------> ETH_REF_CLK
		PA2	------> ETH_MDIO
		PA7	------> ETH_CRS_DV
		PC4	------> ETH_RXD0
		PC5	------> ETH_RXD1
		PB11	------> ETH_TX_EN
		PB12	------> ETH_TXD0
		PB13	------> ETH_TXD1
	*/
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	HAL_ETH_SetMDIOClockRange(heth);

	uint32_t phyreg = 0U;
	HAL_StatusTypeDef status = HAL_ETH_ReadPHYRegister(heth, 0, PHY_CONTROL2, &phyreg);
	if (status != HAL_OK) {
		return;
	}

	/* Set 25MHz clock mode to enable 50 MHz clock on REF_CLK pin
	 * Note: After default bootstrap KSZ8081RND has 50MHz clock mode set
	 * thus REF_CLK pin is not connected and MAC module is
	 * not clocking. So bit ETH_DMABMR_SR in DMABMR register
	 * of MAC subsystem will never cleared */
	phyreg &= (uint16_t)(~(PHY_REF_CLOCK_SELECT_MASK));
	phyreg |= (PHY_REF_CLOCK_SELECT_25MHZ);
	status = HAL_ETH_WritePHYRegister(heth, 0, PHY_CONTROL2, phyreg);
	if (status != HAL_OK) {
		return;
	}
}

struct pbuf *low_level_input(struct netif *netif)
{
	(void)netif;
	struct pbuf *p = NULL;

	__asm volatile ("dmb" : : : "memory");
	if (RxAllocStatus == RX_ALLOC_OK) {
		if (HAL_ETH_ReadData(&s_heth, (void **)&p) != HAL_OK) {
			p = NULL;
		}
	}

	return p;
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	(void)netif;
	uint32_t i = 0U;
	struct pbuf *q = NULL;
	err_t errval = ERR_OK;
	ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT] = { 0 };

	for (q = p; q != NULL; q = q->next) {
		if (i >= ETH_TX_DESC_CNT) {
			return ERR_IF;
		}

		Txbuffer[i].buffer = q->payload;
		Txbuffer[i].len = q->len;

		if (i > 0) {
			Txbuffer[i-1].next = &Txbuffer[i];
		}

		if (q->next == NULL) {
			Txbuffer[i].next = NULL;
		}

		i++;
	}

	TxConfig.Length = p->tot_len;
	TxConfig.TxBuffer = Txbuffer;
	TxConfig.pData = p;

	HAL_StatusTypeDef err_hal = HAL_ETH_Transmit(&s_heth, &TxConfig, ETH_DMA_TRANSMIT_TIMEOUT);
	if (err_hal != HAL_OK) {
		errval = ERR_IF;
	}

	return errval;
}

static void low_level_init(struct netif *netif)
{
	TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
	TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
	TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

	netif->flags |= NETIF_FLAG_LINK_UP;

	/* set MAC hardware address length */
	netif->hwaddr_len = ETHARP_HWADDR_LEN;

	/* set MAC hardware address */
	netif->hwaddr[0] = MAC_ADDR0;
	netif->hwaddr[1] = MAC_ADDR1;
	netif->hwaddr[2] = MAC_ADDR2;
	netif->hwaddr[3] = MAC_ADDR3;
	netif->hwaddr[4] = MAC_ADDR4;
	netif->hwaddr[5] = MAC_ADDR5;

	/* maximum transfer unit */
	netif->mtu = 1500;

	/* device capabilities */
	/* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
	netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

	/* Enable MAC and DMA transmission and reception */
	HAL_ETH_Start(&s_heth);
}

err_t ethernetif_init(struct netif *netif)
{
	LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

	/* Initialize the snmp variables and counters inside the struct netif.
	 * The last argument should be replaced with your link speed, in units
	 * of bits per second. */
	MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000L);

	netif->name[0] = 'P';
	netif->name[1] = 'v';
	/* We directly use etharp_output() here to save a function call.
	 * You can instead declare your own function an call etharp_output()
	 * from it if you have to do some checks before sending (e.g. if link
	 * is available...) */

#if LWIP_IPV4
#if LWIP_ARP || LWIP_ETHERNET
#if LWIP_ARP
	netif->output = etharp_output;
#else
	/* The user should write its own code in low_level_output_arp_off function */
	netif->output = low_level_output_arp_off;
#endif /* LWIP_ARP */
#endif /* LWIP_ARP || LWIP_ETHERNET */
#endif /* LWIP_IPV4 */

#if LWIP_IPV6
	netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */

	netif->linkoutput = low_level_output;

	LWIP_MEMPOOL_INIT(RX_POOL);

	/* initialize the hardware */
	low_level_init(netif);

	return ERR_OK;
}

static void pbuf_free_custom(struct pbuf *p)
{
	struct pbuf_custom* custom_pbuf = (struct pbuf_custom*)p;
	LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);

	/* If the Rx Buffer Pool was exhausted, signal the ethernetif_input task to
	 * call HAL_ETH_GetRxDataBuffer to rebuild the Rx descriptors. */
	__asm volatile ("dmb" : : : "memory");
	if (RxAllocStatus == RX_ALLOC_ERROR) {
		RxAllocStatus = RX_ALLOC_OK;
		__asm volatile ("dmb" : : : "memory");
	}
}

void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
	struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);
	if (p) {
		/* Get the buff from the struct pbuf address. */
		*buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
		p->custom_free_function = pbuf_free_custom;

		/* Initialize the struct pbuf.
		 * This must be performed whenever a buffer's allocated because it may be
		 * changed by lwIP or the app, e.g., pbuf_free decrements ref. */
		pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUF_SIZE);
	} else {
		RxAllocStatus = RX_ALLOC_ERROR;
		__asm volatile ("dmb" : : : "memory");
		*buff = NULL;
	}
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
	struct pbuf **ppStart = (struct pbuf **)pStart;
	struct pbuf **ppEnd = (struct pbuf **)pEnd;
	struct pbuf *p = NULL;

	/* Get the struct pbuf from the buff address. */
	p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
	p->next = NULL;
	p->tot_len = 0;
	p->len = Length;

	/* Chain the buffer. */
	if (!*ppStart) {
		/* The first buffer of the packet. */
		*ppStart = p;
	} else {
		/* Chain the buffer to the end of the packet. */
		(*ppEnd)->next = p;
	}
	*ppEnd  = p;

	/* Update the total length of all the buffers of the chain. Each pbuf in the chain should have its tot_len
	 * set to its own length, plus the length of all the following pbufs in the chain. */
	for (p = *ppStart; p != NULL; p = p->next) {
		p->tot_len = (uint16_t)(p->tot_len + Length);
	}
}

void ethmac_init(void)
{
	static uint8_t MACAddr[6] = {
		MAC_ADDR0,
		MAC_ADDR1,
		MAC_ADDR2,
		MAC_ADDR3,
		MAC_ADDR4,
		MAC_ADDR5
	};

	static ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
	static ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

	s_heth.Instance = ETH;
	s_heth.Init.MACAddr = &MACAddr[0];
	s_heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
	s_heth.Init.TxDesc = DMATxDscrTab;
	s_heth.Init.RxDesc = DMARxDscrTab;
	s_heth.Init.RxBuffLen = 1536;

	HAL_ETH_Init(&s_heth);
}

enum link_status ethphy_getlink(void)
{
	uint32_t regval = 0;
	HAL_StatusTypeDef status = HAL_ETH_ReadPHYRegister(&s_heth, 0, PHY_INTERRUPT_STATUS, &regval);
	if (status == HAL_OK) {
		if (regval & PHY_LINK_INT_UP_OCCURRED) {
			return LINK_UP;
		} else if (regval & PHY_LINK_INT_DOWN_OCCURED) {
			return LINK_DOWN;
		} else {
			return LINK_UNCHANGED;
		}
	}
	return LINK_ERROR;
}
