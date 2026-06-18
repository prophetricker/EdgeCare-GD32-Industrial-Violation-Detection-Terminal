# EdgeCare Project Instructions

Use these instructions for work under this project root.

## Project Context

- Product: `EdgeCare_GD32_Industrial_Violation_Terminal`
- Board: `GD32H759I-START`
- Contest direction: GigaDevice Endpoint AI / edge AI terminal
- MVP: fixed-camera dangerous-zone intrusion detection
- Current validated links: LD2410 GPIO trigger, alarm GPIO, USART0 logs, OV5640 SCCB ID, OV5640 DCI/DMA QVGA capture, gray96 preprocessing, placeholder `edgecare_infer()`

## Operating Rules

- Update `看板.md` before or after meaningful project changes.
- Keep code changes narrow and buildable in Keil MDK.
- Do not auto-flash unless the user explicitly asks. Default workflow is build only, then user downloads/debugs in Keil.
- Keep `main.c` moving toward orchestration only: hardware drivers, preprocessing, model inference, and logging belong in modules.
- Do not hide placeholder behavior. Anything that is not a real model must be labeled as placeholder/demo in code comments and logs.
- Do not commit or package Keil build outputs, large SDK archives, model weights, raw datasets, APKs, videos, or secret files.

## Build

From the firmware directory:

```powershell
cd "D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST\EdgeCare_GD32_Industrial_Violation_Terminal"
powershell -ExecutionPolicy Bypass -File .\build_keil.ps1
```

Expected success line in `build-keil.log`:

```text
0 Error(s), 0 Warning(s)
```

## Serial Observation

- Current user-side log port: `COM8`
- Baud: `115200`
- Format: `8N1`
- Main logs must keep this stable shape:

```text
[ts_ms] state=... radar=... infer_ms=... conf=... alarm=... seq=...
```

Startup probe logs may be verbose, but periodic logs should remain compact and parseable.

## Key Project Docs

- `CODING_STYLE.md`: firmware structure and embedded C rules
- `doc/00_先看这里_文档索引.md`: human-readable document entry
- `doc/02_硬件引脚与接线说明.md`: actual wiring and pinout
- `doc/04_OV5640相机调试记录.md`: OV5640/DCI bring-up notes
- `doc/06_开发工作流_VSCode_Codex_Keil.md`: VS Code + Codex + Keil workflow
- `看板.md`: current status and next actions
