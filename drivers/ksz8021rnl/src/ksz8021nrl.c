#include <stdint.h>
#include "stm32f4xx_hal_eth.h"
#include "ksz8021nrl.h"

static ETH_HandleTypeDef h_eth;

static void ksz8021_gpio_init(ETH_HandleTypeDef *heth);

int ksz8021_init(uint8_t *mac_addr)
{
    HAL_StatusTypeDef status = HAL_ERROR;

    if (mac_addr == NULL)
    {
        return -1;
    }

    status = HAL_ETH_RegisterCallback(
                &h_eth,
                HAL_ETH_MSPINIT_CB_ID,
                ksz8021_gpio_init);

    if (status != HAL_OK)
    {
        return -1;
    }

    h_eth.Instance = ETH;
    h_eth.Init.MACAddr = mac_addr;
    h_eth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
    h_eth.Init.Speed = ETH_SPEED_100M;
    h_eth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
    h_eth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
    h_eth.Init.RxMode = ETH_RXPOLLING_MODE;
    h_eth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
    h_eth.Init.PhyAddress = PHY_ADDRESS;

    /* configure ethernet peripheral (GPIOs, clocks, MAC, DMA) */
    status = HAL_ETH_Init(&h_eth);
    if (status != HAL_OK)
    {
        return -1;
    }

    ksz8021_gpio_init();

    return 0;
}

/**
 * @brief PHY GPIO init
 *
 */
static void ksz8021_gpio_init(ETH_HandleTypeDef *heth)
{
    GPIO_InitTypeDef gpio;
    (void)heth;

    /* Enable GPIOs clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    memset(&gpio, 0, sizeof(GPIO_InitTypeDef));

    gpio.Speed = GPIO_SPEED_HIGH;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Alternate = GPIO_AF11_ETH;

    gpio.Pin = RMII_CSR_DV_PIN;
    HAL_GPIO_Init(RMII_CSR_DV_PORT, &gpio);

    gpio.Pin = RMII_RXD0_PIN;
    HAL_GPIO_Init(RMII_RXD0_PORT, &gpio);

    gpio.Pin = RMII_RXD1_PIN;
    HAL_GPIO_Init(RMII_RXD1_PORT, &gpio);

    gpio.Pin = RMII_TXD0_PIN;
    HAL_GPIO_Init(RMII_TXD0_PORT, &gpio);

    gpio.Pin = RMII_TXD1_PIN;
    HAL_GPIO_Init(RMII_TXD1_PORT, &gpio);

    gpio.Pin = RMII_TXEN_PIN;
    HAL_GPIO_Init(RMII_TXEN_PORT, &gpio);

    gpio.Pin = RMII_MDIO_PIN;
    HAL_GPIO_Init(RMII_MDIO_PORT, &gpio);

    gpio.Pin = RMII_MDC_PIN;
    HAL_GPIO_Init(RMII_MDC_PORT, &gpio);

    gpio.Pin = RMII_REF_CLK_PIN;
    HAL_GPIO_Init(RMII_REF_CLK_PORT, &gpio);

    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = RMII_PHY_INT_PIN;
    HAL_GPIO_Init(RMII_PHY_INT_PORT, &gpio);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin = RMII_PHY_RST_PIN;
    HAL_GPIO_Init(RMII_PHY_RST_PORT, &gpio);

    /* Enable ETHERNET clock  */
    __HAL_RCC_ETH_CLK_ENABLE();

    /* Enable and set EXTI Line0 Interrupt to the lowest priority */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, PHY_INT_PRIO, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}
