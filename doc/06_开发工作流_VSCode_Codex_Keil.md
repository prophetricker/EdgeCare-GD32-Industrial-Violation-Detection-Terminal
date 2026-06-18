# EdgeCare Development Workflow

Goal: use VS Code and Codex for editing, Keil MDK for compiling, downloading, and debugging.

## Daily Loop

1. Codex edits code and updates `看板.md`.
2. Press `Ctrl+Shift+B` in VS Code to run the Keil build task.
3. If build fails, Codex reads `EdgeCare_GD32_Industrial_Violation_Terminal/build-keil.log` and fixes the first real error.
4. If build succeeds, download/debug from Keil.
5. Open Serial Monitor on `COM8`, `115200 8N1`, press RESET, and observe boot/probe logs.
6. Feed useful serial logs back into the project notes or directly to Codex.

## Current Build Task

VS Code task:

```text
Keil: Build EdgeCare GD32H759I-START
```

It runs:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_keil.ps1
```

from:

```text
EdgeCare_GD32_Industrial_Violation_Terminal
```

## Pre-Commit Verify Task

VS Code task:

```text
Project: Verify EdgeCare
```

It runs:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\verify_edgecare.ps1
```

from the project root. The script checks:

- `git diff --check`
- `git diff --cached --check`
- project safety scan, when the local Codex kanban skill is installed
- Keil command-line build
- `build-keil.log` contains `0 Error(s), 0 Warning(s)`

## Keil Responsibilities

Keep these in Keil for now:

- Device pack and target options
- CMSIS-DAP/GD-Link connection
- Flash algorithm configuration
- Download
- Breakpoint debugging
- Register/memory inspection

This keeps the workflow stable while the OpenOCD/GD-Link flash path is uncertain for GD32H759.

## J-Link Automation

J-Link is now validated for the external SWD header path:

- `J-Link SWDIO -> board SWDIO`
- `J-Link SWCLK -> board SWCLK`
- `J-Link GND -> board GND`
- VTref is read from the target board, measured around `3.22 V` during probe.

Safe read-only probe:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\jlink_probe.ps1
powershell -ExecutionPolicy Bypass -File .\tools\jlink_probe.ps1 -Device GD32H759IMT6
```

Reset and capture startup serial logs without touching the board:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\jlink_reset_capture_serial.ps1
```

This opens `COM8` at `115200 8N1`, issues a J-Link software reset/run, captures startup logs, saves a copy under `.embeddedskills/logs/serial/`, and prints the key `camera_*` lines. Close VS Code Serial Monitor first if it already owns `COM8`.

Important device-name rule:

- Keil project device name: `GD32H759IM`
- SEGGER/J-Link device name verified on the bench: `GD32H759IMT6`
- Do not use Keil's shorter `GD32H759IM` as the J-Link `-Device` value; it triggered a SEGGER module error during testing.
- `Cortex-M7` is acceptable for read-only core probing, but not for internal Flash programming.

J-Link flash script is intentionally explicit:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\jlink_flash_edgecare.ps1
```

The command above is a dry run. It only prints the target AXF and device settings. To actually program the target, use `-Flash` only after confirming the latest Keil build is clean:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\verify_edgecare.ps1
powershell -ExecutionPolicy Bypass -File .\tools\jlink_flash_edgecare.ps1 -Flash
```

Keil command-line download is available when the Keil/J-Link project settings are needed:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\keil_flash_edgecare.ps1
```

The command above is a dry run. To actually download the latest AXF through Keil, use:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\keil_flash_edgecare.ps1 -Flash
```

This script fixes the local `MDK-ARM/JLinkSettings.ini` if it contains the stale line `Device="ARM7"`. That stale local setting makes Keil/J-Link fail with `ARM7 is not supported via SWD`; the correct SEGGER device line is `Device="GD32H759IMT6"`.

If SWD download or reset starts timing out, run the read-only health check before trying more flashes:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\jlink_swd_health_check.ps1
```

Interpret the summary fields directly:

- `ready_to_flash_sweep=1`: the current AXF is the prepared camera sweep diagnostic artifact.
- `swd_dp_ok=0`: J-Link sees target voltage but cannot read the SWD debug port; check `GND`, `SWDIO`, and `SWCLK` wiring/contact first.
- `swd_dp_ok=0` with `reset_line_observed=1`: J-Link can see target power and can drive `NRST`, so prioritize the SWD data path itself: `SWDIO -> PA13/JTMS`, `SWCLK -> PA14/JTCK`, possible line swap, contact, or a J-Link SWD-output issue.
- `reset_line_observed=0`: the J-Link reset line did not visibly restart the firmware; check `NRST` wiring/contact, or use manual RESET only for serial capture.
- `serial_lines>0`: COM8 USART logging is alive, so the MCU is still running even if SWD is broken.

Keil GUI note: if Keil/J-Link logs show `JLINK_TIF_Select(JLINKARM_TIF_JTAG)`, set the debug adapter interface back to `SWD` in Keil. The command-line J-Link scripts in this repository already force `-If SWD`; if they still report `swd_dp_ok=0`, continue with the physical `SWDIO/SWCLK` path checks above.

For GDB-style debugging, start the local J-Link GDB server:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\jlink_gdbserver_edgecare.ps1
```

It defaults to `GD32H759IMT6`, SWD, `4000 kHz`, GDB port `2331`, SWO port `2332`, and telnet port `2333`.

## Codex Responsibilities

Codex should:

- Change source files and docs.
- Keep `main.c` from growing further during refactor.
- Read `build-keil.log` when build fails.
- Use the installed `keil` skill after Codex restart for scan/build/log parsing.
- Use the installed `serial` skill after Codex restart for COM discovery and bounded log capture.
- Keep data collection scripts aligned with the firmware `gray96` model input format.

Codex should not:

- Auto-flash without explicit user request.
- Rewrite `.uvprojx` casually.
- Treat placeholder inference as real AI.

## Data Collection Entry

For the complete Chinese dataset collection procedure, read:

```text
doc/01_数据集采集全流程手册.md
```

For lower-level dump and diagnostic details, read:

```text
doc/05_gray96数据采集与诊断细节.md
```

The current dataset capture path uses `OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00`, raw `gray96`, and the PC script:

```powershell
py .\tools\collect_gray96_serial.py --label empty --count 1 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
```

## Embeddedskills Configuration

The project-level configuration is:

```text
.embeddedskills/config.json
```

It pins:

- Keil project path
- Keil target name
- build log directory
- serial defaults for `COM8`, `115200 8N1`

After restarting Codex, installed skills can use this configuration instead of rediscovering the project every time.
