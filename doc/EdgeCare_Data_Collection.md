# EdgeCare Gray96 Data Collection

This workflow captures the exact `96x96` grayscale tensor currently used by the firmware model boundary.

Current status on 2026-06-13: `OV5640_JPEG_TO_YUV_REF + DCI rising` can produce a recognizable real-scene grayscale image. Raw Y is still compressed to roughly `2..49`, so the firmware currently uses temporary `gray96` left-shift-by-2 compensation. This is acceptable for a small MVP dataset, but it is not a camera root-cause fix.

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
#define EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION 1U
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

- `jpeg_to_yuv_ref_y02`: current MVP model input, bytes `0/2` are luminance plus firmware `lshift2` compensation
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
.embeddedskills/logs/serial/edgecare_reset_capture_20260613_225918.log
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_window_readback_frame_20260613_225918.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_225918_window_readback/raw_yuv422_yuyv_y_320x240_norm.bmp
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_225918_window_readback/gray96_y02_lshift2_96x96.bmp
```

Key status:

```text
camera_window_readback: output=320x240 inc=0x31,0x31
camera_capture[normal_jpeg_to_yuv_ref]: dma=done words=38400 remain=0
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=lshift2 ... min=8 max=192 mean=92
```

Interpretation: real-scene grayscale capture is usable enough to start a small MVP dataset. The root cause of the low raw byte range is still open, so keep this dataset labeled as `gray96_lshift2_mvp`.
