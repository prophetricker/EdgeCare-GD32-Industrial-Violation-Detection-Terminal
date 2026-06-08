# EdgeCare Hardware Pinout

Updated: 2026-06-08

This file records the current wiring for the initial-round EdgeCare demo on `GD32H759I-START`.

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

## Pending Wiring

| Module | Signal | GD32 Pin | Status | Notes |
| --- | --- | --- | --- | --- |
| LXB-OVX640 camera | DCMI data/clock/sync | TBD | pending | Photo shows OV5640-style DVP parallel camera module |
| LXB-OVX640 camera | SCCB/I2C | TBD | pending | Photo shows `SDA/SCL`; likely OV5640 SCCB/I2C |
| LXB-OVX640 camera | reset/powerdown | TBD | pending | Photo shows `RES/PWON` |
| LXB-OVX640 camera | XCLK/MCLK | not exposed in visible silk | pending | Module may have onboard clock; no visible `XCLK` pin in photo |
| GD32VW553 | UART TX/RX | TBD | pending | Later for structured alarm upload |
| OLED SSD1306 | I2C/SPI | TBD | optional | Later status display |

## Next Wiring Task

Freeze the GD32-side camera wiring, then add a minimal camera bring-up path:

1. Camera module identified from photo as `LXB-OVX640`, likely OV5640 DVP parallel module.
2. Confirm `GD32H759I-START` header positions for DCI pins.
3. Map camera `D0-D7`, `PCLK`, `HREF`, `SYNC`, `SCL`, `SDA`, `RES`, and `PWON` to H7 GPIO.
4. Confirm whether this module needs external `XCLK/MCLK`; no visible `XCLK` silk was found in the photo.
5. Add the selected GD32 pin mapping to this file before writing the camera driver.

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

Current firmware DCI/DMA diagnostic build uses the table below and prints it at boot. After flashing, Serial Monitor should show one `camera_capture:` line before the periodic `state=...` logs. If that line is missing, the flashed firmware is older than the current workspace build.

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
| `D4` | `PE4 / DCI_D4` | `JP8-3` | Data bit 4 |
| `D5` | `PB6 / DCI_D5` | `JP11-24` | Data bit 5 |
| `D6` | `PE5 / DCI_D6` | `JP8-4` | Data bit 6 |
| `D7` | `PE6 / DCI_D7` | `JP8-5` | Data bit 7 |
| `SCL` | `PB10 / I2C1_SCL` | `JP9-25` | Use this instead of EVAL's `PH4`, which is not exposed |
| `SDA` | `PB11 / I2C1_SDA` | `JP9-26` | SCCB/I2C data |
| `RES` | any free GPIO, suggested `PD0` | `JP11-6` | Camera reset output from H7 |
| `PWON` / `PWDN` | any free GPIO, suggested `PD1` | `JP11-7` | Active level must be verified by ID-read test |
| `3V3` | `3V3` | power header | Do not power from `5V` |
| `GND` | `GND` | power header | Common ground |

### Avoided Pins

| Pin | Reason |
| --- | --- |
| `PC9` | Connected to START board `LED1`; it is also a DCI_D3 candidate, but using it may load or disturb the camera data line |
| `PA8` | Current alarm output and possible CK_OUT0 camera clock conflict; the camera photo does not expose `XCLK`, so do not use it unless ID/capture fails due missing clock |
| `PH4` | Used by the EVAL camera example for I2C SCL, but not exposed on the START board headers |

Immediate rule: wire only power, `PB10/PB11`, `RES`, and `PWON` first for the SCCB ID test. Connect the full DCI bus only after the ID read succeeds.

## Current DCI Capture Diagnostic

Current firmware path:

```text
main.c -> edgecare_camera_id_probe() -> edgecare_camera_capture_probe() -> periodic edgecare_step()
```

Current diagnostic mode:

- OV5640 ISP color bar is enabled by default with `0x503D=0x80`; the saved images should not show the real scene until this test pattern is disabled.
- DVP `0x4745 DATA ORDER` is swept through `0x02/0x00/0x01/0x03/0x04/0x05/0x06/0x07` to cover data-order and bit-reversal options.
- DCI samples both PCLK falling and rising edges for the known best HS/VS polarity pair: `HS blanking low + VS blanking high`.
- The current goal is to find one configuration whose byte-plane dump looks like a stable color-bar pattern. Only after that should real-scene capture be tested.

Expected camera probe output:

```text
camera_init: readback 3008=0x02 size=320x240 polarity_4740=0x20 test_503d=0x80 test_4741=0x00 ...
camera_capture_probe: start qvga=320x240 bytes=153600 words=38400 timeout=60000000 test_pattern=1 mode=1 ...
camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PE4 D5=PB6 D6=PE5 D7=PE6
camera_dvp_gpio: bursts=80 samples_per_burst=50000 pclk_edges=... href_edges=... sync_edges=... href_high=... sync_high=...
camera_capture[pclk_...]: data_order=0x.. dma=done|timeout words=38400 ... quality=phase...,row_range=...,col_range=...,neighbor_delta=...,score=...
camera_capture_best: tag=... data_order=0x.. phase=... row_range=... col_range=... neighbor_delta=... score=... recapture=1
camera_capture[selected_best_for_dump]: data_order=0x.. dma=done|timeout words=38400 ...
preprocess_gray96: source=YUYV_Y qvga=320x240 out=96x96 bytes=9216 ...
preprocess_byte_plane: source=raw_qvga phase=...
infer_probe: model=stat_placeholder input=gray96 mean=... contrast=... conf=... intrusion=...
```

For the next hardware test, burn the current Keil build, reset the board while Serial Monitor is open, and copy the whole boot section through the first few `state=...` lines. The `camera_capture:` line decides the next action:

- `camera_init: write_failed`: SCCB write path failed even though ID read may work; check I2C bus stability and reset/power sequencing.
- `pclk_edges` is nonzero but `href_edges/sync_edges` remain zero: the sensor is outputting pixel clock but H7 is not seeing line/frame sync; check camera `HREF` and `SYNC` wires, then DCI polarity/mapping.
- `pclk_edges/href_edges/sync_edges` are all nonzero: physical DVP timing exists.
- The selected DCI polarity is `HS blanking low + VS blanking high`, based on the hardware test where this combination produced varied captured data instead of repeated words.
- Full-frame QVGA capture is verified when `camera_capture[working_hs_blank_low_vs_blank_high]` reports `dma=done words=38400 nonzero=38400`.
- In the current diagnostic build, prefer the `camera_capture_best` line and the dumped `byte_plane*.bmp` files over the old fixed `working_hs_blank_low_vs_blank_high` tag.
- A valid color-bar diagnostic should show strong, regular vertical/column structure in at least one byte plane. Pure horizontal scanlines still mean the image path is not visually validated.
- Day 4 preprocessing is verified when `preprocess_gray96` reports nontrivial `min/max/mean/checksum`, and those values change with scene movement or occlusion.
- The current `infer_probe` line verifies only the firmware interface for later model replacement. Do not present it as the final AI model in contest materials.
- `camera_dvp_gpio` edge counts are all zero: H7 sees no physical `PCLK/HREF/SYNC`; check the three signal wires first, then camera stream clock/XCLK and `PA6` LED2 conflict.
- `camera_dvp_gpio` has edges but DCI still times out: DVP timing exists; debug DCI polarity, pin AF mapping, DMA request/channel, or data bus wiring.
- `dma=done`: start validating image data and then add OV5640 low-resolution streaming init.
- `dma=timeout` with no DCI flags: check `PCLK/HREF/SYNC`, missing camera clock, and the `PA6` LED2 conflict.
- `dma=timeout` with DCI flags: check DMA request/channel, D0-D7 wiring, polarity, and OV5640 output format.
