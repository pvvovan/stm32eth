#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_rng.h"

#include "error_handler.h"
#include "ethif.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/inet.h"


static RNG_HandleTypeDef rng_handle;

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

static void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/* Configure the main internal regulator output voltage */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/* Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure. */
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

volatile int g_link;
volatile char *g_ip;
volatile char *g_cpu;

static void ethernet_link_updated(struct netif *netif)
{
	/* notify the user about the interface status change */
	if (netif_is_up(netif)) {
		g_link = 1;
	} else {
		g_link = 2;
	}
}

static void link_state(void *const arg)
{
	(void)arg;
	for ( ; ; ) {
		vTaskDelay(10);
		enum link_status link = ethphy_getlink();
		if (link == LINK_UP) {
			netif_set_up(&s_netif);
			netif_set_link_up(&s_netif);
		} else if (link == LINK_DOWN) {
			netif_set_down(&s_netif);
			netif_set_link_down(&s_netif);
		} else {
			/* No action */
		}
	}
}

static void ethernetif_input(void *const arg)
{
	(void)arg;
	struct pbuf *p = NULL;

	for ( ; ; ) {
		vTaskDelay(2);
		do {
			/* move received packet into a new pbuf */
			p = low_level_input(&s_netif);
			if (p != NULL) {
				/* entry point to the LwIP stack */
				err_t err = s_netif.input(p, &s_netif);
				if (err != ERR_OK) {
					pbuf_free(p);
					p = NULL;
				}
			}
		} while (p != NULL);
	}
}

static void led_init(void)
{
	__HAL_RCC_GPIOD_CLK_ENABLE();
	GPIO_InitTypeDef gpio = { 0 };
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;

	gpio.Pin = GPIO_PIN_13;
	HAL_GPIO_Init(GPIOD, &gpio);

	gpio.Pin = GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &gpio);
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
}

static void led_toggle(void)
{
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);
}

static void init_task(void *arg)
{
	(void)arg;

	ethmac_init();
	led_init();
	(void)HAL_RNG_Init(&rng_handle);

	/* Create TCP/IP stack thread */
	/* Initilialize the LwIP stack with RTOS */
	tcpip_init(NULL, NULL);

	/* IP addresses initialization with DHCP (IPv4) */
	ip_addr_set_zero_ip4(&s_ipaddr);
	ip_addr_set_zero_ip4(&s_netmask);
	ip_addr_set_zero_ip4(&s_gw);
	/* add the network interface (IPv4/IPv6) with RTOS */
	/* The application must add the network interface to lwIP list of network interfaces (netifs
	 in lwIP parlance) by calling netif_add(), which takes the interface initialization function */
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
	xTaskCreate(ethernetif_input, "ethif_in", 128, NULL, 3, NULL);

	/* Application can call dhcp_start() to start the DHCP negotiation */
	/* Start DHCP negotiation for a network interface (IPv4) */
	dhcp_start(&s_netif);

	for ( ; ; ) {
		vTaskDelay(500);
		led_toggle();

		static char pcWriteBuffer[1024];
		vTaskGetRunTimeStats((char *)&pcWriteBuffer[0]);
		g_cpu = &pcWriteBuffer[0];
		g_cpu[1023] = 0;

		if (dhcp_supplied_address(&s_netif)) {
			static char str[128] = { 0 };
			snprintf(str, sizeof(str), "%s", inet_ntoa(s_netif.ip_addr));
			g_ip = &str[0];
			g_ip[127] = 0;
		}
	}
}

int main()
{
	/* Software break point */
	// __asm volatile ("bkpt #0" : : : "memory");

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
