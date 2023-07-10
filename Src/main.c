#include "stm32f4xx_hal.h"
#include "hw_delay.h"


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
#define PHY_REF_CLOCK_SELECT_MASK           ((uint16_t)0x0080)
#define PHY_REF_CLOCK_SELECT_25MHZ          ((uint16_t)0x0080)

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

int main()
{
	__asm volatile ("bkpt #0" : : : "memory");

	HAL_Init();

	/* Configure the system clock to 168MHz */
	SystemClock_Config();

	static ETH_HandleTypeDef heth = { 0 };
	static uint8_t MACAddr[6] = { 0x00, 0x80, 0xE1, 0x00, 0x00, 0x00 };
	static ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
	static ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

	heth.Instance = ETH;
	heth.Init.MACAddr = &MACAddr[0];
	heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
	heth.Init.TxDesc = DMATxDscrTab;
	heth.Init.RxDesc = DMARxDscrTab;
	heth.Init.RxBuffLen = 1536;
	HAL_ETH_Init(&heth);

	__HAL_RCC_GPIOD_CLK_ENABLE();
	GPIO_InitTypeDef gpio = { 0 };
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;

	gpio.Pin = GPIO_PIN_13;
	HAL_GPIO_Init(GPIOD, &gpio);

	gpio.Pin = GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &gpio);
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);

	for ( ; ; ) {
		HAL_Delay(500);
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
	}
}

void SysTick_Handler(void)
{
	HAL_IncTick();
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
