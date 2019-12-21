#ifndef KSZ8021NRL_H
#define KSZ8021NRL_H

#include "stm32f4xx_hal.h"

/* ################### Ethernet PHY GPIO configuration ###################### */

#define RMII_CSR_DV_PORT        GPIOA
#define RMII_CSR_DV_PIN         GPIO_PIN_7

#define RMII_PHYADD_PORT        GPIOA
#define RMII_PHYADD_PIN         GPIO_PIN_7

#define RMII_RXD0_PORT          GPIOC
#define RMII_RXD0_PIN           GPIO_PIN_4

#define RMII_RXD1_PORT          GPIOC
#define RMII_RXD1_PIN           GPIO_PIN_5

#define RMII_TXD0_PORT          GPIOB
#define RMII_TXD0_PIN           GPIO_PIN_12

#define RMII_TXD1_PORT          GPIOB
#define RMII_TXD1_PIN           GPIO_PIN_13

#define RMII_TXEN_PORT          GPIOB
#define RMII_TXEN_PIN           GPIO_PIN_11

#define RMII_MDIO_PORT          GPIOA
#define RMII_MDIO_PIN           GPIO_PIN_2

#define RMII_MDC_PORT           GPIOC
#define RMII_MDC_PIN            GPIO_PIN_1

#define RMII_REF_CLK_PORT       GPIOA
#define RMII_REF_CLK_PIN        GPIO_PIN_1

#define RMII_PHY_INT_PORT       GPIOE
#define RMII_PHY_INT_PIN        GPIO_PIN_8

#define RMII_PHY_RST_PORT       GPIOD
#define RMII_PHY_RST_PIN        GPIO_PIN_10

#define PHY_INT_PRIO            (2)

/* ################## Ethernet peripheral configuration ##################### */

/* Section 2: PHY configuration section */

/* KSZ8021NRL PHY Address
 * Note: 0 - default address after startup */
#define PHY_ADDRESS                     0x00U
/* PHY Reset delay these values are based on a 1 ms Systick interrupt*/
#define PHY_RESET_DELAY                 0x000000FFU
/* PHY Configuration delay */
#define PHY_CONFIG_DELAY                0x00000FFFU

#define PHY_READ_TO                     0x0000FFFFU
#define PHY_WRITE_TO                    0x0000FFFFU

/* Section 3: Common PHY Registers */

#define PHY_BCR                         ((uint16_t)0x0000)  /*!< Transceiver Basic Control Register   */
#define PHY_BSR                         ((uint16_t)0x0001)  /*!< Transceiver Basic Status Register    */

#define PHY_RESET                       ((uint16_t)0x8000)  /*!< PHY Reset */
#define PHY_LOOPBACK                    ((uint16_t)0x4000)  /*!< Select loop-back mode */
#define PHY_FULLDUPLEX_100M             ((uint16_t)0x2100)  /*!< Set the full-duplex mode at 100 Mb/s */
#define PHY_HALFDUPLEX_100M             ((uint16_t)0x2000)  /*!< Set the half-duplex mode at 100 Mb/s */
#define PHY_FULLDUPLEX_10M              ((uint16_t)0x0100)  /*!< Set the full-duplex mode at 10 Mb/s  */
#define PHY_HALFDUPLEX_10M              ((uint16_t)0x0000)  /*!< Set the half-duplex mode at 10 Mb/s  */
#define PHY_AUTONEGOTIATION             ((uint16_t)0x1000)  /*!< Enable auto-negotiation function     */
#define PHY_RESTART_AUTONEGOTIATION     ((uint16_t)0x0200)  /*!< Restart auto-negotiation function    */
#define PHY_POWERDOWN                   ((uint16_t)0x0800)  /*!< Select the power down mode           */
#define PHY_ISOLATE                     ((uint16_t)0x0400)  /*!< Isolate PHY from MII                 */

#define PHY_AUTONEGO_COMPLETE           ((uint16_t)0x0020)  /*!< Auto-Negotiation process completed   */
#define PHY_LINKED_STATUS               ((uint16_t)0x0004)  /*!< Valid link established               */
#define PHY_JABBER_DETECTION            ((uint16_t)0x0002)  /*!< Jabber condition detected            */

/* Section 4: Extended PHY Registers */

#define PHY_SR                          ((uint16_t)0x001E)  /*!< PHY status register Offset                      */
#define PHY_MICR                        ((uint16_t)0x001B)  /*!< MII Interrupt Control Register                  */
#define PHY_MISR                        ((uint16_t)0x001B)  /*!< MII Interrupt Status and Misc. Control Register */

#define PHY_LINK_STATUS                 ((uint16_t)0x0001)  /*!< PHY Link mask                                   */
#define PHY_SPEED_STATUS                ((uint16_t)0x0001)  /*!< PHY Speed mask                                  */
#define PHY_DUPLEX_STATUS               ((uint16_t)0x0004)  /*!< PHY Duplex mask                                 */
#define PHY_MISR_LINK_INT_EN            ((uint16_t)0x0500)  /*!< Enable Interrupt on change of link status       */
#define PHY_LINK_INT_UP_MASK            ((uint16_t)0x0001)  /*!< PHY link status interrupt mask                  */
#define PHY_LINK_INT_DOWN_MASK          ((uint16_t)0x0004)  /*!< PHY link status interrupt mask                  */

int ksz8021_init(ETH_HandleTypeDef *heth, uint8_t *mac_addr);

#endif /* KSZ8021NRL_H */

