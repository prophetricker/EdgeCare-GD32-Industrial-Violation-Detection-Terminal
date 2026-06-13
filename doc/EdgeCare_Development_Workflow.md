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

For model data collection, read:

```text
doc/EdgeCare_Data_Collection.md
```

The current capture path uses a firmware switch `EDGECARE_ENABLE_GRAY96_DUMP` and the PC script:

```powershell
py .\tools\collect_gray96_serial.py --label empty --count 1 --port COM8
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
