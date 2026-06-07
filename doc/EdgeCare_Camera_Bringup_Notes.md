# EdgeCare Camera Bring-Up Notes

Updated: 2026-06-07

## Current Goal

Day 3 target: make the GD32H759 capture one valid camera frame after LD2410 triggers. Do not integrate AI inference yet.

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

- The boot log now reads the verified OV5640 ID first.
- It then writes a minimal OV5640 QVGA/YUV stream configuration derived from the OV5640 software application notes section 13.1.1 and 13.1.2.
- The latest build also includes the key timing/subsampling/exposure-window registers from the official VGA preview table, because the first minimal table produced PCLK but no HREF/SYNC activity.
- Before enabling DCI/DMA, it samples `PCLK/HREF/SYNC` as plain GPIO inputs over 80 longer bursts and prints `camera_dvp_gpio:` edge and high-sample counts.
- The working DCI polarity found from hardware logs is `HS blanking low + VS blanking high`; this attempt produced non-constant captured data, while other successful combinations mostly produced repeated blanking-like words.
- The current build captures one full QVGA YUV422 frame: `320x240x2 = 153600 bytes = 38400 words`.
- Hardware logs have verified full-frame capture: `dma=done words=38400 nonzero=38400`, with varied `first/mid/last` samples.
- It then extracts the Y channel from YUYV and downsamples to a `96x96` grayscale model-input buffer.
- Hardware logs have verified that the `96x96` grayscale buffer responds to scene brightness: brighter scene `mean=105/max=195`, darker scene `mean=49/max=135`.
- It prints `camera_capture[working_hs_blank_low_vs_blank_high]`, `preprocess_gray96`, and `infer_probe` diagnostic lines before returning to periodic `state=...` logs.
- `infer_probe` currently uses `model=stat_placeholder`. It is only a replaceable interface proof, not the final intrusion classifier.
- If the DVP bus is not wired or the camera does not output pixel/sync clocks, the probe should report `dma=timeout` instead of blocking the alarm/radar loop.
- The probe currently uses `PCLK=PA6`. The local `readme.txt` for the START demo says LED2 is also connected to `PA6`, so if capture times out or produces unstable data, treat the `PA6` board LED load as a suspect and consider moving pixel clock to another valid exposed `DCI_PIXCLK` pin such as `PE3` if available on the header.

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
camera_init: ov5640 qvga yuv probe regs=... source=OV5640 app note 13.1.1/13.1.2
camera_init: readback 3008=0x02 size=320x240
camera_capture_probe: start qvga=320x240 bytes=153600 words=38400 timeout=60000000 working_pol=hs_blank_low_vs_blank_high
camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PE4 D5=PB6 D6=PE5 D7=PE6
camera_dvp_gpio: bursts=80 samples_per_burst=50000 pclk_edges=... href_edges=... sync_edges=... href_high=... sync_high=...
camera_capture[working_hs_blank_low_vs_blank_high]: dma=done|timeout words=38400 nonzero=... repeated=... checksum=... first=... mid=... last=...
preprocess_gray96: source=YUYV_Y qvga=320x240 out=96x96 bytes=9216 min=... max=... mean=... checksum=... center=...
infer_probe: model=stat_placeholder input=gray96 mean=... contrast=... conf=... intrusion=... note=replace_with_trained_model
```

Interpretation:

- `camera_init: readback 3008=0x02 size=320x240`: SCCB writes landed and the sensor is commanded to stream QVGA.
- `pclk_edges` is nonzero but `href_edges/sync_edges` remain zero: the camera has a pixel clock, but frame/line sync is not visible. Check HREF/SYNC wiring and active level first; if wiring is correct, try a fuller OV5640 reference init table or adjust DCI polarity.
- `dma=done words=38400 nonzero` close to `38400`: full-frame DMA capture is working. Next step is validating YUV byte order and deriving a grayscale/ROI buffer.
- `preprocess_gray96` has a reasonable `min/max` spread and its `checksum/center` changes when the scene changes: the model input buffer is usable for Day 4 data collection.
- `infer_probe` appearing after `preprocess_gray96`: model-call interface is wired. Treat its result as a placeholder until a trained model is integrated.
- `first/mid/last` all nearly constant or `repeated` near `38399`: DMA filled but likely captured blanking or a stuck data bus; revisit polarity, PCLK edge, and D0-D7 wiring.
- `camera_dvp_gpio: pclk_edges=0 href_edges=0 sync_edges=0`: H7 still sees no physical DVP activity. Check camera `PCLK/HREF/SYNC` wiring, whether the module needs an external `XCLK/MCLK`, `PWON/PWDN` active level, and the `PA6` LED2 conflict.
- `camera_dvp_gpio` has edges but `camera_capture: dma=timeout`: physical signals exist, so focus on DCI pin mapping, DCI polarity, DMA request/channel, or D0-D7 wiring.
- `dma=done` and nonzero/checksum changing: DCI/DMA is receiving camera data.
- `dma=timeout`, `hs/vs/fv/ef/vsif/elif` all `0`: likely no PCLK/HREF/VSYNC activity, wrong pins, missing camera clock, or camera not initialized for streaming.
- `dma=timeout` with sync flags changing: DCI sees camera timing but DMA/data path still needs pin, DMA request, or polarity debugging.
- `dmaerr=100`, `010`, or `001`: DMA transfer/access/synchronization/FIFO error flags are set; inspect DMA channel/request and buffer placement.

Do not start with LCD, AI model, or continuous inference. The first milestone is a repeatable single-frame capture.
