/*
 * app.c -- application start-up and main loop. See app.h for where
 * main.c calls these.
 */
#include "app.h"
#include "tasks.h"
#include "uart.h"
#include "hrtim.h"
#include "pfm.h"
#include "gate_driver.h"
#include "qspi_test.h"
#include "pfm_input.h"

void App_Init(void)
{
    /* Brings the PFM table module to a known-empty, known-stopped state
       before anything else can touch it (a TABLE:* command over UART, or
       a FIRE). Does not start HRTIM outputs -- see PFM_Init()'s own
       comment in pfm.c. */
    PFM_Init();

    /* One explicit GateDriver_CheckFault() call, here at boot, before
       relying on the EXTI interrupt (gate_driver.c, configured by
       BoardIo_Init()) for everything from here on. EXTI is
       edge-triggered: a pin that is ALREADY in its fault state (per
       GDS_FAULT_POLARITY, ctrlr_config.h) at the moment PE0..PE11 get
       configured for interrupt mode produces no edge of its own -- the
       ISR would simply never fire for it, silently, until something
       eventually toggles that pin. Confirmed relevant on this exact
       board: a GDS? snapshot taken earlier the same day this was added
       showed GateDriverStatus_03 (PE2) already HIGH, which is a fault
       under this build's NORMALLY_LOW polarity. This call catches
       exactly that case -- any fault already present at boot -- instead
       of depending on a future transition that might never come. */
    GateDriver_CheckFault();

    /* QUADSPI bring-up (PE12-PE15/PB10-PB11, W25Q128JVS) -- see
       qspi_test.h for scope. No-op when QSPI_TEST_FEATURE_ENABLED is 0,
       matching the same always-call/resolves-to-something-or-nothing
       pattern already used for BootJump_CheckAndEnter(). */
    QspiTest_Init();

    /* PFM_Input period/duty capture (PA15/PD4/PB2/PC12/PB4/PD12, TIM2/
       TIM3/TIM4/TIM5) -- see pfm_input.h. Configures the timers/GPIO
       only; does not arm or start any capture (that's PfmInput_Arm()
       via PFMIN:CAPTURE, and PfmInput_OnShotStart(), called from
       PFM_Restart() in pfm.c). No-op when PFM_INPUT_FEATURE_ENABLED is
       0. */
    PfmInput_Init();

    uart_start(&uart2);

    /* The HRTIM master-repetition interrupt must be enabled now, at
       boot, even though outputs are not yet running: PFM_CycleBoundaryHandler()
       needs to be wired up and ready before the first FIRE, not armed
       reactively at fire time. The ISR itself is a no-op with respect to
       actual switching until HRTIM1_PWM_Start() has been called (by
       cmd_fire() -> PFM_Restart()). Ported from the sibling
       PFM-STM32G474 project's main.c, same placement/rationale. */
    HRTIM1_EnableMasterInterrupt();
}

void App_Poll(void)
{
    /* Polls for a completed serial command line and dispatches it. */
    TaskScpi_Poll();
}
