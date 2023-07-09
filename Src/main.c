#include "stm32f4xx_hal.h"

int main()
{
	asm volatile ("bkpt #0" : : : "memory");

	HAL_Init();

	while (1) {
		static long val;
		val++;
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
