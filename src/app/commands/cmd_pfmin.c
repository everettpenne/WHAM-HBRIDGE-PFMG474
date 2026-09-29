/*
 * cmd_pfmin.c -- serial command handlers: PFMIN:*.
 * Split out of the old single commands.c (2026-09-29); the handlers are
 * unchanged. See commands.h for the file map.
 */
#include "commands.h"
#include "cmd_common.h"
#include "ctrlr_config.h"
#include "pfm_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if (PFM_INPUT_FEATURE_ENABLED != 0)
/* --------------------------------------------------------------------------
 * PFMIN:CAPTURE, PFMIN:STATus?, PFMIN:DATA?
 *
 * See pfm_input.h and commands.h's own header comment above these
 * declarations for the shot-synchronized design -- PFMIN:CAPTURE only
 * arms (PfmInput_Arm()); the actual capture starts inside
 * PFM_Restart() (pfm.c), at the next FIRE, not here.
 * -------------------------------------------------------------------------- */
void cmd_pfmin_capture(uart_instance_t *inst, char *args)
{
    char *tok;
    long  m;

    tok = (args != NULL) ? strtok(args, " \r\n") : NULL;
    if (tok == NULL)
    {
        SendErr(inst, ERR_PFMIN_BAD_COUNT, "PFMIN:CAPTURE needs one argument: M (1-PFM_INPUT_MAX_PERIODS)");
        return;
    }

    m = atol(tok);
    if ((m < 1L) || (m > (long)PFM_INPUT_MAX_PERIODS))
    {
        SendErr(inst, ERR_PFMIN_BAD_COUNT, "M out of range (1-PFM_INPUT_MAX_PERIODS)");
        return;
    }

    PfmInput_Arm((uint16_t)m);

    uart_send(inst, "OK\r\n");
}

void cmd_pfmin_status(uart_instance_t *inst, char *args)
{
    char buf[64];
    int  len = 0;
    (void)args;

    len += snprintf(&buf[len], sizeof(buf) - (size_t)len, "OK");
    for (uint8_t ch = 0U; ch < PFM_INPUT_NUM_CHANNELS; ch++)
    {
        len += snprintf(&buf[len], sizeof(buf) - (size_t)len,
                         " %u", (unsigned int)PfmInput_GetCount(ch));
    }
    snprintf(&buf[len], sizeof(buf) - (size_t)len, "\r\n");

    uart_send(inst, buf);
}

/* TEMPORARY debug command, 2026-09-09 -- see pfm_input.h's own comment
   on PfmInput_GetDmaStartStatus(). Remove once DMA capture is
   confirmed reliable. */
void cmd_pfmin_dmastat(uart_instance_t *inst, char *args)
{
    char buf[64];
    int  len = 0;
    (void)args;

    len += snprintf(&buf[len], sizeof(buf) - (size_t)len, "OK");
    for (uint8_t ch = 0U; ch < PFM_INPUT_NUM_CHANNELS; ch++)
    {
        len += snprintf(&buf[len], sizeof(buf) - (size_t)len,
                         " %u", (unsigned int)PfmInput_GetDmaStartStatus(ch));
    }
    snprintf(&buf[len], sizeof(buf) - (size_t)len, "\r\n");

    uart_send(inst, buf);
}

/* Sized for the worst case: PFM_INPUT_MAX_PERIODS entries, each up to
   " 4294967295" (11 chars), plus the "OK <count> OVERCAP=<n>" prefix
   and CRLF -- comfortably over 200*11+32 with room to spare. static,
   not a stack local: this project's linker script reserves only
   _Min_Stack_Size (1 KB) for the stack (STM32G474QETX_FLASH.ld) --
   putting several KB on the stack here would be a real, needless risk
   even though the actual runtime stack has far more room in practice
   (see AGENTS.md's own flagged-but-deferred note on stack sizing). */
static char g_pfminDataBuf[2600];

void cmd_pfmin_data(uart_instance_t *inst, char *args)
{
    char    *tok;
    long     chArg;
    uint8_t  ch;
    uint16_t count;
    const uint32_t *periods;
    int      len = 0;

    tok = (args != NULL) ? strtok(args, " \r\n") : NULL;
    if (tok == NULL)
    {
        SendErr(inst, ERR_PFMIN_BAD_CHANNEL, "PFMIN:DATA? needs one argument: channel (1-6)");
        return;
    }

    chArg = atol(tok);
    if ((chArg < 1L) || (chArg > (long)PFM_INPUT_NUM_CHANNELS))
    {
        SendErr(inst, ERR_PFMIN_BAD_CHANNEL, "Invalid PFM_Input channel (1-6)");
        return;
    }
    ch = (uint8_t)(chArg - 1L);   /* 1-6 on the wire -> 0-5 internally */

    count   = PfmInput_GetCount(ch);
    periods = PfmInput_GetPeriods(ch);

    /* OVERCAP=<n>: a genuine data-quality indicator (see
       PfmInput_GetOvercaptureCount()'s own doc comment in
       pfm_input.c) -- nonzero means a second rising edge arrived
       before this module's ISR could read the previous one, so some
       entries below may not be trustworthy. Expected to read 0 in
       normal operation; not yet documented in
       docs/command_reference.md. */
    len += snprintf(&g_pfminDataBuf[len], sizeof(g_pfminDataBuf) - (size_t)len,
                     "OK %u OVERCAP=%u", (unsigned int)count,
                     (unsigned int)PfmInput_GetOvercaptureCount(ch));

    for (uint16_t i = 0U; i < count; i++)
    {
        len += snprintf(&g_pfminDataBuf[len], sizeof(g_pfminDataBuf) - (size_t)len,
                         " %lu", (unsigned long)periods[i]);
    }
    snprintf(&g_pfminDataBuf[len], sizeof(g_pfminDataBuf) - (size_t)len, "\r\n");

    uart_send(inst, g_pfminDataBuf);
}
#endif /* PFM_INPUT_FEATURE_ENABLED */
