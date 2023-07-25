#include "error_handler.h"


void HardFault_Handler(void)
{
	Error_Handler();
}

void MemManage_Handler(void)
{
	Error_Handler();
}

void BusFault_Handler(void)
{
	Error_Handler();
}

void UsageFault_Handler(void)
{
	Error_Handler();
}

void Error_Handler(void)
{
	__asm volatile ("cpsid i" : : : "memory");
	for ( ; ; ) { }
}
