# EdgeCare Wiring Table

Updated: 2026-06-16

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

## 4. OV5640 Camera SCCB and Control

For the current dataset-collection build, keep these wires connected. For a brand-new board bring-up, these can still be connected first before adding the DVP bus.

| Camera module | GD32H759I-START | Direction | Required | Notes |
| --- | --- | --- | --- | --- |
| `3V3` | `3V3` | power | yes | Do not use 5V |
| `GND` | `GND` | common | yes | Must share ground |
| `SCL` | `PF1 / I2C1_SCL` | GD32 -> camera | yes | Remapped after `PB11` header damage |
| `SDA` | `PF0 / I2C1_SDA` | bidirectional | yes | Remapped after `PB11` header damage |
| `RES` | `PD0` | GD32 -> camera | yes | Camera reset |
| `PWON` / `PWDN` | `PD1` | GD32 -> camera | yes | Camera power-down / enable control |

Expected boot log:

```text
camera_sccb: SCL=PF1 SDA=PF0 RES=PD0 PWON=PD1
camera_id: ov5640_regs[0x300A,0x300B]=0x56 0x40
```

## 5. OV5640 Full DVP Camera Bus

Keep these wired for the current raw `gray96` dataset path.

| Camera module | GD32H759I-START | START header | Direction | Notes |
| --- | --- | --- | --- | --- |
| `PCLK` | `PA6 / DCI_PIXCLK` | `JP9-2` | camera -> GD32 | Pixel clock; keep wire short |
| `HREF` | `PA4 / DCI_HSYNC` | `JP8-29` | camera -> GD32 | Horizontal reference |
| `SYNC` / `VSYNC` | `PB7 / DCI_VSYNC` | `JP11-25` | camera -> GD32 | Vertical sync |
| `D0` | `PC6 / DCI_D0` | `JP10-23` | camera -> GD32 | Data bit 0 |
| `D1` | `PC7 / DCI_D1` | `JP10-24` | camera -> GD32 | Data bit 1 |
| `D2` | `PC8 / DCI_D2` | `JP10-25` | camera -> GD32 | Data bit 2 |
| `D3` | `PG11 / DCI_D3` | `JP11-16` | camera -> GD32 | Avoid `PC9`, which is connected to LED1 |
| `D4` | `PC11 / DCI_D4` | `JP11-5` | camera -> GD32 | Remapped after `PE4` header damage; START LED3 load risk |
| `D5` | `PB6 / DCI_D5` | `JP11-24` | camera -> GD32 | Data bit 5 |
| `D6` | `PE5 / DCI_D6` | `JP8-4` | camera -> GD32 | Data bit 6 |
| `D7` | `PB9 / DCI_D7` | `JP11-28` | camera -> GD32 | Remapped after `PE6` header damage |

Current firmware boot print should match:

```text
camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PC11 D5=PB6 D6=PE5 D7=PB9
```

## 6. Current Camera Dataset Mode

The current firmware is a dataset-collection build:

- OV5640 uses the `OV5640_JPEG_TO_YUV_REF` real-scene path.
- GD32 DCI uses PCLK rising edge.
- OV5640 `0x4745` data order is pinned to `0x00`.
- `preprocess_gray96` must print `scale=raw`.
- `EDGECARE_ENABLE_GRAY96_DUMP=1U` is enabled for serial sample export.
- `EDGECARE_ENABLE_CAMERA_BYTE_PLANE_DUMP=0U` is normally disabled.

For collection, keep this standard wiring before flashing/resetting:

```text
PCLK -> PA6
HERF/HREF -> PA4
SYNC -> PB7
```

Do not probe candidate pins during collection. The current raw `gray96` path already has real-scene evidence.

After flashing the latest build, run the collector and press RESET once per sample:

```powershell
py .\tools\collect_gray96_serial.py --label empty --count 1 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
```

Valid boot/capture logs should include:

```text
camera_capture[normal_jpeg_to_yuv_ref]: data_order=0x00/read0x00 dma=done words=38400 remain=0
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=raw ...
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
| `PC11` | Connected to START board LED3; currently used for DCI_D4 only because `PE4` is damaged |
| `PB10/PB11` | Old SCCB pair; current board has `PB11` damage, use `PF1/PF0` |
| `PE4/PE6` | Damaged camera data pins on the current board; use `PC11/PB9` |
| `PA8` | Already used by alarm output; also a possible CK_OUT0 camera-clock conflict |
| `PH4` | Used by some EVAL examples for I2C SCL, but not exposed on GD32H759I-START headers |
