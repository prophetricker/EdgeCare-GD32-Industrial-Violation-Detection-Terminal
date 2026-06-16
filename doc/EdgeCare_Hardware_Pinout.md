# EdgeCare Hardware Pinout

Updated: 2026-06-16

This file records the current wiring for the initial-round EdgeCare demo on `GD32H759I-START`.

For a direct wire-by-wire checklist, read `doc/EdgeCare_Wiring_Table.md`.

## Confirmed Wiring

| Module | Signal | GD32H759I-START Pin | Direction | Notes |
| --- | --- | --- | --- | --- |
| USB-TTL serial log | RXD | `PF4 / USART0_TX` | H7 -> PC | Serial Monitor receives logs here |
| USB-TTL serial log | TXD | `PF5 / USART0_RX` | PC -> H7 | Optional for current log-only firmware |
| USB-TTL serial log | GND | `GND` | common | Must share ground |
| Alarm lamp | `IN` | `PA8` | H7 -> alarm | Active-low; `0` turns alarm on |
| Alarm lamp | `VCC` | external/module-rated power | power | Match the alarm module label |
| Alarm lamp | `GND` | `GND` | common | Must share ground with H7 |
| HLK-LD2410B adapter | `O` | `PF8` | radar -> H7 | Active-high target state output |
| HLK-LD2410B adapter | `V` | `5V` | power | Official module VCC is 5V |
| HLK-LD2410B adapter | `G` | `GND` | common | Must share ground with H7 |

## LD2410B Configuration State

| Parameter | Current Value | Source |
| --- | --- | --- |
| `OUT` polarity | High = human target, low = no target | Official manual and verified by GPIO logs |
| `OUT` IO level | 3.3V logic | Official manual |
| No-person duration | `1 s` | Set by user with `HLKRadarToolAPP` |
| H7 polling period | `500 ms` | `EDGECARE_LOG_PERIOD_MS` in firmware |

Expected behavior after no-person duration is set to `1 s`:

```text
person enters radar area  -> radar=1 -> alarm=1 within 0-500 ms
person leaves radar area  -> LD2410 waits about 1 s -> radar=0 -> alarm=0 within another 0-500 ms
```

## LD2410B Detection Area

Main reference:

```text
doc/hardware_manuals/HLK_LD2410B_official/HLK LD2410B生命存在感应模组说明书 V1.0 9.pdf
```

Useful facts:

- Indoor human-presence radar.
- Up to `6 m` sensing distance.
- Configurable detection range: `0.75 m - 6 m`.
- Detection angle: about `±60 deg`.
- Distance resolution: `0.75 m`.
- Detects moving and static/micro-moving human targets.

For EdgeCare, use LD2410B as a broad forward fan/cone trigger. Do not treat it as an exact dangerous-zone boundary. The camera ROI and model should make the final zone-intrusion decision.

## Remaining Pending Wiring

| Module | Signal | GD32 Pin | Status | Notes |
| --- | --- | --- | --- | --- |
| GD32VW553 | UART TX/RX | TBD | pending | Later for structured alarm upload |
| OLED SSD1306 | I2C/SPI | TBD | optional | Later status display |

## Current Wiring Task

The camera wiring is currently frozen for dataset collection. Do not move camera wires unless the normal raw `gray96` path regresses.

Current collection evidence uses:

```text
OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00 + raw gray96
```

## LXB-OVX640 Camera Module Pinout From Photo

The photo appears to show a 2x9 signal header. Read this table with the lens/FPC connector side as the module top and the black pin header at the bottom.

| Header row | Left-to-right signals from silk | Notes |
| --- | --- | --- |
| Upper row | `PWON`, `PCLK`, `D6`, `D4`, `D2`, `D0`, `SDA`, `SCL`, `GND` | `PWON` is probably power-down/power enable, active level must be verified |
| Lower row | `FLASH`, `D7`, `D5`, `D3`, `D1`, `RES`, `HREF`, `SYNC`, `3V3` | `HREF` is printed like `HERF`; `SYNC` is likely `VSYNC` |

Derived camera signals:

| Camera Function | Module Signal |
| --- | --- |
| Data bus | `D0-D7` |
| Pixel clock | `PCLK` |
| Horizontal reference | `HREF` |
| Vertical sync | `SYNC` |
| SCCB/I2C data | `SDA` |
| SCCB/I2C clock | `SCL` |
| Reset | `RES` |
| Power-down / power enable | `PWON` |
| Flash LED control | `FLASH` |
| Power | `3V3`, `GND` |

The module does not expose a visible `XCLK/MCLK` pin in the photo. Treat it as onboard-clock capable until proven otherwise.

## GD32H759I-START Camera Pin Map

The `GD32H759I_START_Demo_Suites/Docs/Schematic/GD32H759I-START-V1.5.pdf` schematic confirms that the needed DCI camera pins are exposed on the START expansion headers. `PH4` is not exposed on this START board, so the EVAL example's `PH4 / I2C1_SCL` must not be copied directly.

Current firmware uses the table below and prints it at boot. After flashing, Serial Monitor should show one `camera_capture:` line before the periodic `state=...` logs. If that line is missing, the flashed firmware is older than the current workspace build.

### Recommended Wiring

| Camera Signal | Recommended GD32H759I-START Pin | START Header From Schematic | Notes |
| --- | --- | --- | --- |
| `PCLK` | `PA6 / DCI_PIXCLK` | `JP9-2` | Pixel clock input; START demo `readme.txt` says `PA6` is also LED2, so this pin is a capture-time risk if the LED load is still fitted |
| `HREF` / `HSYNC` | `PA4 / DCI_HSYNC` | `JP8-29` | Module silk says `HREF` |
| `VSYNC` / `SYNC` | `PB7 / DCI_VSYNC` | `JP11-25` | Module silk says `SYNC`; likely vertical sync |
| `D0` | `PC6 / DCI_D0` | `JP10-23` | Data bit 0 |
| `D1` | `PC7 / DCI_D1` | `JP10-24` | Data bit 1 |
| `D2` | `PC8 / DCI_D2` | `JP10-25` | Data bit 2 |
| `D3` | `PG11 / DCI_D3` | `JP11-16` | Prefer this over `PC9`, because `PC9` is connected to START LED1 |
| `D4` | `PC11 / DCI_D4` | `JP11-5` | Remapped after `PE4` header damage; `PC11` is connected to START LED3, so this pin is a capture-time risk |
| `D5` | `PB6 / DCI_D5` | `JP11-24` | Data bit 5 |
| `D6` | `PE5 / DCI_D6` | `JP8-4` | Data bit 6 |
| `D7` | `PB9 / DCI_D7` | `JP11-28` | Remapped after `PE6` header damage |
| `SCL` | `PF1 / I2C1_SCL` | `JP8-9` | Remapped as a pair with SDA after `PB11` header damage |
| `SDA` | `PF0 / I2C1_SDA` | `JP8-8` | Remapped after `PB11` header damage |
| `RES` | any free GPIO, suggested `PD0` | `JP11-6` | Camera reset output from H7 |
| `PWON` / `PWDN` | any free GPIO, suggested `PD1` | `JP11-7` | Active level must be verified by ID-read test |
| `3V3` | `3V3` | power header | Do not power from `5V` |
| `GND` | `GND` | power header | Common ground |

### Avoided Pins

| Pin | Reason |
| --- | --- |
| `PC9` | Connected to START board `LED1`; it is also a DCI_D3 candidate, but using it may load or disturb the camera data line |
| `PC11` | Connected to START board `LED3`; it is now used only because `PE4` is damaged. If D4 symptoms appear, check LED3 loading first |
| `PA8` | Current alarm output and possible CK_OUT0 camera clock conflict; the camera photo does not expose `XCLK`, so do not use it unless ID/capture fails due missing clock |
| `PH4` | Used by the EVAL camera example for I2C SCL, but not exposed on the START board headers |

Immediate rule: for normal dataset collection, keep the full camera bus wired exactly as listed above. The old minimum SCCB-only bring-up step is historical.

## Current DCI Capture State

Current firmware path:

```text
main.c -> edgecare_camera_id_probe() -> edgecare_camera_capture_probe() -> periodic edgecare_step()
```

Current dataset mode:

- OV5640 real-scene JPEG-to-YUV reference path is enabled.
- Test pattern is off in the normal collection path.
- DVP data order is pinned to `0x4745=0x00`.
- GD32 DCI uses PCLK rising edge with the known working HS/VS polarity pair.
- `preprocess_gray96` should print `scale=raw`, not `scale=lshift2`.

Expected camera probe output:

```text
camera_init: readback 3008=0x02 size=320x240 polarity_4740=0x20 test_503d=0x00 test_4741=0x00 ...
camera_capture_probe: start qvga=320x240 bytes=153600 words=38400 timeout=60000000 test_pattern=0 mode=1 ...
camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PC11 D5=PB6 D6=PE5 D7=PB9
camera_dvp_gpio: bursts=80 samples_per_burst=50000 pclk_edges=... href_edges=... sync_edges=... href_high=... sync_high=...
camera_capture[normal_jpeg_to_yuv_ref]: data_order=0x00/read0x00 dma=done words=38400 remain=0 ...
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=raw qvga=320x240 out=96x96 bytes=9216 ...
infer_probe: model=stat_placeholder input=gray96 mean=... contrast=... conf=... intrusion=...
```

For data collection, burn the current Keil build, run `tools/collect_gray96_serial.py`, and press RESET once per sample. The `camera_capture:` line still decides whether collection is valid:

- `camera_init: write_failed`: SCCB write path failed even though ID read may work; check I2C bus stability and reset/power sequencing.
- `pclk_edges` is nonzero but `href_edges/sync_edges` remain zero: the sensor is outputting pixel clock but H7 is not seeing line/frame sync; check camera `HREF` and `SYNC` wires, then DCI polarity/mapping.
- `pclk_edges/href_edges/sync_edges` are all nonzero: physical DVP timing exists.
- The selected DCI polarity is `HS blanking low + VS blanking high`, based on the hardware test where this combination produced varied captured data instead of repeated words.
- Full-frame QVGA capture is verified when `camera_capture[normal_jpeg_to_yuv_ref]` reports `dma=done words=38400 remain=0`.
- Dataset preprocessing is valid when `preprocess_gray96` reports `scale=raw` and the saved BMP is visually recognizable as the real scene.
- The current `infer_probe` line verifies only the firmware interface for later model replacement. Do not present it as the final AI model in contest materials.
- `camera_dvp_gpio` edge counts are all zero: H7 sees no physical `PCLK/HREF/SYNC`; check the three signal wires first, then camera stream clock/XCLK and `PA6` LED2 conflict.
- `camera_dvp_gpio` has edges but DCI still times out: DVP timing exists; debug DCI polarity, pin AF mapping, DMA request/channel, or data bus wiring.
- `dma=done`: start validating image data and then add OV5640 low-resolution streaming init.
- `dma=timeout` with no DCI flags: check `PCLK/HREF/SYNC`, missing camera clock, and the `PA6` LED2 conflict.
- `dma=timeout` with DCI flags: check DMA request/channel, D0-D7 wiring, polarity, and OV5640 output format.
