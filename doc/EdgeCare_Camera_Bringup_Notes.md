# EdgeCare Camera Bring-Up Notes

Updated: 2026-06-13

## Current Goal

The first recognizable real-scene grayscale frame has been captured through the `OV5640_JPEG_TO_YUV_REF` path. A later single-variable DCI PCLK-edge test showed that keeping `0x4740=0x20` but sampling the GD32 DCI on the rising edge gives a visibly cleaner real-scene grayscale frame with much less horizontal striping. The current MVP input uses a temporary `gray96` left-shift-by-2 compensation to recover usable model-input contrast while the camera root cause remains under investigation.

Evidence frame:

```text
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_frame_20260613_170316_clean.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_170316_clean/raw_yuv422_yuyv_y_320x240_norm.bmp
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_170316_clean/raw_640x240_norm.bmp
```

Clean hardware log:

```text
.embeddedskills/logs/serial/edgecare_reset_capture_20260613_170316.log
```

Key line:

```text
camera_capture[normal_jpeg_to_yuv_ref]: data_order=0x02/read0x02 dma=done words=38400 remain=0 nonzero=38400 repeated=5376 checksum=0x89D5289F ... dmaerr=000
```

The image is recognizable but still diagnostic quality only. Do not treat it as final color photo success.

The current best JPEG-to-YUV evidence is the DCI rising-edge run. It keeps the OV5640 path at `0x4740=0x20`, `0x4745=0x02`, auto exposure (`3503=0x00`), and uses GD32 DCI `PCLK rising + HS blank low + VS blank high`. Evidence:

```text
.embeddedskills/logs/serial/edgecare_reset_capture_20260613_204732.log
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_rising_default_frame_20260613_204756.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_204756_rising_default/raw_yuv422_yuyv_y_320x240_norm.bmp
```

Key line:

```text
camera_capture_normal: source=OV5640_JPEG_TO_YUV_REF output_mux=0x00 timing=471d00_474020 dci=pclk_rising_hs_blank_low_vs_blank_high note=photo_proof_candidate
camera_capture[normal_jpeg_to_yuv_ref]: data_order=0x02/read0x02 dma=done words=38400 remain=0 nonzero=38400 repeated=11289 checksum=0x39928308 ... dmaerr=000
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 ... min=1 max=52 mean=20 checksum=0xC56EF0E2 ...
```

Do not use the numeric `score` alone to judge frame quality. The rising-edge run scored high and also looked better, but earlier `href_gate24` also scored high while looking worse.

The low dynamic range now has a strong byte-scale symptom: on the `baseline20_recheck` frame, Y phases 0/2 are only about `2..49` with mean `26`, while U/V phases 1/3 sit near `31`. Offline `lshift2` previews in `tools/decode_camera_frame.py` show that shifting every byte left by 2 moves Y to about `8..196` and U/V to about `124`, which is closer to normal 8-bit YUV ranges. This is a diagnostic hint, not a final camera fix.

Firmware-side MVP input compensation is now enabled by default:

```text
EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION=1
```

The compensation is applied only in the YUV422-to-`gray96` preprocessing path: values above `63` saturate to `255`, otherwise the byte is shifted left by 2. It is intentionally labeled in the probe log as `scale=lshift2`. The J-Link RAM frame is still the raw camera buffer and does not change due to this preprocessing compensation.

Evidence from the compensated hardware run:

```text
.embeddedskills/logs/serial/edgecare_reset_capture_20260613_221540.log
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_lshift2_frame_20260613_221540.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_221540_lshift2/
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_221540_lshift2/gray96_y02_96x96.bmp
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_221540_lshift2/gray96_y02_lshift2_96x96.bmp
```

Key lines and stats:

```text
camera_capture[normal_jpeg_to_yuv_ref]: dma=done words=38400 remain=0 checksum=0x8E5B0421 first=20091F0B,20091E07,20091F09,20091F09 ...
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=lshift2 ... min=8 max=192 mean=93 checksum=0x683CDABA ...
```

The saved RAM frame was verified against the serial `first=` words: the file begins with bytes `0B 1F 09 20 07 1E 09 20 ...`, matching the little-endian words in the capture log. Offline same-frame model-input comparison:

```text
gray96_y02_96x96:         min=2 max=48 mean=23
gray96_y02_lshift2_96x96: min=8 max=192 mean=93
```

Visual inspection of the 96x96 preview shows the compensated input is clearly more readable than raw `gray96`. Keep it as a temporary MVP model-input workaround. The root cause is still unresolved; continue with DVP data-bit mapping, output bit-width/byte-scale checks, and output-window/active-line phase work rather than treating this as a camera fix.

Latest read-only window diagnostics were added and run on the same JPEG-to-YUV baseline. Evidence:

```text
.embeddedskills/logs/serial/edgecare_reset_capture_20260613_225918.log
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_window_readback_frame_20260613_225918.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_225918_window_readback/
```

Key lines:

```text
camera_window_readback[normal_jpeg_to_yuv_ref]: crop=0,4-2623,1947 sensor_span=2624x1944 output=320x240 hts=1896 vts=984 offset=16,6 inc=0x31,0x31
camera_capture[normal_jpeg_to_yuv_ref]: dma=done words=38400 remain=0 checksum=0xE83E2040 first=1F071F07,20081F07,...
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=lshift2 ... min=8 max=192 mean=92 checksum=0xC63D5E43
```

Interpretation: `0x3800..0x3815` reads back as the expected full-sensor crop downsampled to QVGA, so a simply wrong output window is unlikely to be the current root cause. The latest saved RAM frame still has raw Y phases around `2..49` and U/V-like phases near `31`; `gray96_y02_lshift2_96x96.bmp` is usable for MVP data collection, but raw byte-scale remains wrong.

The focused bit-mode hypothesis `0x3034=0x18` was tested and rejected as a default fix. The run forced OV5640 SC PLL CONTROL0 from the app-note `0x1A` value to `0x18`, then read back `3034=0x18 3035=0x21 3036=0x46 3037=0x13`, but the saved frame still had Y phases only `2..44` and U/V near `31`. Evidence:

```text
.embeddedskills/logs/serial/edgecare_reset_capture_20260613_213104.log
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_force_8bit3034_frame_20260613_213104.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_213104_force_8bit3034/
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_210100_baseline20_recheck_lshift2/
```

Default firmware was rebuilt and flashed back to the baseline after this test. Confirmation log `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_213715.log` shows `pll=3034:0x1A 3035:0x21 3036:0x46 3037:0x13`, `3824=0x04`, `460b=0x37`, `460c=0x20`, `4740=0x20`, `4745=0x02`, DCI rising, and `dma=done`.

`0x4740=0x24` was tested as a focused `href_gate24` candidate and should not be the default baseline. It produced a complete DMA frame and still showed real-scene geometry, but visual inspection was worse than the `0x20` proof frame: lower useful dynamic range and heavier speckle/noise. Evidence:

```text
.embeddedskills/logs/serial/edgecare_reset_capture_20260613_173850.log
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_href_gate24_frame_20260613_173850.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_173850_href_gate24/raw_yuv422_yuyv_y_320x240_norm.bmp
```

Key line:

```text
camera_capture[normal_jpeg_to_yuv_ref]: data_order=0x02/read0x02 dma=done words=38400 remain=0 nonzero=38400 repeated=7299 checksum=0xE1980260 ... score=259
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 ... min=0 max=47 mean=30 checksum=0xF9039EFD ...
```

## Current Camera Module Identification

The uploaded photo shows an OV5640-style DVP parallel camera module. Visible silk/pins include:

```text
PWON PCLK D6 D4 D2 D0 SDA SCL GND
FLASH D7 D5 D3 D1 RES HREF SYNC 3V3
```

The board side silk looks like `LXB-OVX640`/`LXB-OV5640`; combined with the downloaded `FD5640-500W-V11` material, treat it as an OV5640-class 5MP DVP module.

Verified ID-read result:

```text
camera_id: ov5640_regs[0x300A,0x300B]=0x56 0x40
```

This confirms the module as OV5640 for the next DCI/DMA bring-up step.

## START Board Wiring Decision

The START schematic in `doc/hardware_manuals/GD32H759I_START_Demo_Suites/Docs/Schematic/GD32H759I-START-V1.5.pdf` confirms that the needed DCI signals are exposed. The EVAL example still cannot be copied 1:1:

- EVAL uses `PH4` for SCCB/I2C SCL, but `PH4` is not exposed on START.
- START exposes `PB10/PB11`, so use `PB10 / I2C1_SCL` and `PB11 / I2C1_SDA` for camera SCCB.
- EVAL uses `PC9` for DCI_D3, but START connects `PC9` to LED1. Prefer `PG11 / DCI_D3` instead.
- Keep `PA8` reserved for the alarm until the camera proves it needs an external `XCLK/MCLK`.

Current firmware status:

- Current source defaults to `EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF=1`, using the proven real-scene grayscale evidence path.
- The normal JPEG-to-YUV path is pinned to `0x4740=0x20` and logs `timing=471d00_474020`. The current default also sets `EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING=1`, so the normal capture log must say `dci=pclk_rising_hs_blank_low_vs_blank_high`.
- `EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_TUNE_SWEEP=0` is currently off. The focused sweep variants remain in source for future A/B checks, but the `href_gate24/0x24` candidate has already been saved, decoded, and rejected as the normal default.
- The focused manual-exposure attempt `3503=0x07`, exposure `0x0375`, gain `0x02A` was saved and rejected. It was darker and worse than auto exposure: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_202337.log`, frame `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_manual_exp0375_gain02a_frame_20260613_202337.bin`, decoded under `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_202337_manual_exp0375_gain02a/`. Keep `EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE=0`.
- The `0x4745` data-order raw-PCLK sweep was run and rejected as a primary fix. ISP colorbar raw-PCLK samples stayed constant per order (`0xFF`, `0x3F`, `0xCF`, `0xFC`, etc.), and real-scene orders had at most tiny transitions such as order `0x00` `min=0x46 max=0x4A changes=1`, order `0x02` `min=0x12 max=0x13 changes=1`, and order `0x04` `min=0x14 max=0x20 changes=1`. Evidence log: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_203147.log`. Keep `EDGECARE_ENABLE_CAMERA_DATA_ORDER_SOURCE_SWEEP=0`.
- Two focused VFIFO/PCLK candidates were tested on the normal JPEG-to-YUV path and rejected as default changes. `460B=0x35,460C=0x22,3824=0x04` evidence: log `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_205729.log`, frame `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_vfifo_ref_candidate_frame_20260613_205753.bin`, recheck log `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_210307.log`, decoded recheck `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_210331_vfifo_ref_candidate_recheck/`. `460B=0x37,460C=0x20,3824=0x08` evidence: log `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_210739.log`, frame `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_pclkdiv08_candidate_frame_20260613_210803.bin`, decoded `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_210803_pclkdiv08_candidate/`. Same-scene visual and summary metrics were effectively identical to `baseline20_recheck` (`data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_210100_baseline20_recheck/`), so keep `EDGECARE_CAMERA_JPEG_TO_YUV_VFIFO_REF_CANDIDATE=0` and `EDGECARE_CAMERA_JPEG_TO_YUV_PCLKDIV08_CANDIDATE=0`.
- The focused `0x3034=0x18` bit-mode candidate was tested and rejected as a default change. It did not restore raw Y/U/V byte ranges to normal 8-bit levels. Keep `EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP=0`; the switch remains in source only for repeatable A/B testing.
- The EEWorld link `https://bbs.eeworld.com.cn/thread-1029272-1-1.html#pid2779794` could not be read in this environment: direct HTTP returned only the site WAF script page, and a real Edge/Playwright browser was blocked by `EEWORLD WAF`. Do not cite it as a source until the actual post text is available.
- Historical source-comparison builds used `camera_pattern_source_sweep[...]` with `EDGECARE_ENABLE_CAMERA_RAW_PCLK_SAMPLE=1` and `EDGECARE_ENABLE_CAMERA_PATTERN_SOURCE_SWEEP=1`, then ran RGB565 experiments. Those are no longer the default path for the current proof build.
- The source sweep now resets each candidate to `501F=0x00`, `471D=0x00`, and `4740=0x20`, then compares only `ISP colorbar` (`503D=0x80, 4741=0x00`), `DVP pattern` (`503D=0x00, 4741=0x05`), and `real scene` (`503D=0x00, 4741=0x00`). This prevents a previous RAW/RGB path from polluting the source comparison.
- The current normal RGB565 attempt applies the ST DVP overlay, then sets `4300=0x6F`, `501F=0x01`, `503D=0x00`, `4741=0x00`, `471D=0x00`, and `4740=0x20`, captured with GD32 DCI `PCLK falling + HS blank low + VS blank high`.
- The older clean RAW8 transport probe remains historical evidence: `OV5640_SNR_RAW8`, `0x501F=0x04`, `0x503D=0x00`, `0x4741=0x00`, `0x471D=0x00`, `0x4740=0x22`, with GD32 DCI `PCLK rising + HS blank high + VS blank high`.
- Latest clean transport log: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_012250.log`.
- Transport validation line: `camera_capture[normal_snr_raw8]: data_order=0x02/read0x02 dma=done words=38400 remain=0 nonzero=38400 ... quality=phase0,... score=513 ... dmaerr=000`.
- Important limitation: the saved real-scene RAW8 frame `data/photo_evidence_real_raw8/edgecare_real_raw8_frame_20260613_101207.bin`, decoded under `data/photo_evidence_real_raw8/decoded_20260613_101207/`, is noise-like. This proves data movement, not recognizable imaging.
- Internal OV5640 DVP pattern evidence exists at `data/photo_evidence_dvp_pattern/edgecare_dvp_pattern_frame_20260613_093617.bin` and decodes as regular vertical bars. This proves structured DVP/DCI/DMA capture, but it is not a real scene.
- The ISP/YUV and RGB565 photo-sanity attempts completed DMA but produced constant frames such as `0x02020202`/`0x05050505`. Keep ISP/YUV, RGB565, and RAW8 real-scene output as unresolved camera bring-up work before any MVP dataset collection.
- The `OV5640_JPEG_TO_YUV_REF` path is the first exception to the "no recognizable real-scene image" conclusion: it produces a real-scene grayscale frame with visible object boundaries and markings. The unresolved parts are striping, brightness, and color correctness.
- The boot log now reads the verified OV5640 ID first.
- The latest build defaults to `EDGECARE_ENABLE_OV5640_FULL_REFERENCE_INIT=0` plus `EDGECARE_ENABLE_OV5640_ST_DVP_REFERENCE_PATH=1`. The full OV5640 application-note VGA/QVGA table remains available behind `EDGECARE_ENABLE_OV5640_FULL_REFERENCE_INIT=1` for A/B comparison.
- The older minimal QVGA probe table is still present behind `EDGECARE_ENABLE_OV5640_FULL_REFERENCE_INIT=0` for A/B comparison.
- Current source does not globally enable `EDGECARE_ENABLE_CAMERA_TEST_PATTERN`; the source sweep enables patterns only for its own candidates, then the normal RGB565 attempt returns to real-scene registers.
- The next technical hypothesis should move away from exposure, `0x4745` data-order, `0x3034=0x18`, and the already tested `0x460B/0x460C/0x3824` single-variable candidates. Remaining suspects are output window/line alignment (`0x3800..0x3815` crop/offset/subsample), active-line phase, DVP data-bit mapping, and YUV/color interpretation.
- Before enabling DCI/DMA, it samples `PCLK/HREF/SYNC` as plain GPIO inputs over 80 longer bursts and prints `camera_dvp_gpio:` edge and high-sample counts.
- The older optional `camera_sync_sweep[...]` diagnostic uses OV5640 datasheet registers `0x3017`, `0x301D`, and `0x301A` to temporarily force the sensor HREF/VSYNC pads high/low. It is currently disabled by `EDGECARE_ENABLE_CAMERA_SYNC_TIMING_DIAGS=0` because previous logs already proved the standard HREF/VSYNC wiring.
- The older optional `camera_timing_sweep[...]` diagnostic sweeps OV5640 DVP timing/sync registers `0x471B`, `0x471D`, `0x4730`, and `0x4740`; candidates are kept only if they produce at least `100` HREF edges, so weak two-edge polarity changes are reported as `apply=0`. It is currently disabled with the sync sweep.
- `camera_sync_sweep[...]` now restores `0x3051` after forced-output testing, preventing the sync diagnostic from polluting the following timing sweep or DCI capture.
- The diagnostic build now also runs `camera_dvp_mode_sweep[...]` after the sync/timing sweep. It sweeps only the OV5640 DVP/JPEG/VFIFO output-mode controls `0x300E`, `0x302E`, and `0x4713`, then restores them unless a candidate produces at least `100` HREF edges and clearly improves over baseline.
- The latest user log showed `camera_dvp_mode_sweep_best: tag=none apply=0`, so the `0x300E/0x302E/0x4713` output-mode path did not restore natural HREF/VSYNC.
- The optional `camera_data_pad_sweep[...]` diagnostic is currently disabled. It can be re-enabled if the next source comparison loses the known-good internal DVP pattern. It temporarily forces OV5640 data pads through `0x3017`, `0x3018`, `0x301A`, `0x301B`, `0x301D`, and `0x301E`, then reads the MCU-side physical `D0-D7` GPIO bus. The `0x301E` write is required by the OV5640 datasheet for D[5:0] output select; an earlier diagnostic only forced D[9:6]. It also prints `camera_data_pad_summary[...]` masks so the bit-walk can be interpreted from one line.
- Latest J-Link auto-capture on 2026-06-11 is `.embeddedskills/logs/serial/edgecare_reset_capture_20260611_005211.log`. With the corrected `0x301E` path, `read_301e=0xFC` confirms D[5:0] are in register-controlled output mode. In the `pad_d9_d2` summary, `strong_mask=0xED`, `missing_mask=0x12`, `extra_mask=0x02`, `zero_float_mask=0x10`, and `ff_missing_mask=0x10`. This means D0, D2, D3, D5, D6, and D7 respond, but D0 also raises D1, D1 is not independently driven, and D4 is weak-high/floating or not connected to the driven camera pad.
- Earlier `camera_capture_mode_sweep[...]` logs showed both snapshot and continuous DCI modes could be diagnosed; continuous capture was not the missing switch.
- The current lightweight evidence build prints `camera_raw_pclk_sample:` before DCI/DMA capture, then prints tagged raw samples for each source-sweep candidate. This samples physical `D0-D7` as GPIO on PCLK edges while HREF is high, so the next log can tell whether ISP color bar, internal DVP pattern, and real scene differ before full-frame DMA.
- `EDGECARE_ENABLE_CAMERA_SYNC_TIMING_DIAGS` can disable the older forced-output and timing sweeps for one A/B capture attempt without removing the newer DVP mode sweep.
- The working DCI polarity found from hardware logs is `HS blanking low + VS blanking high`; this attempt produced non-constant captured data, while other successful combinations mostly produced repeated blanking-like words.
- The current diagnostic build captures one full QVGA 8-bit DVP frame buffer: `320x240x2 = 153600 bytes = 38400 words`.
- Hardware logs have verified full-frame capture: `dma=done words=38400 nonzero=38400`, with varied `first/mid/last` samples.
- It can still exercise the `96x96` grayscale model-input buffer for interface testing.
- Do not treat current `gray96` dumps as training samples unless a matching full-frame or preview image is visually recognizable as the real scene.
- It prints `camera_dvp_regs[...]`, `camera_capture[...]`, `camera_capture_best`, `preprocess_gray96`, `preprocess_byte_plane`, and `infer_probe` diagnostic lines before returning to periodic `state=...` logs.
- It also prints `camera_dvp_ext_regs[...]` after each `camera_dvp_regs[...]` line. This extended line reads OV5640 DVP clock/pad/control registers `0x3007/0x3019/0x301B/0x302E/0x4709/0x470A/0x470B/0x471C/0x471F`; the regular `camera_dvp_regs[...]` line now also includes `0x4713`.
- `infer_probe` currently uses `model=stat_placeholder`. It is only a replaceable interface proof, not the final intrusion classifier.
- If the DVP bus is not wired or the camera does not output pixel/sync clocks, the probe should report `dma=timeout` instead of blocking the alarm/radar loop.
- The probe currently uses `PCLK=PA6`. START board LED documentation is version-dependent: the local copied utility header uses `PF10/PA6`, while the downloaded START V1.4 demo says LEDs are `PC9-PC12`. Treat `PA6` LED loading as a version-related risk, but if `pclk_edges` is stable it is not the first suspect.

## Reference Example

Extracted source:

```text
doc/extracted_camera_examples/DCI_OV2640_gesture_release/
```

Original archive:

```text
doc/GD32EmbeddedAI_V1.2.0.39432/AI_Demo/GD32H759I-EVAL-Camera-Object-Dection/DCI_OV2640_gesture_release.rar
```

Most relevant files:

| File | Use |
| --- | --- |
| `Soft_Drive/dci_ov2640.c` | DCI GPIO, DCI peripheral, DMA, OV2640 init |
| `Soft_Drive/dci_ov2640.h` | OV2640 ID and init APIs |
| `Soft_Drive/dci_ov2640_init_table.h` | OV2640 register table |
| `Soft_Drive/sccb.c` | SCCB/I2C read/write |
| `Soft_Drive/sccb.h` | SCCB/I2C pin and address macros |
| `Image_process/image_process.h` | Example frame size and buffer addresses |
| `main.c` | Example call order |

## Reference Pin Map From GD32H759I-EVAL Example

These pins come from the EVAL example. They are not automatically valid for the current `GD32H759I-START` wiring.

| Camera Signal | GD32 Pin In Example | Notes |
| --- | --- | --- |
| `XCLK/MCLK` | `PA8 / CKOUT0` | Conflicts with current alarm `PA8` |
| `PCLK` | `PE3 / DCI_PIXCLK` | AF13 |
| `VSYNC` | `PB7 / DCI_VSYNC` | AF13 |
| `HSYNC` | `PA4 / DCI_HSYNC` | AF13 |
| `D0` | `PC6` | AF13 |
| `D1` | `PH10` | AF13 |
| `D2` | `PC8` | AF13 |
| `D3` | `PC9` | AF13 |
| `D4` | `PE4` | AF13 |
| `D5` | `PB6` | AF13 |
| `D6` | `PE5` | AF13 |
| `D7` | `PE6` | AF13 |
| `SDA` | `PB11 / I2C1_SDA` | SCCB/I2C, AF4 |
| `SCL` | `PH4 / I2C1_SCL` | SCCB/I2C, AF4 |

## Reference Capture Settings

From `dci_ov2640.c` and `image_process.h`:

- DCI mode: continuous capture.
- DCI interface: 8-bit.
- Pixel clock: rising edge.
- HSYNC/VSYNC polarity: low.
- DMA: `DMA1`, channel `DMA_CH7`, request `DMA_REQUEST_DCI`.
- DMA memory address in example: `0xC0000000`.
- Example source image: `256x256`.
- Example output format: RGB565 in frame buffer, converted to RGB888 for preprocessing.
- Example resized input: `192x192x3`.

## Immediate Risk

The reference example outputs camera clock on `PA8/CKOUT0`. Current EdgeCare firmware uses `PA8` for the active-low alarm lamp.

Before camera bring-up, choose one:

1. Move alarm `IN` from `PA8` to another free GPIO.
2. Use a camera module with its own clock and do not use `PA8/CKOUT0`.
3. Keep the alarm disconnected during camera-only bring-up.

Option 1 is preferred if the camera needs `XCLK/MCLK`.

## Day 3 Minimal Bring-Up Plan

1. Connect only camera `3V3`, `GND`, `PB10/SCL`, `PB11/SDA`, `RES`, and `PWON` first.
2. Keep the DVP data bus disconnected until SCCB ID read works.
3. Use `PD0` for `RES` and `PD1` for `PWON/PWDN` unless those pins are unavailable on the actual wiring.
4. First firmware test: read camera ID over SCCB/I2C and print it over USART0.
5. If SCCB ID is `OV5640`, switch from the OV2640 register table to an OV5640 init table.
6. Resolve `PA8` conflict if the module actually needs H7-generated `XCLK/MCLK`.
7. Second firmware test: enable DCI/DMA and capture one low-resolution frame.
8. Verification fallback: print a small checksum or a few sampled pixels from the frame buffer.

Expected firmware output for the Day 3 DVP test:

```text
camera_init: ov5640 qvga yuv probe regs=... source=OV5640 app note 13.1.1/13.1.2 camera_init_path=full_reference_vga_then_qvga
camera_init: readback 3008=0x02 size=320x240 polarity_4740=... test_503d=... test_4741=...
camera_dvp_regs[after_init]: ...
camera_capture_probe: start qvga=320x240 bytes=153600 words=38400 timeout=60000000 test_pattern=1 mode=1 ... data_order_sweep=1
camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PE4 D5=PB6 D6=PE5 D7=PE6
camera_dvp_gpio: bursts=80 samples_per_burst=50000 pclk_edges=... href_edges=... sync_edges=... href_high=... sync_high=...
camera_sync_sweep: enabled source=OV5640_datasheet_3017_301d_301a ...
camera_sync_sweep[force_h1_v0]: forced_href=1 forced_vsync=0 ...
camera_timing_sweep: enabled source=OV5640_datasheet_471b_471d_4730_4740 ...
camera_timing_sweep[baseline]: 471b=0x.. 471d=0x.. 4730=0x.. 4740=0x.. pclk_edges=... href_edges=... sync_edges=... score=...
camera_timing_sweep_best: tag=... apply=0|1 baseline_score=... best_score=...
camera_dvp_mode_sweep: enabled source=OV5640_300e_302e_4713 ...
camera_dvp_mode_sweep[baseline]: 300e=0x.. 302e=0x.. 4713=0x.. pclk_edges=... href_edges=... sync_edges=... score=...
camera_dvp_mode_sweep[...]: 300e=0x../read0x.. 302e=0x../read0x.. 4713=0x../read0x.. pclk_edges=... href_edges=... sync_edges=... score=...
camera_dvp_mode_sweep_best: tag=... apply=0|1 baseline_score=... best_score=...
camera_dvp_regs[after_dvp_mode_sweep]: ...
camera_dvp_ext_regs[after_dvp_mode_sweep]: ...
camera_data_pad_sweep: enabled source=OV5640_datasheet_3017_3018_301a_301b_301d_301e ...
camera_data_pad_sweep[pad_d7_d0_..]: map=pad_d7_d0 expected=0x.. read=0x.. match=0|1 strong_match=0|1 ...
camera_data_pad_sweep[pad_d9_d2_..]: map=pad_d9_d2 expected=0x.. read=0x.. match=0|1 strong_match=0|1 ...
camera_data_pad_summary[pad_d9_d2]: bit_tests=8 full_matches=... strong_matches=... present_mask=0x.. strong_mask=0x.. missing_mask=0x.. extra_mask=0x.. zero_float_mask=0x.. ff_missing_mask=0x..
camera_data_pad_sweep: restored ...
camera_raw_pclk_sample: enabled max_edges=4096 ...
camera_capture[pclk_...]: data_order=0x.. dma=done|timeout words=38400 remain=... nonzero=... quality=...
camera_capture_best: tag=... data_order=0x.. phase=... row_range=... col_range=... neighbor_delta=... score=... recapture=1
camera_capture[selected_best_for_dump]: data_order=0x.. dma=done|timeout words=38400 ...
preprocess_gray96: source=YUYV_Y qvga=320x240 out=96x96 bytes=9216 min=... max=... mean=... checksum=... center=...
preprocess_byte_plane: source=raw_qvga phase=...
infer_probe: model=stat_placeholder input=gray96 mean=... contrast=... conf=... intrusion=... note=replace_with_trained_model
```

Interpretation:

- `camera_init: readback 3008=0x02 size=320x240`: SCCB writes landed and the sensor is commanded to stream QVGA.
- `camera_dvp_regs` should show `300e` with DVP enabled, `3017/3018` output pads enabled, `503d=0x80` in ISP color-bar mode, `4741=0x00`, and `4745` matching the active data-order attempt.
- During `camera_sync_sweep`, restore the standard test wiring first: camera `PCLK -> PA6`, module `HERF/HREF -> PA4`, and module `SYNC -> PB7`. Do not move `PA4` between candidate pins for this test.
- `camera_sync_sweep[force_h1_v0]` should make the real HREF pad read high on `href=1` and `href_high` near the total sample count if module `HERF/HREF -> PA4` is really connected to HREF.
- `camera_sync_sweep[force_h0_v1]` should make the real VSYNC pad read high on `sync=1` and `sync_high` near the total sample count if module `SYNC -> PB7` is really connected to VSYNC.
- The latest user log has already confirmed the standard wiring through forced output: `force_h1_v0` drives HREF on `PA4`, and `force_h0_v1` drives VSYNC on `PB7`. Do not continue random pin probing unless this forced-output proof later fails.
- `camera_timing_sweep_best: apply=1` means one OV5640 timing register combination produced clearly better sync activity and has been kept for the following DMA capture attempt. Paste that line plus the first `camera_capture[...]` line.
- `camera_timing_sweep_best: apply=0` means no timing candidate improved enough; the firmware restored the saved timing registers before the DVP mode sweep.
- `camera_dvp_mode_sweep_best: apply=1` means one `0x300E/0x302E/0x4713` candidate produced real HREF activity and has been kept for the following DMA capture attempt. Paste that line plus the first `camera_capture[...]` line.
- `camera_dvp_mode_sweep_best: apply=0` means the firmware restored the saved DVP mode controls. If capture still times out, the next step should be a DCI-side sync-window strategy, an external clock/module-state check, or a known-good reference module comparison, not random wire probing.
- `camera_dvp_ext_regs[...]` helps distinguish sensor-internal DVP clock/output state from board wiring. In particular, compare `0x3007`, `0x3019`, `0x301B`, `0x302E`, `0x4713`, and `0x4709..0x471F` across `after_init`, `before_capture`, and `after_dvp_mode_sweep`.
- `camera_data_pad_sweep[...] match=1` on all four patterns for either mapping means the OV5640 data pads can drive the MCU-side D0-D7 wires. If raw PCLK data still stays constant afterward, the problem is likely sensor streaming/test-pattern/data-window state rather than broken data wires.
- `camera_data_pad_sweep[...]` failing for one or more bit patterns means focus on the D0-D7 physical data path, module data-pad mapping, or OV5640 pad select/output-enable behavior before spending more time on DCI polarity.
- `camera_data_pad_summary[pad_d9_d2] strong_mask` is the most useful compact field for the current wiring because `0x4745=0x02`; a missing bit in `missing_mask`, coupled bit in `extra_mask`, or weak bit in `zero_float_mask`/`ff_missing_mask` should be checked on the physical data bus first.
- `camera_raw_pclk_sample` showing `href_gated>0`, `nonzero>0`, and `changes>0` means the data bus carries changing bytes under natural PCLK; then the next step is DCI frame-sync strategy rather than more pad probing.
- `camera_raw_pclk_sample` showing `href_gated>0` but `changes=0` means HREF/PCLK are present but the sampled byte is stuck; compare it with `camera_data_pad_sweep[...]` to separate a stuck physical bus from a sensor-output/test-pattern issue.
- If forced HREF changes `sync` instead of `href`, the module silk or wiring is swapped. If forced HREF/VSYNC changes neither `href` nor `sync`, the module header pins may not be routed as expected or the physical contact is still wrong.
- `pclk_edges` is nonzero but `href_edges/sync_edges` remain zero: the camera has a pixel clock, but frame/line sync is not visible. Check HREF/SYNC wiring and active level first; if wiring is correct, try a fuller OV5640 reference init table or adjust DCI polarity.
- `dma=done words=38400 nonzero` close to `38400`: full-frame DMA capture is working. Next step is validating YUV byte order and deriving a grayscale/ROI buffer.
- `dma=done` while `ef=0` or `remain=0` but the color bar still appears as horizontal scanlines: DMA filled, but the selected data-order/sampling window is probably not producing valid active pixels.
- One `data_order` value producing a clearly more regular color bar than the others means the next normal-capture build should freeze that `0x4745` value.
- `neighbor_delta` and `col_range` are rough color-bar quality hints. A color-bar-like image should have meaningful horizontal-neighbor and column variation; a mostly horizontal-line image usually has weak or incoherent column structure.
- `preprocess_gray96` having a reasonable `min/max` spread only proves that bytes vary. It is usable for Day 4 data collection only after the saved image is visually recognizable as the real scene.
- `infer_probe` appearing after `preprocess_gray96`: model-call interface is wired. Treat its result as a placeholder until a trained model is integrated.
- `first/mid/last` all nearly constant or `repeated` near `38399`: DMA filled but likely captured blanking or a stuck data bus; revisit polarity, PCLK edge, and D0-D7 wiring.
- `camera_dvp_gpio: pclk_edges=0 href_edges=0 sync_edges=0`: H7 still sees no physical DVP activity. Check camera `PCLK/HREF/SYNC` wiring, whether the module needs an external `XCLK/MCLK`, `PWON/PWDN` active level, and the `PA6` LED2 conflict.
- `camera_dvp_gpio` has edges but `camera_capture: dma=timeout`: physical signals exist, so focus on DCI pin mapping, DCI polarity, DMA request/channel, or D0-D7 wiring.
- `dma=done` and nonzero/checksum changing: DCI/DMA is receiving camera data.
- `dma=timeout`, `hs/vs/fv/ef/vsif/elif` all `0`: likely no PCLK/HREF/VSYNC activity, wrong pins, missing camera clock, or camera not initialized for streaming.
- `dma=timeout` with sync flags changing: DCI sees camera timing but DMA/data path still needs pin, DMA request, or polarity debugging.
- `dmaerr=100`, `010`, or `001`: DMA transfer/access/synchronization/FIFO error flags are set; inspect DMA channel/request and buffer placement.

Do not start with LCD, AI model, or continuous inference. The first milestone is a repeatable single-frame capture.
