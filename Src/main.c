#include "stm32f4xx_hal.h"

static void Error_Handler(void)
{
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
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
				|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

int main()
{
	__asm volatile ("bkpt #0" : : : "memory");

	HAL_Init();

	/* Configure the system clock to 168 MHz */
	SystemClock_Config();

	__HAL_RCC_GPIOD_CLK_ENABLE();
	GPIO_InitTypeDef gpio = { 0 };
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;

	gpio.Pin = GPIO_PIN_13;
	HAL_GPIO_Init(GPIOD, &gpio);

	gpio.Pin = GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &gpio);
	HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);

	while (1) {
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
