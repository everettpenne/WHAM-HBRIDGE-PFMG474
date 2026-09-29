/*
 * mcu.h -- core/CPU services the application needs, behind a HAL-free
 * interface. Implemented in src/bsp/<chip>/mcu.c.
 */
#ifndef MCU_H
#define MCU_H

#include <stdint.h>

/* Free-running CPU cycle counter (170 MHz on this board). Start once;
   Mcu_CycleCount() wraps, so take differences as uint32_t. Start is
   idempotent. */
void     Mcu_CycleCounterStart(void);
uint32_t Mcu_CycleCount(void);

/* Raises SysTick to the highest interrupt priority. Call right after
   HAL_Init() (main.c) -- see mcu.c for why. */
void Mcu_SetSysTickHighestPriority(void);

#endif /* MCU_H */
