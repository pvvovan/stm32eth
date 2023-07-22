#include "stm32f4xx_hal.h"
#include "hw_delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal_rng.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/inet.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"
#include "lwip/etharp.h"
#include "lwip/dhcp.h"
#include <string.h>


static RNG_HandleTypeDef rng_handle;
static ETH_HandleTypeDef s_heth;
static ETH_TxPacketConfig TxConfig;
static struct netif s_netif;
static ip4_addr_t s_ipaddr;
static ip4_addr_t s_netmask;
static ip4_addr_t s_gw;

uint32_t rand_wrapper(void)
{
	uint32_t random = 0;

	(void)HAL_RNG_GenerateRandomNumber(&rng_handle, &random);

	return random;
}

static void Error_Handler(void)
{
	__disable_irq();
	while (1) { }
}

static void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/* Configure the main internal regulator output voltage */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 168;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/* Initializes the CPU, AHB and APB buses clocks */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
					| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

#define RMII_PHY_RST_PORT       GPIOD
#define RMII_PHY_RST_PIN        GPIO_PIN_10
#define RMII_CSR_DV_PORT        GPIOA
#define RMII_CSR_DV_PIN         GPIO_PIN_7
#define KSZ8081_RESET_ASSERT_DELAY_US   (500)
#define KSZ8081_BOOTUP_DELAY_US         (100)

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


/* PHY Registers */
#define PHY_BASIC_CONTROL                   ((uint16_t)0x0000)
#define PHY_BASIC_STATUS                    ((uint16_t)0x0001)
#define PHY_IDENTIFIER1                     ((uint16_t)0x0002)
#define PHY_IDENTIFIER2                     ((uint16_t)0x0003)
#define PHY_AUTONEG_ADVERTISEMENT           ((uint16_t)0x0004)
#define PHY_AUTONEG_LINK_PARTNER_ABILITY    ((uint16_t)0x0005)
#define PHY_AUTONEG_EXPANSION               ((uint16_t)0x0006)
#define PHY_AUTONEG_NEXT_PAGE               ((uint16_t)0x0007)
#define PHY_LINK_PARTNER_NEXT_PAGE_ABILITY  ((uint16_t)0x0008)
#define PHY_DIGITAL_RESERVED_CONTROL        ((uint16_t)0x0010)
#define PHY_AFE_CONTROL1                    ((uint16_t)0x0011)
#define PHY_RX_ERROR_COUNTER                ((uint16_t)0x0015)
#define PHY_OPERATION_MODE_STRAP_OVERRIDE   ((uint16_t)0x0016)
#define PHY_OPERATION_MODE_STRAP_STATUS     ((uint16_t)0x0017)
#define PHY_EXPANDED_CONTROL                ((uint16_t)0x0018)
#define PHY_INTERRUPT_CONTROL               ((uint16_t)0x001B)
#define PHY_INTERRUPT_STATUS                ((uint16_t)0x001B)
#define PHY_LINKMD_CONTROL                  ((uint16_t)0x001D)
#define PHY_LINKMD_STATUS                   ((uint16_t)0x001D)
#define PHY_CONTROL1                        ((uint16_t)0x001E)
#define PHY_CONTROL2                        ((uint16_t)0x001F)

/* PHY Masks */
#define PHY_REF_CLOCK_SELECT_MASK           ((uint16_t)0x0080)
#define PHY_REF_CLOCK_SELECT_25MHZ          ((uint16_t)0x0080)
#define PHY_LINK_INT_UP_OCCURRED            ((uint16_t)0x0001)
#define PHY_LINK_INT_DOWN_OCCURED           ((uint16_t)0x0004)


void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
	ksz8081_bootstrap();

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* Enable Peripheral clock */
	__HAL_RCC_ETH_CLK_ENABLE();

	/* ETH GPIO Configuration
		PC1      ------> ETH_MDC
		PA1      ------> ETH_REF_CLK
		PA2      ------> ETH_MDIO
		PA7      ------> ETH_CRS_DV
		PC4      ------> ETH_RXD0
		PC5      ------> ETH_RXD1
		PB11     ------> ETH_TX_EN
		PB12     ------> ETH_TXD0
		PB13     ------> ETH_TXD1
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
	*       thus REF_CLK pin is not connected and MAC module is
	*       not clocking. So bit ETH_DMABMR_SR in DMABMR register
	*       of MAC subsystem will never cleared */
	phyreg &= (uint16_t)(~(PHY_REF_CLOCK_SELECT_MASK));
	phyreg |= (PHY_REF_CLOCK_SELECT_25MHZ);
	status = HAL_ETH_WritePHYRegister(heth, 0, PHY_CONTROL2, phyreg);
	if (status != HAL_OK) {
		return;
	}
}

static void link_state(void *const arg)
{
	uint32_t regval = 0;
	(void)arg;

	for ( ; ; ) {
		vTaskDelay(10);
		HAL_StatusTypeDef status = HAL_ETH_ReadPHYRegister(&s_heth, 0, PHY_INTERRUPT_STATUS, &regval);
		if (status == HAL_OK) {
			if (regval & PHY_LINK_INT_UP_OCCURRED) {
				netif_set_up(&s_netif);
				netif_set_link_up(&s_netif);
			} else if (regval & PHY_LINK_INT_DOWN_OCCURED) {
				netif_set_down(&s_netif);
				netif_set_link_down(&s_netif);
			} else {
				/* No action */
			}
		}
	}
}

static struct pbuf *low_level_input(struct netif *netif)
{
	(void)netif;
	struct pbuf *p = NULL;

	HAL_ETH_ReadData(&s_heth, (void **)&p);

	return p;
}

static void ethernetif_input(void *const arg)
{
	err_t err;
	(void)arg;
	struct pbuf *p;

	for ( ; ; ) {
		vTaskDelay(5);
		do {
			/* move received packet into a new pbuf */
			p = low_level_input(&s_netif);
			if (p != NULL) {
				/* entry point to the LwIP stack */
				err = s_netif.input(p, &s_netif);
				if (err != ERR_OK) {
					pbuf_free(p);
					p = NULL;
				}
			}
		} while (p != NULL);
	}
}

volatile int g_link;

static void ethernet_link_updated(struct netif *netif)
{
	/* notify the user about the interface status change */
	if (netif_is_up(netif)) {
		g_link = 1;
	} else {
		g_link = 2;
	}
}

#define ETH_DMA_TRANSMIT_TIMEOUT		( 20U )

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	(void)netif;
	uint32_t i = 0U;
	struct pbuf *q = NULL;
	err_t errval = ERR_OK;
	ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT] = { 0 };

	memset(Txbuffer, 0 , ETH_TX_DESC_CNT * sizeof(ETH_BufferTypeDef));

	for(q = p; q != NULL; q = q->next) {
		if(i >= ETH_TX_DESC_CNT) {
			return ERR_IF;
		}

		Txbuffer[i].buffer = q->payload;
		Txbuffer[i].len = q->len;

		if(i>0) {
			Txbuffer[i-1].next = &Txbuffer[i];
		}

		if(q->next == NULL) {
			Txbuffer[i].next = NULL;
		}

		i++;
	}

	TxConfig.Length = p->tot_len;
	TxConfig.TxBuffer = Txbuffer;
	TxConfig.pData = p;

	HAL_ETH_Transmit(&s_heth, &TxConfig, ETH_DMA_TRANSMIT_TIMEOUT);

	return errval;
}

static err_t ethernetif_init(struct netif *netif)
{
	LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

	/*
	* Initialize the snmp variables and counters inside the struct netif.
	* The last argument should be replaced with your link speed, in units
	* of bits per second.
	*/
	// MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

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

	/* initialize the hardware */
	// low_level_init(netif);

	return ERR_OK;
}

static void init_task(void *arg)
{
	(void)arg;
	static uint8_t MACAddr[6] = { 0x00, 0x80, 0xE1, 0x00, 0x00, 0x00 };
	static ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
	static ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

	s_heth.Instance = ETH;
	s_heth.Init.MACAddr = &MACAddr[0];
	s_heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
	s_heth.Init.TxDesc = DMATxDscrTab;
	s_heth.Init.RxDesc = DMARxDscrTab;
	s_heth.Init.RxBuffLen = 1536;
	HAL_ETH_Init(&s_heth);

	__HAL_RCC_GPIOD_CLK_ENABLE();
	GPIO_InitTypeDef gpio = { 0 };
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;

	gpio.Pin = GPIO_PIN_13;
	HAL_GPIO_Init(GPIOD, &gpio);

	gpio.Pin = GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &gpio);
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);

	(void)HAL_RNG_Init(&rng_handle);

	TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
	TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
	TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

	/* Create TCP/IP stack thread */
	/* Initilialize the LwIP stack with RTOS */
	tcpip_init(NULL, NULL);

	/* IP addresses initialization with DHCP (IPv4) */
	s_ipaddr.addr = 0;
	s_netmask.addr = 0;
	s_gw.addr = 0;
	/* add the network interface (IPv4/IPv6) with RTOS */
	/* The application must add the network interface to lwIP list of network interfaces
	(netifs in lwIP parlance) by calling netif_add(), which takes the interface initialization function */
	netif_add(&s_netif, &s_ipaddr, &s_netmask, &s_gw, NULL, &ethernetif_init, &tcpip_input);

	/* Registers the default network interface */
	netif_set_default(&s_netif);

	/* Set the link callback function, this function is called on change of link status */
	/* The application should register a function that will be called when interface is brought up
	or down via netif_set_status_callback(). Application can use this callback to notify the user
	about the interface status change. */
	netif_set_link_callback(&s_netif, ethernet_link_updated);

	/* Bring up the net interface by calling netif_set_up() with default netif pointer */
	if (netif_is_link_up(&s_netif)) {
		/* When the netif is fully configured this function must be called */
		netif_set_up(&s_netif);
	} else {
		/* When the netif link is down this function must be called */
		netif_set_down(&s_netif);
	}

	xTaskCreate(link_state, "link_st", 128, NULL, 3, NULL);
	xTaskCreate(ethernetif_input, "link_st", 128, NULL, 3, NULL);

	/* Application can call dhcp_start() to start the DHCP negotiation */
	/* Start DHCP negotiation for a network interface (IPv4) */
	dhcp_start(&s_netif);

	for ( ; ; ) {
		vTaskDelay(600);
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
		static volatile char pcWriteBuffer[1024];
		vTaskGetRunTimeStats((char *)&pcWriteBuffer[0]);

		if (dhcp_supplied_address(&s_netif)) {
			char str[128] = { 0 };
			sprintf(str, "%s", inet_ntoa(s_netif.ip_addr));
			str[127] = 0;
		}
	}
}

int main()
{
	__asm volatile ("bkpt #0" : : : "memory");

	HAL_Init();

	/* Configure the system clock to 168MHz */
	SystemClock_Config();

	xTaskCreate(init_task, "init", 2048, NULL, 3, NULL);
	vTaskStartScheduler();
}

uint32_t HAL_GetTick(void)
{
	return xTaskGetTickCount();
}

void SysTick_Handler(void)
{
	HAL_IncTick();
}

void tim2_init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	__asm volatile ("dmb" : : : "memory");
	while ((RCC->APB1ENR & RCC_APB1ENR_TIM2EN) == 0) { }
	__asm volatile ("dmb" : : : "memory");

	TIM2->PSC = 1680 - 1; // 100kHz counter
	TIM2->EGR |= TIM_EGR_UG;
	TIM2->ARR = 0xFFFFFFFFuL;
	__asm volatile ("dmb" : : : "memory");

	TIM2->CR1 |= TIM_CR1_CEN;
}

uint32_t tim2_cnt(void)
{
	return TIM2->CNT;
}

void HardFault_Handler(void)
{
	while (1) { }
}

void MemManage_Handler(void)
{
	while (1) { }
}

void BusFault_Handler(void)
{
	while (1) { }
}

void UsageFault_Handler(void)
{
	while (1) { }
}
