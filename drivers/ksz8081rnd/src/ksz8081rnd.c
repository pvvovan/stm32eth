#include <stdint.h>
#include <string.h>
#include "ksz8081rnd.h"

static void ksz8081_msp_init(ETH_HandleTypeDef *heth);
static int ksz8081_gpio_init();
static int ksz8081_bootstrap();
static int ksz8081_preinit(ETH_HandleTypeDef *heth);

static void ksz8081_msp_init(ETH_HandleTypeDef *heth)
{
    (void)heth;
    /* Place additional stuff here.
     * Function is called on early stage of ETH initialization */
}

/**
 * @brief PHY GPIO init
 *
 */
static int ksz8081_gpio_init()
{
    GPIO_InitTypeDef gpio;

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

    /* Enable and set EXTI Line 8 Interrupt */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, PHY_INT_PRIO, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    return 0;
}

static int ksz8081_bootstrap()
{
    GPIO_InitTypeDef gpio;

    /* Enable GPIOs clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    memset(&gpio, 0, sizeof(GPIO_InitTypeDef));

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

    /* Bootup PHY */
    HAL_GPIO_WritePin(RMII_PHY_RST_PORT, RMII_PHY_RST_PIN, GPIO_PIN_SET);

    HAL_GPIO_DeInit(RMII_CSR_DV_PORT, RMII_CSR_DV_PIN);

    return 0;
}

static int ksz8081_preinit(ETH_HandleTypeDef *heth)
{
    uint32_t tmpreg1 = 0U;
    uint32_t hclk = 60000000U;
    uint32_t phyreg = 0xFF;
    HAL_StatusTypeDef status;

    /* Enable SYSCFG Clock */
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    /* Select MII or RMII Mode*/
    SYSCFG->PMC &= ~(SYSCFG_PMC_MII_RMII_SEL);
    SYSCFG->PMC |= (uint32_t)heth->Init.MediaInterface;
    /*-------------------------------- MAC Initialization ----------------------*/
    /* Get the ETHERNET MACMIIAR value */
    tmpreg1 = (heth->Instance)->MACMIIAR;
    /* Clear CSR Clock Range CR[2:0] bits */
    tmpreg1 &= ETH_MACMIIAR_CR_MASK;

    /* Get hclk frequency value */
    hclk = HAL_RCC_GetHCLKFreq();

    /* Set CR bits depending on hclk value */
    if((hclk >= 20000000U)&&(hclk < 35000000U))
    {
        /* CSR Clock Range between 20-35 MHz */
        tmpreg1 |= (uint32_t)ETH_MACMIIAR_CR_Div16;
    }
    else if((hclk >= 35000000U)&&(hclk < 60000000U))
    {
        /* CSR Clock Range between 35-60 MHz */
        tmpreg1 |= (uint32_t)ETH_MACMIIAR_CR_Div26;
    }
    else if((hclk >= 60000000U)&&(hclk < 100000000U))
    {
        /* CSR Clock Range between 60-100 MHz */
        tmpreg1 |= (uint32_t)ETH_MACMIIAR_CR_Div42;
    }
    else if((hclk >= 100000000U)&&(hclk < 150000000U))
    {
        /* CSR Clock Range between 100-150 MHz */
        tmpreg1 |= (uint32_t)ETH_MACMIIAR_CR_Div62;
    }
    else /* ((hclk >= 150000000)&&(hclk <= 183000000)) */
    {
        /* CSR Clock Range between 150-183 MHz */
        tmpreg1 |= (uint32_t)ETH_MACMIIAR_CR_Div102;
    }

    /* Write to ETHERNET MAC MIIAR: Configure the ETHERNET CSR Clock Range */
    (heth->Instance)->MACMIIAR = (uint32_t)tmpreg1;
    /* Set the ETH peripheral state to READY */
    heth->State = HAL_ETH_STATE_READY;

    status = HAL_ETH_ReadPHYRegister(heth, PHY_CONTROL2, &phyreg);
    if (status != HAL_OK)
    {
        return -1;
    }

    phyreg &= ~(PHY_REF_CLOCK_SELECT_MASK);
    phyreg |= PHY_REF_CLOCK_SELECT_25MHZ;
    status = HAL_ETH_WritePHYRegister(heth, PHY_CONTROL2, phyreg);
    if (status != HAL_OK)
    {
        return -1;
    }

    return 0;
}

static PHY_STATUS_E ksz8081_hal_error_map(HAL_StatusTypeDef err)
{
    PHY_STATUS_E retval = E_PHY_STATUS_ERROR;

    switch (err)
    {
        case HAL_OK:
        {
            retval = E_PHY_STATUS_OK;
            break;
        }

        case HAL_ERROR:
        {
            retval = E_PHY_STATUS_ERROR;
            break;
        }

        case HAL_BUSY:
        {
            retval = E_PHY_STATUS_BUSY;
            break;
        }

        case HAL_TIMEOUT:
        {
            retval = E_PHY_STATUS_TIMEOUT;
            break;
        }

        default:
        {
            retval = E_PHY_STATUS_ERROR;
            break;
        }
    }

    return retval;
}

PHY_STATUS_E ksz8081_init(ETH_HandleTypeDef *heth, uint8_t *mac_addr)
{
    int ksz_status = 0;
    HAL_StatusTypeDef hal_status = HAL_ERROR;

    if (heth == NULL || mac_addr == NULL)
    {
        return E_PHY_STATUS_ERROR;
    }

    hal_status = HAL_ETH_RegisterCallback(
                heth,
                HAL_ETH_MSPINIT_CB_ID,
                ksz8081_msp_init);

    if (hal_status != HAL_OK)
    {
        return E_PHY_STATUS_ERROR;
    }

    ksz_status = ksz8081_bootstrap();
    if (ksz_status < 0)
    {
        return E_PHY_STATUS_ERROR;
    }

    ksz_status = ksz8081_gpio_init(heth);
    if (ksz_status < 0)
    {
        return E_PHY_STATUS_ERROR;
    }
    /* Enable ETHERNET clock  */
    __HAL_RCC_ETH_CLK_ENABLE();

    heth->Instance = ETH;
    heth->Init.MACAddr = mac_addr;
    heth->Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
    heth->Init.Speed = ETH_SPEED_100M;
    heth->Init.DuplexMode = ETH_MODE_FULLDUPLEX;
    heth->Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
    heth->Init.RxMode = ETH_RXPOLLING_MODE;
    heth->Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
    heth->Init.PhyAddress = PHY_ADDRESS;

    /* DEBUG */
    ksz_status = ksz8081_preinit(heth);
    if (ksz_status < 0)
    {
        return E_PHY_STATUS_ERROR;
    }

    /* configure ethernet peripheral */
    hal_status = HAL_ETH_Init(heth);

    return ksz8081_hal_error_map(hal_status);
}
