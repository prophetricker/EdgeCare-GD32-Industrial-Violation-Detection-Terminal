# EdgeCare Gray96 Data Collection

This workflow captures the exact `96x96` grayscale tensor currently used by the firmware model boundary.

For the full Chinese end-to-end field procedure, read:

```text
doc/EdgeCare_Dataset_Collection_Manual_CN.md
```

Current status on 2026-06-16: `OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00` can produce a recognizable real-scene grayscale image with full `0..255` raw `gray96` range. The previous temporary left-shift-by-2 compensation is now disabled. This is suitable for starting a small MVP dataset from `gray96`; color/YUV rendering is still not fixed.

Do not use old RAW8/noise-like samples as training data. For every new capture session, first preview one saved BMP and confirm that the fixed scene, danger-zone marker, and intrusion object/person are recognizable.

## Firmware Switch

Edit:

```text
EdgeCare_GD32_Industrial_Violation_Terminal/GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/board/board_config.h
```

Set only for capture firmware:

```c
#define EDGECARE_ENABLE_GRAY96_DUMP 1U
```

Keep these MVP defaults unchanged unless doing a focused camera experiment:

```c
#define EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF 1U
#define EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING 1U
#define EDGECARE_CAMERA_DATA_ORDER_DEFAULT 0x00U
#define EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION 0U
```

Build and burn with Keil or J-Link. Open the serial monitor at `COM8`, `115200 8N1`, then press RESET. The firmware dumps one boot-time `gray96` frame after camera capture and preprocessing.

Set `EDGECARE_ENABLE_GRAY96_DUMP` back to `0U` for normal demo firmware, because the UART dump is intentionally verbose.

During camera bring-up, this extra switch may also be enabled:

```c
#define EDGECARE_ENABLE_CAMERA_BYTE_PLANE_DUMP 1U
```

It dumps four downsampled raw byte planes from the same `320x240` frame. These files are diagnostics, not model samples.

## Live Capture

Install pyserial once if needed:

```powershell
py -m pip install pyserial
```

Capture current MVP model-input samples:

```powershell
cd "D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST"
py .\tools\collect_gray96_serial.py --label empty --count 20 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
py .\tools\collect_gray96_serial.py --label safe --count 20 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
py .\tools\collect_gray96_serial.py --label intrusion --count 20 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
```

Press RESET once for each sample. If the script times out, close VS Code Serial Monitor, Keil serial windows, PuTTY/MobaXterm, HLK tools, or any previous collector process that may own `COM8`, then retry.

The script saves samples under:

```text
data/raw_gray96/<label>/
```

Each sample has:

- `.pgm`: binary PGM grayscale image, `96x96`
- `.bmp`: 8-bit grayscale BMP image for quick Windows preview
- `.json`: label and capture metadata

The `data/` directory is intentionally ignored by Git.

## Labels

- `empty`: no person/object inside or near the marked danger zone.
- `safe`: person/object visible outside the danger zone, or normal background activity that should not alarm.
- `intrusion`: person/object crosses into the marked danger zone.

## Practical Capture Rules

- Keep camera position fixed and do not reframe during one dataset batch.
- Keep the danger-zone boundary visible or physically fixed.
- Start with a small balanced check set: `20` samples per label.
- Inspect at least the first 3 BMPs per label before collecting more.
- Expand toward `50-100+` samples per label only after the first check set is visually recognizable.
- Include lighting changes, background movement, partial entry, and near-boundary safe cases.
- Do not train from the current `stat_placeholder` confidence; train from the saved `gray96` image data.

## Camera Sanity Check

When `EDGECARE_ENABLE_GRAY96_DUMP_VARIANTS` is `1U`, the current JPEG-to-YUV firmware dumps two interpretations from the same raw YUV422 frame:

- `jpeg_to_yuv_ref_y02`: current MVP model input, bytes `0/2` are luminance from the fixed `0x4745=0x00` path
- `yuv422_y13`: alternate assumption, bytes `1/3` are luminance

Capture both from one reset:

```powershell
py .\tools\collect_gray96_serial.py --label empty --count 2 --variant any --kind gray96 --port COM8
```

If `yuv422_y13` looks like the real scene but `jpeg_to_yuv_ref_y02` does not, the camera is probably streaming but the luminance byte phase is wrong. If both look unrelated to the scene, keep debugging DVP wiring, data-bit order, PCLK sampling edge, and OV5640 timing/register setup before collecting more training data.

## Raw Byte-Plane Diagnostic

When both `jpeg_to_yuv_ref_y02` and `yuv422_y13` do not resemble the real scene, capture four raw byte planes:

```powershell
py .\tools\collect_gray96_serial.py --label empty --kind byte_plane --count 4 --variant any --port COM8 --max-wait-sec 60
```

Open the saved `.bmp` files under:

```text
data/camera_diagnostics/<label>/
```

Interpretation:

- If one byte plane has a recognizable scene, the DCI/DMA frame is likely real and the next fix is byte order or YUV output interpretation.
- If none of the four byte planes has a recognizable scene, stop dataset collection and continue camera bring-up: re-check `D0-D7` wiring/order, `PCLK` sampling edge, DCI packing, and OV5640 output registers.
- If all planes show only stripes, repeated blocks, or noise, suspect data-bit wiring/order or sampling edge before changing the AI model.

## Current Evidence

Latest verified reference evidence:

```text
.embeddedskills/logs/serial/edgecare_camera_order00_normal_20260616_112516.log
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_order00_frame_20260616_112553.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260616_112553_order00/raw_yuv422_yuyv_y_320x240.bmp
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260616_112553_order00/gray96_y02_96x96.bmp
```

Key status:

```text
camera_capture_probe: ... data_order_default=0x00 ... jpeg_yuv_order_capture_sweep=0
camera_dvp_regs: ... 4745=0x00 ...
camera_capture[normal_jpeg_to_yuv_ref]: data_order=0x00/read0x00 dma=done words=38400 remain=0
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=raw ... min=0 max=255 mean=66
```

Interpretation: real-scene grayscale capture is now usable for the first MVP dataset without the old `lshift2` workaround. Keep the dataset labeled as raw `gray96` from the `order00` camera firmware, and do not mix it with old `gray96_lshift2_mvp` samples.
