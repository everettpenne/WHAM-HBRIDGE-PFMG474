# WHAM-HBRIDGE-PFMG474 — Serial Command Reference

USART2, **115200 8N1** (raised from 9600 on 2026-09-04 — see `docs/changelog.txt`; WHAM-HBRIDGE-PFMG474-only, the sibling PFM-STM32G474 project still uses 9600). Commands are terminated by `\r`, `\n`, or `\r\n`.

## Response conventions

Ported from the sibling PFM-STM32G474 project, per project decision:

| Form | Meaning |
|---|---|
| `OK\r\n` | Accepted, no data |
| `OK <value>\r\n` | Accepted, with a return value |
| `ERR <n> <msg>\r\n` | Rejected; error codes are stable across firmware versions |

### Error codes

| Code | Meaning |
|---|---|
| 1 | Unknown command |
| 2 | Not currently uploading a table — send `TABLE:BEGIN` first |
| 3 | Table full (`PFM_TABLE_SIZE` entries already appended) |
| 4 | Invalid `TABLE:STEP` arguments (wrong count — must be `1 + HRTIM_NUM_CHANNELS`, see `CONFig:CHANnels?` — or a value outside uint16 range 0-65535) |
| 5 | Table is empty — `FIRE` has nothing to play back |
| 6 | Fault latched — either PC10/HRTIM1_FLT6 (native HRTIM hardware fault input) or the GateDriverStatus_01..12 EXTI interrupt (`PE0`-`PE11`) — `FAULT:CLEAR` required before `FIRE` will work again |
| 7 | QUADSPI command failed or timed out (see `QSPI:ID?`) |
| 8 | Invalid `PFM_Input` channel (1-6) |
| 9 | `M` out of range for `PFMIN:CAPTURE` (1-`PFM_INPUT_MAX_PERIODS`) |
| 10 | `TABLE:STEP` `per` value implies a carrier frequency above `PFM_MAX_CARRIER_FREQ_HZ` |
| 12 | Invalid command arguments — see the specific command's own usage |
| 13 | Not allowed in the current state (`FWUPdate:*` while the output is running, no transfer in progress, nothing verified) |
| 17 | `FWUPdate:*` flash/CRC/image/option-byte failure |

Codes are never renumbered or reused once assigned, matching the
sibling project's convention. 11, 14, 15 and 16 are used only by
WHAM-XREX-PFMG474 (channels, nicknames, external enable) and are reserved
here so the shared numbers mean the same thing on both projects.

### Unsolicited `!BOOT` banner (every boot)

Added 2026-09-29 (ported from WHAM-XREX-PFMG474). Sent once per boot,
after all initialization and right before the main loop — the exact
signal that a reset (including `FWUPdate:SWAP`/`ROLLback`) has completed
and the command link is live. `!BOOT` never collides with a reply
(`OK`/`ERR`). Three lines:

```
!BOOT WHAM-HBRIDGE-PFMG474 v0.7 2c272540 BANK=1 BFB2=0 STATE=IDLE tick=0
!BOOT diag boot=1 prev: stage=0 fault=0@0 cfsr=00000000 hfsr=00000000 err=0@0 nmi=0@0 eccr=00000000 rst=
!BOOT Rise and shine, controller's awake and ready to work 🌞
```

- Line 1: board, firmware version, git commit (`-dirty` if built from an
  uncommitted tree), the running flash bank, the `BFB2` option bit, the
  state (`IDLE` at every boot; `FIRING` if a table is playing), and
  `HAL_GetTick()` at that point.
- Line 2 (`src/drivers/boot_diag.h`, `.noinit` RAM — survives every reset
  but not a power cycle): `boot` counts boots that reached `main()`
  since power-up. Everything after `prev:` describes the PREVIOUS boot:
  the last stage it reached (`1` main, `2` HAL init, `3` clock, `4`
  GPIO, `5` HRTIM, `6` USART2, `7` app init, `8` main loop, `9` about to
  reload option bytes for a swap), and whether it ended in
  `HardFault_Handler` (`fault=count@stage`, with `CFSR`/`HFSR`),
  `Error_Handler` (`err=`) or `NMI_Handler` (`nmi=`, with `FLASH->ECCR`
  — a flash ECC double error raises an NMI). `rst=` lists the reset
  sources since the previous boot (`OBL` option-byte reload, `PIN`
  NRST, `BOR` brown-out/power-on, `SFT` software, `IWDG`/`WWDG`,
  `LPWR`); a normal `FWUPdate:SWAP` reports `OBL,PIN,`.
- Line 3: a friendly greeting. The emoji is sent as raw UTF-8 bytes.

The first line can arrive with a garbage byte in front of it: the reset
itself glitches the TX line. `python/fw_update.py` allows for this.

## Mnemonic syntax (SCPI-style)

Commands are matched by `scpi_parser.c`'s `scpi_match()` against a flat
table of patterns — see that file's header comment for the full
specification. Summary:

- **Hierarchical**: pattern levels are separated by `:`, e.g.
  `SOURce:VOLTage:LIMit`. A leading `:` on the input is tolerated but
  never required (there's no "current path" concept — every command is
  matched from the root).
- **Short/long form, per level**: in a pattern token, the leading
  UPPERCASE run is the mandatory short form; any lowercase letters
  after it are an optional long-form suffix that must be matched *in
  full* if used at all — no partial-long-form matches. Example:
  `VERSion` accepts `VERS`, `VERSION`, `version` — not `VERSI`.
- **Case-insensitive** throughout.
- **Query suffix `?` must match exactly**: a pattern ending in `?` only
  matches input also ending in `?`, and vice versa.
- **Common (IEEE 488.2) commands** like `*IDN?` are just zero-colon
  patterns — no special-casing needed.

Compound commands (`;`-separated, multiple mnemonics on one line) are
**not** supported yet.

## Commands

### `*IDN?`

Board and firmware identification.

```
> *IDN?
< OK WHAM-HBRIDGE-PFMG474 REVA v0.7 2c272540
  (or, built from an uncommitted working tree:)
< OK WHAM-HBRIDGE-PFMG474 REVA v0.7 2c272540-dirty
```

Reports, space-separated: `HW_BOARD_NAME`, `HW_BOARD_REV`,
`FW_VERSION_STRING` (compile-time constants, `src/config/ctrlr_config.h`),
then `FW_GIT_COMMIT` (added 2026-09-29, ported from WHAM-XREX-PFMG474) —
the short git commit hash this exact build was made from, with a `-dirty`
suffix if the working tree had uncommitted changes, or `unknown` if
`build/generated/git_version.h` was never generated. This answers "which
commit is actually running on this board" independently of what you think
you last flashed. Before 2026-09-29 the reply had only the first three
fields.

`HW_BOARD_REV` is currently a placeholder (`REVA`); update it to match
the actual PCB silkscreen revision.

**Build with `python3 python/wham_build.py`** (not a bare `make`) to get
an accurate `FW_GIT_COMMIT`: it regenerates `build/generated/git_version.h`
first. `git_version.h` is gitignored, so a fresh checkout has none until
`wham_build.py` (or `gen_git_version.py`) runs once.

### `BOOT`

Resets the MCU into the STM32 ROM serial bootloader (System memory), so
a new `.bin` can be written over the same USART2 link with no physical
BOOT0/NRST access. See `docs/serial_reflash_guide.md` for the full
mechanism and usage via `python/wham_serial_flash.py`.

```
> BOOT
< OK ENTERING BOOTLOADER
  (link drops — MCU has reset into the ROM bootloader)
```

Present only when `BOOT_JUMP_FEATURE_ENABLED` (`Core/Inc/boot_jump.h`)
is nonzero (the default). When disabled, `BOOT` is simply unrecognized
(`ERR 1 Unknown command`), like any other unknown mnemonic — see that
header for exactly what disabling the module does.

No state-machine/Firing concept exists in this firmware yet, so `BOOT`
has no rejection conditions today. When application logic that can be
mid-operation is added, gate this command the same way the sibling
project's `cmd_boot()` does (reject with an `ERR` code while active —
resetting under load would drop outputs uncontrolled).

### `TABLE:BEGIN`, `TABLE:STEP`, `TABLE:END`, `TABLE?`

Uploads a complete PFM shot profile (a sequence of `(per, cmp0, ...,
cmp(N-1))` steps, N = `HRTIM_NUM_CHANNELS` — see `Core/Inc/pfm.h`'s
`PFM_Step_t` and `Core/Inc/ctrlr_config.h`), built entirely
off-controller. This firmware has no on-device table *construction*
logic (no frequency/duty math, no sweep builder) — see `pfm.h`'s
"ADDED" header note for why that's a deliberate project decision, not
a gap waiting to be filled in here. `python/pfm_table_upload.py` is
the reference host-side builder — it queries `CONFig:CHANnels?` itself
before uploading, rather than assuming N.

```
> TABLE:BEGIN
< OK
> TABLE:STEP 1699 850 850 850
< OK
> TABLE:STEP 1699 850 850 850
< OK
  ... (repeat for every entry -- example above is N=3) ...
> TABLE:END
< OK 500
```

- **`TABLE:BEGIN`** — clears the table (`PFM_TableReset()`) and opens
  an upload session. Safe to call again mid-upload to start over.
- **`TABLE:STEP <per> <cmp0> ... <cmp(N-1)>`** — appends exactly one
  entry: `per` plus exactly one compare value per channel (N =
  `HRTIM_NUM_CHANNELS`, a compile-time constant — query
  `CONFig:CHANnels?` rather than assuming it). All values are
  `uint16_t` (0-65535); no other validation happens here (see
  `PFM_AppendStep()`'s doc comment — the eventual apply-time
  `HRTIM1_ClampCompare()` in `hrtim.c` is a backstop, not a substitute
  for the host script sending sane values). Rejects with `ERR 2` if no
  `TABLE:BEGIN` is open, `ERR 3` if the table is already at
  `PFM_TABLE_SIZE` (5000) capacity, `ERR 4` for a malformed line
  (including the wrong token count for this board's N).
- **`TABLE:END`** — closes the upload session and reports the final
  entry count (`OK <count>`). Table stays exactly as uploaded even if
  you never send this — it just stops enforcing "must call
  `TABLE:BEGIN` first" for a stray `TABLE:STEP`.
- **`TABLE?`** — reports the current entry count at any time
  (`OK <count>`), upload session open or not.

### `FIRE`

Begins PWM output: (re)starts playback of whatever table is currently
uploaded, from step 0. Playback advances automatically, one table
entry per PWM period, driven by the HRTIM1 master-repetition interrupt
(`HRTIM1_Master_IRQHandler()` in `stm32g4xx_it.c`, calling
`PFM_CycleBoundaryHandler()` in `pfm.c`) — no polling, no further
commands needed once fired. Output stops automatically, at a coherent
period boundary, once the last table entry has completed.

```
> FIRE
< OK
  (HRTIM channels 0..N-1 begin switching, evenly phase-spaced -- see
   hrtim.c's HRTIM1_PWM_Start(); channels beyond N are never started,
   see hrtim.h and ctrlr_config.h)
  ... plays back every uploaded (per, cmp0, ..., cmp(N-1)) entry in
      order, one period each ...
  (output stops on its own after the last entry — no further command
   or reply marks this; poll TABLE? or scope the outputs)
```

- No `ARM`/state-machine interlock exists in this firmware — `FIRE`
  always takes effect immediately, whether the controller was idle or
  already mid-shot (re-firing mid-shot restarts from step 0). If a
  fuller interlock/fault-gated firing sequence is ever needed, this is
  the command to extend, not a design decision this entry documents as
  final.
- Rejects with `ERR 5` if the table is empty (`TABLE?` reports 0).
  Without this check, an empty-table `FIRE` would still briefly enable
  outputs at step 0's (garbage, never-written) register contents before
  the very first master-repetition interrupt stopped them again —
  `ERR 5` catches this before it can happen at all.
- No `STOP` command exists yet — playback only stops on its own, at
  table exhaustion. Adding an operator-initiated stop is future work.

### `FAULT?`, `FAULT:CLEAR`

Status/clear for **two independent fault sources**, combined into one
answer here (`commands.c`'s `AnyFaultLatched()`) — from an operator's
perspective, "is there a fault, and can I `FIRE`" is one question, not
two, even though the two mechanisms underneath stay structurally
separate:

1. **PC10/HRTIM1_FLT6** — a real HRTIM1 hardware fault input
   (`Core/Inc/hrtim.h`), active-low. The peripheral forces every
   fault-enabled channel's outputs to a safe (inactive) level
   autonomously, in silicon, the instant the pin trips — no CPU,
   interrupt, or polling latency, and it keeps working even if the CPU
   is hung.
2. **GateDriverStatus_01..12 EXTI interrupt** (`PE0`-`PE11`,
   `Core/Inc/gate_driver.h`) — software/interrupt-driven. Re-reads all
   12 pins on any edge and evaluates them against the compile-time
   `GDS_FAULT_POLARITY` (`Core/Inc/ctrlr_config.h`); on a fault, forces
   HRTIM output off and latches. Needs the EXTI ISR to actually run,
   unlike PC10's autonomous hardware path — a boot-time explicit check
   (`main.c`) covers the one gap that leaves (a pin already faulted
   before the interrupt is even armed produces no edge of its own).

```
> FAULT?
< OK 0
> FIRE
< OK
   ... (a fault trips mid-shot; outputs go safe immediately) ...
> FAULT?
< OK 1
> FIRE
< ERR 6 Fault latched -- send FAULT:CLEAR first
> FAULT:CLEAR
< OK
> FAULT?
< OK 0
```

- **`FAULT?`** — `OK 0` (healthy) or `OK 1` (latched), either source.
- **`FAULT:CLEAR`** — clears both latches unconditionally (clearing a
  source that was never tripped is a harmless no-op). Does **not**
  itself reconnect/restart outputs — that's the next explicit `FIRE`'s
  job. For the GateDriverStatus source specifically, clearing
  immediately re-validates by re-checking all 12 pins: if any pin is
  still in its fault state, `FAULT:CLEAR` re-latches before its own
  `OK` reply even goes out, rather than reporting success while the
  physical condition persists — confirmed on real hardware (see
  `docs/changelog.txt`, 2026-09-08).
- Latched-until-cleared by design, both sources — a fault silently
  clearing itself, with output resuming unnoticed, is the hazard this
  avoids.
- **Verified on real hardware, end to end**: a genuine GateDriverStatus
  fault (GateDriverStatus_03/PE2, present at boot) was correctly
  caught by the boot-time check, correctly blocked `FIRE` with `ERR 6`,
  and correctly re-latched on `FAULT:CLEAR` while still physically
  present — the full sequence above, not just the command-layer shape
  of it. The PC10/HRTIM1_FLT6 path is verified idle-safe (doesn't
  spuriously trip) and does not yet have its own real low-drive bench
  trip confirmed on scope/DSLogic — see `docs/changelog.txt` for what
  specifically remains open there.

### `CONFig:CHANnels?`

Reports `HRTIM_NUM_CHANNELS` (`Core/Inc/ctrlr_config.h`) — the
compile-time HRTIM channel count this specific firmware build was
configured for (1-5; see that file for why not 6). Added 2026-09-08
alongside the channel-count generalization specifically so host
tooling can confirm what a board actually is instead of assuming a
value that could silently drift after a rebuild with a different
channel count.

```
> CONFIG:CHANNELS?
< OK 3
```

Not modifiable at runtime — there is no corresponding `SET` command,
by design. Changing it means editing `ctrlr_config.h`, rebuilding, and
reflashing.

### `GDS?`

Raw `HIGH`/`LOW` snapshot of the 12 GateDriverStatus_01..12 pins
(`PE0`–`PE11`, `GPIOE` — `docs/pin_mapping_v4.csv`), added 2026-09-08
as a diagnostic while investigating why a fault wasn't being
registered. Direct, uncached `GPIOE->IDR` read at the moment of the
query — no debounce, no polarity interpretation, no fault-latching of
its own. This command itself is read-only visibility and cannot stop
PWM output by itself. (These same 12 pins DO now gate PWM output, via
a separate mechanism — the GateDriverStatus EXTI fault interrupt, see
`FAULT?`/`FAULT:CLEAR` below, added the same day — `GDS?` is
unaffected by and independent of that mechanism either way, just a raw
snapshot.)

```
> GDS?
< OK 01=LOW 02=LOW 03=HIGH 04=LOW 05=LOW 06=LOW 07=LOW 08=LOW 09=LOW 10=LOW 11=LOW 12=LOW
```

One `OK` line, one `NN=HIGH` or `NN=LOW` token per pin, space
separated, `NN` = `01`–`12` matching the `GateDriverStatus_01`..`_12`
numbering (`01` = `PE0`, `12` = `PE11`). Kept to this project's
existing single-`OK <value>`-line response convention rather than the
sibling PFM-STM32G474 project's multi-line/bitmask `GDS?` reply
shapes.

### `FWUPdate:*` — in-application firmware update (dual-bank)

Added 2026-09-29 (ported from WHAM-XREX-PFMG474, where it was verified on
both boards). Updates the firmware over this same serial link at 115200
8N1, so it works through the ethernet serial bridge, which can't carry the
ROM bootloader's 8E1. Host tool: `python/fw_update.py` (`--status`,
`--no-swap`, `--rollback`).

The STM32G474 runs in dual-bank mode (`OPTR.DBANK=1`, 2 x 256 KB). The
running bank is always mapped at `0x08000000` and the other ("inactive")
bank at `0x08040000`. A new image — the same `.bin` that would be flashed
at `0x08000000`, no relinking — is written into the inactive bank,
CRC-checked and sanity-checked, then booted by setting `OPTR.BFB2`: the ROM
bootloader's dual-bank boot starts that bank with the banks swapped. The
previous image stays in the other bank; `FWUPdate:ROLLback` boots it again.

Strict stop-and-wait: `uart.c` holds one line at a time, so wait for each
reply before sending the next line.

| Command | Reply | Notes |
|---|---|---|
| `FWUPdate:BEGin <size> <crc32hex>` | `OK ERASED <n> PAGES BANK <b>` | Needs the output stopped (no `FIRE` in progress). `size` = image length padded with `0xFF` to a multiple of 8, at most 262144. CRC = standard CRC-32 (`zlib.crc32`) of the padded image. Erases only the inactive bank, then checks it reads blank |
| `FWUPdate:DATA <offsethex> <hex>` | `OK` | 8-48 bytes, a multiple of 8, offsets strictly in order from 0 |
| `FWUPdate:END` | `OK VERIFIED CRC=<crc>` | CRC over the written bank, then checks the initial stack pointer is in SRAM, the reset vector is a Thumb address inside the image, and the image contains this build's NUL-terminated product name (`HW_BOARD_NAME`), so another product's image can't be loaded |
| `FWUPdate:SWAP` | `OK SWAPPING -- rebooting into bank <b>` | Needs a verified image and the output stopped. Programs `BFB2`, then reloads option bytes (a full reset) |
| `FWUPdate:ROLLback` | same as `SWAP` | Boots the image already in the other bank, after the same sanity checks (no CRC — there's no reference). Refused mid-transfer |
| `FWUPdate:ABORt` | `OK` | Forgets a transfer. The inactive bank is left as is |
| `FWUPdate:STATus?` | `OK BANK=<1\|2> BFB2=<0\|1> DBANK=<0\|1> STATE=<IDLE\|RECEIVING\|VERIFIED> RX=<n>/<size>` | `BANK` = physical bank currently running |

Errors: `ERR 12` bad arguments/offset, `ERR 13` wrong state (output running,
no transfer, nothing verified), `ERR 17` flash/CRC/image/option-byte
failure. Nothing before the final swap can affect the running image.

**Before flashing with the ST-Link**, check `FWUPdate:STATus?` shows
`BANK=1 BFB2=0` (or `fw_update.py --rollback` until it does). With
`BFB2=1` the board boots physical bank 2, so a normal ST-Link write to
`0x08000000` would not be the image that runs. **The board needs a firm
NRST pull-up for the swap's reset to be reliable** — see AGENTS.md,
hard-won invariant 4.

**Recovery with an ST-Link** if a swapped-to image won't run: clear `BFB2`
so the board boots physical bank 1 normally, then reflash bank 1. Attach with
SWD only (leave the ST-Link's NRST wire off) and `reset_config none`.

### `DIAGnostic:OPTBytes?`

Reports the boot-source bits of the live `FLASH->OPTR`:

```
> DIAGnostic:OPTBytes?
< OK OPTR=BBEFF8AA nBOOT0=1 nSWBOOT0=0 nBOOT1=1
```

`nSWBOOT0=1` means boot0 is read from the physical `BOOT0`/`PB8` pin;
`0` means boot0 is taken entirely from `nBOOT0` and the pin is never
sampled. `nBOOT0` is the option-byte boot0 value, used only when
`nSWBOOT0` is `0`; `nBOOT1` combines with the effective boot0 to pick the
boot target.

### `DIAGnostic:RSTCause?` / `DIAGnostic:RSTCause:CLEar`

`RSTCause?` reports the `RCC->CSR` reset-cause flags:

```
> DIAGnostic:RSTCause?
< OK CSR=00000000 BOR=0 PIN=0 SFT=0 IWDG=0 WWDG=0 LPWR=0 OBL=0
```

`RSTCause:CLEar` zeroes them (`OK`). The boot banner already captures and
clears these flags at every boot, so this mostly shows resets since then.

### `QSPI:ID?`

QUADSPI connectivity test against the W25Q128JVS NOR flash wired to
`PE12`-`PE15`/`PB10`/`PB11` (all AF10 — `docs/pin_mapping_v4.csv`),
added 2026-09-08 as an **easily removable** module
(`Core/Src/Inc/qspi_test.c/.h`) — see `QSPI_TEST_FEATURE_ENABLED` in
`qspi_test.h`; when disabled, this command is entirely absent from the
table (`ERR 1 Unknown command`, like any unrecognized mnemonic), the
same removability pattern `BOOT` uses. Deliberately narrow: issues the
chip's standard JEDEC Read ID instruction (`0x9F`, plain 1-line mode,
no Quad Enable bit required) and reports the 3 raw ID bytes as
uppercase hex — nothing else. No program/erase, no memory-mapped
access.

```
> QSPI:ID?
< OK EF 40 18
```

No expected value is hardcoded or asserted anywhere in this
command — it reports whatever the chip actually says; compare against
the W25Q128JVS datasheet's own JEDEC ID table yourself (`EF` = Winbond
manufacturer ID; the other two bytes are memory type/capacity). `ERR 7`
on any `HAL_QSPI` command/receive failure or timeout — e.g. no chip
present, a wiring fault, or a wedged bus.

**Verified on real hardware**: `OK EF 40 18` — correct Winbond
manufacturer ID and correct W25Q128 memory type/capacity bytes,
confirmed repeatable (3x) after reflashing. A real bug was found and
fixed getting here: the first attempt returned a stable "DE 80 30"
instead, traced to a QUADSPI sample-timing setting
(`SampleShifting`) — see `docs/changelog.txt` and `qspi_test.c`'s own
bugfix comment for the full diagnosis (an exact 1-bit shift of the
correct value, not random noise) and fix.

## Adding a command

From `cmd_parser.c`'s own header comment:

1. Implement the handler in `commands.c`.
2. Declare it in `commands.h`.
3. Add a `{ "PATTern:MNEMonic?", handler }` row to `command_table[]` in
   `cmd_parser.c`.

Nothing else changes — the table is flat, so a new leaf or a whole new
subsystem is always just one more row. Update this document when you do.
