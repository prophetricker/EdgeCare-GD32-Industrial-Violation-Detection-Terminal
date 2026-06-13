# EdgeCare Wiring Table

Updated: 2026-06-09

This file is the quick wiring checklist for the current `GD32H759I-START` EdgeCare prototype. For detailed pin rationale and bring-up notes, read `doc/EdgeCare_Hardware_Pinout.md`.

## Safety Rules

- All signal wires connected to GD32 GPIO must be `3.3 V` logic.
- Every external module must share `GND` with the GD32 board.
- Power the OV5640 camera from `3V3`, not `5V`.
- Before connecting LD2410 `O` to `PF8`, measure `O` high level once with a multimeter and confirm it is about `3.3 V`.
- Do not connect or disconnect the OV5640 DVP data bus while the board is powered.
- Keep wires short for `PCLK`, `HREF`, `SYNC`, and `D0-D7`; long Dupont wires can make the camera unstable.

## 1. USB-TTL Serial Log

Use this to view firmware logs on PC Serial Monitor.

Serial settings:

```text
COM8, 115200 baud, 8N1, no flow control
```

| USB-TTL module | GD32H759I-START | Direction | Required | Notes |
| --- | --- | --- | --- | --- |
| `GND` | `GND` | common | yes | Must share ground |
| `RXD` | `PF4 / USART0_TX` | GD32 -> PC | yes | Receives EdgeCare logs |
| `TXD` | `PF5 / USART0_RX` | PC -> GD32 | optional | Not required for current log-only firmware |
| `VCC` | not connected | power | no | Do not power the board from USB-TTL unless intentionally doing so |

Expected periodic log:

```text
[0] state=IDLE radar=0 infer_ms=0 conf=0.00 alarm=0 seq=1
```

## 2. Alarm Lamp

The current alarm module has `VCC`, `GND`, and `IN`. Its `IN` is active-low.

| Alarm lamp | GD32H759I-START / power | Direction | Required | Notes |
| --- | --- | --- | --- | --- |
| `VCC` | module-rated power | power | yes | Use the voltage written on the alarm module |
| `GND` | `GND` | common | yes | Must share ground with GD32 |
| `IN` | `PA8` | GD32 -> alarm | yes | `0` turns alarm on, `1` turns alarm off |

Firmware config:

```c
#define ALARM_GPIO_PORT GPIOA
#define ALARM_GPIO_PIN  GPIO_PIN_8
```

## 3. LD2410 Radar GPIO Trigger

Current plan uses the LD2410 adapter board's `V/G/O` pins. This is enough for the initial demo.

| LD2410 adapter | GD32H759I-START / power | Direction | Required | Notes |
| --- | --- | --- | --- | --- |
| `V` | `5V` | power | yes | LD2410 module power input |
| `G` | `GND` | common | yes | Must share ground with GD32 |
| `O` | `PF8` | radar -> GD32 | yes | Active-high: human target = `1` |

Firmware config:

```c
#define RADAR_GPIO_PORT   GPIOF
#define RADAR_GPIO_PIN    GPIO_PIN_8
#define RADAR_ACTIVE_HIGH 1U
```

Current LD2410 setting:

| Parameter | Value |
| --- | --- |
| `OUT` polarity | high = detected person |
| no-person duration | `1 s`, set by `HLKRadarToolAPP` |
| GD32 polling/log period | `500 ms` |

Expected behavior:

```text
person enters -> radar=1 -> alarm=1
person leaves -> about 1 s radar hold -> radar=0 -> alarm=0
```

## 4. OV5640 Camera Minimal SCCB Test

Connect only these wires first. Do not connect the full DVP bus until the ID read works.

| Camera module | GD32H759I-START | Direction | Required | Notes |
| --- | --- | --- | --- | --- |
| `3V3` | `3V3` | power | yes | Do not use 5V |
| `GND` | `GND` | common | yes | Must share ground |
| `SCL` | `PB10 / I2C1_SCL` | GD32 -> camera | yes | SCCB/I2C clock |
| `SDA` | `PB11 / I2C1_SDA` | bidirectional | yes | SCCB/I2C data |
| `RES` | `PD0` | GD32 -> camera | yes | Camera reset |
| `PWON` / `PWDN` | `PD1` | GD32 -> camera | yes | Camera power-down / enable control |

Expected boot log:

```text
camera_sccb: SCL=PB10 SDA=PB11 RES=PD0 PWON=PD1
camera_id: ov5640_regs[0x300A,0x300B]=0x56 0x40
```

## 5. OV5640 Full DVP Camera Bus

Connect these only after the SCCB ID test succeeds.

| Camera module | GD32H759I-START | START header | Direction | Notes |
| --- | --- | --- | --- | --- |
| `PCLK` | `PA6 / DCI_PIXCLK` | `JP9-2` | camera -> GD32 | Pixel clock; keep wire short |
| `HREF` | `PA4 / DCI_HSYNC` | `JP8-29` | camera -> GD32 | Horizontal reference |
| `SYNC` / `VSYNC` | `PB7 / DCI_VSYNC` | `JP11-25` | camera -> GD32 | Vertical sync |
| `D0` | `PC6 / DCI_D0` | `JP10-23` | camera -> GD32 | Data bit 0 |
| `D1` | `PC7 / DCI_D1` | `JP10-24` | camera -> GD32 | Data bit 1 |
| `D2` | `PC8 / DCI_D2` | `JP10-25` | camera -> GD32 | Data bit 2 |
| `D3` | `PG11 / DCI_D3` | `JP11-16` | camera -> GD32 | Avoid `PC9`, which is connected to LED1 |
| `D4` | `PE4 / DCI_D4` | `JP8-3` | camera -> GD32 | Data bit 4 |
| `D5` | `PB6 / DCI_D5` | `JP11-24` | camera -> GD32 | Data bit 5 |
| `D6` | `PE5 / DCI_D6` | `JP8-4` | camera -> GD32 | Data bit 6 |
| `D7` | `PE6 / DCI_D7` | `JP8-5` | camera -> GD32 | Data bit 7 |

Current firmware boot print should match:

```text
camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PE4 D5=PB6 D6=PE5 D7=PE6
```

## 6. Current Camera Diagnostic Mode

The current firmware is still a camera diagnostic build:

- OV5640 init defaults to the full app-note VGA YUV reference table plus QVGA output override; startup logs should show `camera_init_path=full_reference_vga_then_qvga`.
- OV5640 ISP color bar test pattern is enabled by default.
- DVP data order is swept to identify the correct byte/bit order.
- OV5640 sync output sweep is enabled: `camera_sync_sweep[...]` temporarily forces HREF/VSYNC pad values through `0x3017/0x301D/0x301A`.
- OV5640 timing sweep is enabled: `camera_timing_sweep[...]` tries DVP timing/sync register variants through `0x471B/0x471D/0x4730/0x4740`; a candidate is kept only if it reaches the line-sync threshold.
- `byte_plane` dumps are diagnostic images, not training data.
- Do not start real dataset collection until the diagnostic image is visually valid.

For the next timing-output test, keep this standard wiring before flashing/resetting:

```text
PCLK -> PA6
HERF/HREF -> PA4
SYNC -> PB7
```

Do not probe candidate pins during this test. The latest forced-output sweep already proved that `HERF/HREF -> PA4` reaches the OV5640 HREF pad and `SYNC -> PB7` reaches the OV5640 VSYNC pad.

After flashing the latest build, open Serial Monitor, press RESET, and copy only these lines:

```text
camera_init: ... camera_init_path=full_reference_vga_then_qvga
camera_timing_sweep[...]
camera_timing_sweep_best: ...
camera_dvp_regs[after_timing_sweep]: ...
camera_dvp_ext_regs[after_timing_sweep]: ...
camera_capture[pclk_...]: ...
```

## 7. VW553 Upload Module

Not wired yet.

| VW553 signal | GD32H759I-START | Status | Notes |
| --- | --- | --- | --- |
| `TX` | TBD GD32 UART RX | pending | For H7 receiving from VW553 if needed |
| `RX` | TBD GD32 UART TX | pending | H7 will send JSON alarm messages |
| `GND` | `GND` | pending | Common ground required |
| `VCC` | module-rated power | pending | Confirm voltage before wiring |

Target message from H7 to VW553:

```json
{"dev":"edgecare-01","event":"zone_intrusion","conf":0.87,"lat_ms":132,"seq":42}
```

## 8. Pin Avoid List

| Pin | Avoid reason |
| --- | --- |
| `PC9` | Connected to START board LED1; avoid using it as DCI_D3 |
| `PA8` | Already used by alarm output; also a possible CK_OUT0 camera-clock conflict |
| `PH4` | Used by some EVAL examples for I2C SCL, but not exposed on GD32H759I-START headers |
