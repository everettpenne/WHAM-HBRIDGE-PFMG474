/*
 * command_table.c -- the serial command table and the dispatch entry point.
 *
 * Matching (SCPI-style hierarchical mnemonics, short/long form, '?' query
 * suffix) is src/middleware/scpi/scpi_parser.c -- see its header for the
 * pattern syntax. This file is the product's own part: which pattern runs
 * which handler.
 *
 * To add a command
 * -----------------
 *  1. Implement the handler in the matching cmd_<subsystem>.c
 *  2. Declare it in commands.h
 *  3. Add a { "PATTern:MNEMonic?", handler } row to command_table[]
 *     below. Nothing else changes -- the table is flat, so adding a new
 *     leaf or a whole new subsystem is always just one more row.
 */

#include "commands.h"
#include "cmd_common.h"
#include "scpi_parser.h"
#include "boot_jump.h"
#include "qspi_test.h"
#include "pfm_input.h"

static const scpi_command_t command_table[] = {
    /* Common commands (IEEE 488.2) */
    { "*IDN?",              cmd_idn          },

    /* Serial-bootloader entry -- excluded entirely (not just an
       unreachable row) when BOOT_JUMP_FEATURE_ENABLED is 0, so BOOT
       falls through to "ERR 1 Unknown command" like any other
       unrecognized mnemonic. See boot_jump.h. */
#if (BOOT_JUMP_FEATURE_ENABLED != 0)
    { "BOOT",                cmd_boot         },
#endif

    /* PFM table upload -- see commands.c's own header comment on
       these four handlers. */
    { "TABle:BEGin",         cmd_table_begin  },
    { "TABle:STEP",          cmd_table_step   },
    { "TABle:END",           cmd_table_end    },
    { "TABle?",              cmd_table_query  },

    /* Begins PWM output -- see commands.c's own header comment on
       cmd_fire(). */
    { "FIRE",                cmd_fire         },

    /* PFM table-advance ISR timing diagnostic -- see commands.c's own
       header comment on cmd_pfm_diag(). */
    { "PFM:DIAG?",           cmd_pfm_diag     },

    /* TEMPORARY, see commands.h -- raw per-call gap log behind
       PFM:DIAG?'s aggregate max-gap number. */
    { "PFM:GAPLOG?",         cmd_pfm_gaplog   },

    /* Reports the compile-time HRTIM channel count -- see commands.c's
       own header comment on cmd_config_channels(). */
    { "CONFig:CHANnels?",    cmd_config_channels },

    /* PC10/HRTIM1_FLT6 hardware fault status/clear -- see commands.c's
       own header comment on cmd_fault_query()/cmd_fault_clear(). */
    { "FAULT?",              cmd_fault_query  },
    { "FAULT:CLEar",         cmd_fault_clear  },

    /* Raw GateDriverStatus_01..12 (PE0..PE11) diagnostic readback --
       see commands.c's own header comment on cmd_gds_query(). */
    { "GDS?",                cmd_gds_query    },

    /* QUADSPI connectivity test (W25Q128JVS) -- excluded entirely when
       QSPI_TEST_FEATURE_ENABLED is 0, same removability pattern as
       BOOT above. See commands.c's own header comment on
       cmd_qspi_id(). */
#if (QSPI_TEST_FEATURE_ENABLED != 0)
    { "QSPI:ID?",            cmd_qspi_id      },
#endif

    /* PFM_Input period/duty capture -- excluded entirely when
       PFM_INPUT_FEATURE_ENABLED is 0, same removability pattern as
       BOOT/QSPI:ID? above. See commands.c's own header comment on
       cmd_pfmin_capture()/cmd_pfmin_status()/cmd_pfmin_data(). */
#if (PFM_INPUT_FEATURE_ENABLED != 0)
    { "PFMIN:CAPTURE",       cmd_pfmin_capture },
    { "PFMIN:STATus?",       cmd_pfmin_status  },
    { "PFMIN:DMASTAT?",      cmd_pfmin_dmastat },  /* TEMPORARY, see commands.h */
    { "PFMIN:DATA?",         cmd_pfmin_data    },
#endif
};

#define NUM_COMMANDS  (sizeof(command_table) / sizeof(command_table[0]))

void Commands_Dispatch(uart_instance_t *inst, char *line)
{
    if (scpi_dispatch(command_table, NUM_COMMANDS, inst, line) == SCPI_UNKNOWN)
    {
        SendErr(inst, ERR_UNKNOWN_COMMAND, "Unknown command");
    }
}
