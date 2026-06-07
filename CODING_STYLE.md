# EdgeCare Firmware Coding Style

This file defines the target structure for refactoring the current bring-up firmware. It is intentionally lightweight: enough discipline to keep the demo maintainable, without imposing full MISRA work during the 15-day initial-round build.

## Target Module Layout

Keep vendor libraries and demo-suite files where they are. Refactor only the EdgeCare application code under:

```text
GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/
```

Target layout:

```text
app/
  edgecare_app.c/.h          state machine and demo business flow
board/
  board_config.h             pin mapping, serial/log constants, frame sizes
bsp/
  bsp_alarm.c/.h             PA8 active-low alarm output
  bsp_radar_ld2410.c/.h      PF8 LD2410 OUT digital input
  bsp_camera_ov5640.c/.h     SCCB, OV5640 init, DCI/DMA capture
platform/
  edgecare_log.c/.h          printf wrappers and fixed log format
vision/
  edgecare_preprocess.c/.h   YUYV to gray96, ROI, scaling
model/
  edgecare_infer.c/.h        placeholder now; real model later
main.c                       cache/systick/board init, then app loop only
```

`main.c` should eventually contain no register-level camera, radar, alarm, or model details.

## Naming

- Public module functions use `module_action_object()` style, for example `bsp_alarm_set_active()` or `edgecare_app_step()`.
- File-local helpers and globals are `static`.
- File-local globals use `g_`.
- Constants and macros use `UPPER_SNAKE_CASE`.
- Types use lower snake case with `_t`, for example `edgecare_context_t`.
- Hardware pins should not be duplicated across files. Put final pin definitions in `board_config.h`.

## Embedded C Rules

- No dynamic allocation.
- No recursion.
- No blocking wait without a timeout.
- DMA buffers must be explicitly aligned and documented.
- DCache clean/invalidate calls must be near DMA ownership transfers.
- ISR code, when added, should only set flags or move small fixed data.
- Keep units explicit in names: `_ms`, `_bytes`, `_words`, `_percent`.
- Prefer `uint8_t`, `uint16_t`, `uint32_t` over plain `int` for hardware data.
- Keep public headers small: types, constants, and public functions only.

## Logging Rules

Periodic status log stays stable:

```text
[ts_ms] state=... radar=... infer_ms=... conf=... alarm=... seq=...
```

Structured upload target:

```json
{"dev":"edgecare-01","event":"zone_intrusion","conf":0.87,"lat_ms":132,"seq":42}
```

Rules:

- Do not change field names casually.
- Add new probe logs with clear prefixes such as `camera_`, `preprocess_`, `infer_`, `vw553_`.
- Do not print full frames or large buffers over UART.

## Model Boundary

`edgecare_infer()` is the only application-facing model API. It should accept a fixed-size grayscale input buffer and return:

- class: safe/intrusion
- confidence
- inference time if measured outside the function

The current statistics-based implementation is a placeholder and must remain labeled as such until replaced by a trained model or documented traditional ML implementation.

## Refactor Order

Refactor in small buildable steps:

1. Extract `board_config.h` with constants and pin mappings.
2. Extract `bsp_alarm` and `bsp_radar_ld2410`.
3. Extract `edgecare_log` and state formatting.
4. Extract `vision/edgecare_preprocess`.
5. Extract `model/edgecare_infer`.
6. Extract `bsp_camera_ov5640`.
7. Reduce `main.c` to initialization plus `edgecare_app_step()`.

After each step, run the Keil build and update `看板.md`.
