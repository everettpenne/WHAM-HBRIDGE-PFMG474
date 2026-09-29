/*
 * mcu.c -- STM32G4 implementation of drivers/mcu.h.
 */
#include "mcu.h"
#include "main.h"

/* TRCENA (DEMCR) must be set before DWT->CTRL's CYCCNTENA bit will stick,
   per the Cortex-M4 debug architecture. Idempotent. */
void Mcu_CycleCounterStart(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t Mcu_CycleCount(void)
{
    return DWT->CYCCNT;
}

/**
  * @brief  Raises SysTick's NVIC priority off HAL's default lowest
  *         value, before anything else can run at an intermediate
  *         priority.
  *
  * Ported verbatim from the sibling PFM-STM32G474 project's main.c.
  * HAL_InitTick() (called from HAL_Init(), which must run before this)
  * leaves SysTick_IRQn at TICK_INT_PRIORITY (15, the lowest possible
  * priority on this Cortex-M4's 4-bit-preempt NVIC grouping). This
  * project's interrupt priority scheme needs SysTick to be the
  * *highest*-priority interrupt instead, at 0 -- ahead of both
  * HRTIM1_Master_IRQn (1, see HRTIM1_EnableMasterInterrupt() in
  * hrtim.c) and USART2_IRQn (2, see main.c's MX_USART2_UART_Init()) --
  * so that HAL_Delay()/HAL_GetTick() (both driven by SysTick, and used
  * by ordinary HAL driver calls such as HAL_UART_Init() during
  * startup) can never be starved by either of those interrupts firing
  * back-to-back. Left at the HAL default, a sufficiently busy
  * HRTIM1_Master_IRQn or USART2_IRQn could indefinitely delay a
  * HAL_Delay()-based timeout inside some future HAL call, which would
  * look like an unexplained hang rather than a priority bug.
  */
void Mcu_SetSysTickHighestPriority(void)
{
    HAL_NVIC_SetPriority(SysTick_IRQn, 0U, 0U);
}
