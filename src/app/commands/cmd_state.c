/*
 * cmd_state.c -- serial command handlers: FAULT?/FAULT:CLEar.
 * Split out of the old single commands.c (2026-09-29); the handlers are
 * unchanged. See commands.h for the file map.
 */
#include "commands.h"
#include "cmd_common.h"
#include "ctrlr_config.h"
#include "hrtim.h"
#include "gate_driver.h"
#include <stdio.h>

/* --------------------------------------------------------------------------
 * FAULT? / FAULT:CLEar
 *
 * Status/clear for BOTH of this project's fault sources, combined into
 * one operator-facing fault state (AnyFaultLatched(), above):
 *   - PC10/HRTIM1_FLT6 -- native HRTIM hardware fault input (hrtim.h).
 *     Autonomous in silicon; already fully in effect by the time either
 *     of these is ever called.
 *   - GateDriverStatus_01..12 (PE0..PE11) -- software/EXTI-driven fault
 *     interrupt (gate_driver.h), added 2026-09-08. Needs the EXTI ISR
 *     to actually run (GateDriver_CheckFault()), unlike the PC10 path.
 * These two mechanisms stay structurally independent underneath (two
 * separate latches, two separate detection paths) -- combined only
 * here, at the command layer, because from an operator's perspective
 * "is there a fault, and can I FIRE" is one question with one answer,
 * not two. FAULT:CLEAR clears both latches unconditionally (clearing
 * one that was never set is a harmless no-op); FAULT? reports 1 if
 * either is latched.
 * -------------------------------------------------------------------------- */
void cmd_fault_query(uart_instance_t *inst, char *args)
{
    (void)args;

    uart_send(inst, (AnyFaultLatched() != 0U) ? "OK 1\r\n" : "OK 0\r\n");
}

void cmd_fault_clear(uart_instance_t *inst, char *args)
{
    (void)args;

    HRTIM1_FaultClear();
    GateDriver_FaultClear();

    uart_send(inst, "OK\r\n");
}
