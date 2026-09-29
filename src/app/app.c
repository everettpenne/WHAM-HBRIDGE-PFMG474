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
#include "flash_bank.h"
#include "boot_diag.h"
#include "mcu.h"
#include "ctrlr_config.h"
#include "git_version.h"
#include <stdio.h>

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

/* Unsolicited boot signal, sent once at the end of start-up: a host (for
   example python/fw_update.py after a bank swap) treats a !BOOT line as
   proof the new image is running. Lines start with "!BOOT" so they can never
   be mistaken for an OK/ERR reply.
     !BOOT <name> <version> <commit>[-dirty] BANK=<1|2> BFB2=<0|1> STATE=<s> tick=<ms>
     !BOOT diag ...     previous boot's record and this boot's reset cause
                        (boot_diag.h)
     !BOOT <greeting>
   STATE is IDLE at every boot (no fire can be in progress yet); it becomes
   the operating-state machine's state when that exists. */
void App_SendBootBanner(void)
{
    char banner[200];
    uint8_t bank = FlashBank_Active();
    uint8_t bfb2 = FlashBank_Bfb2();
    const char *stateName = (PFM_GetState() == PFM_STATE_RUNNING) ? "FIRING" : "IDLE";

    snprintf(banner, sizeof(banner),
             "!BOOT %s %s %s%s BANK=%u BFB2=%u STATE=%s tick=%lu\r\n",
             HW_BOARD_NAME, FW_VERSION_STRING, FW_GIT_COMMIT,
             (FW_GIT_DIRTY != 0U) ? "-dirty" : "",
             (unsigned)bank, (unsigned)bfb2, stateName,
             (unsigned long)Mcu_GetTickMs());
    uart_send(&uart2, banner);

    /* Previous boot's record and this boot's reset cause (boot_diag.c). */
    BootDiag_FormatReport(banner, sizeof(banner));
    uart_send(&uart2, banner);
    uart_send(&uart2, "!BOOT Rise and shine, controller's awake and ready to work \xF0\x9F\x8C\x9E\r\n");
}

void App_Poll(void)
{
    /* Polls for a completed serial command line and dispatches it. */
    TaskScpi_Poll();
}
