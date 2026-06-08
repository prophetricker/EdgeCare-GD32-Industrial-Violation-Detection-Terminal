# EdgeCare Gray96 Data Collection

This workflow captures the exact `96x96` grayscale tensor currently used by the firmware model boundary.

Do not use these files as training data until the camera image is visually valid. If a simple target, such as two bottles in front of the camera, does not leave a recognizable shape in the saved image, run the byte-plane diagnostic first.

## Firmware Switch

Edit:

```text
EdgeCare_GD32_Industrial_Violation_Terminal/GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/board/board_config.h
```

Set:

```c
#define EDGECARE_ENABLE_GRAY96_DUMP 1U
```

Build and burn with Keil. Open the serial monitor at `COM8`, `115200 8N1`, then press RESET. The firmware will dump one boot-time `gray96` frame after camera capture and preprocessing.

Set the switch back to `0U` for normal demo firmware, because the UART dump is intentionally verbose.

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

Capture one empty/safe sample:

```powershell
cd "D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST"
py .\tools\collect_gray96_serial.py --label empty --count 1 --port COM8
```

Capture one intrusion sample:

```powershell
py .\tools\collect_gray96_serial.py --label intrusion --count 1 --port COM8
```

The script saves samples under:

```text
data/raw_gray96/<label>/
```

Each sample has:

- `.pgm`: binary PGM grayscale image, `96x96`
- `.bmp`: 8-bit grayscale BMP image for quick Windows preview
- `.json`: label and capture metadata

The `data/` directory is intentionally ignored by Git.

## Practical Capture Rules

- Keep camera position fixed.
- Keep the dangerous-zone ROI scene stable during one capture.
- Collect balanced samples: start with at least 30 `empty/safe` and 30 `intrusion`, then expand toward 100+ each.
- Include lighting changes, background movement, and partial body entry samples.
- Do not train from the current `stat_placeholder` confidence; train from the saved `gray96` image data.

## Camera Sanity Check

When `EDGECARE_ENABLE_GRAY96_DUMP_VARIANTS` is `1U`, the firmware dumps two interpretations from the same raw YUV422 frame:

- `yuv422_y02`: current assumption, bytes `0/2` are luminance
- `yuv422_y13`: alternate assumption, bytes `1/3` are luminance

Capture both from one reset:

```powershell
py .\tools\collect_gray96_serial.py --label empty --count 2 --variant any --port COM8
```

If `yuv422_y13` looks like the real scene but `yuv422_y02` does not, the camera is probably streaming but the luminance byte phase is wrong. If both look unrelated to the scene, keep debugging the DVP wiring, data-bit order, PCLK sampling edge, and OV5640 timing/register setup before collecting more training data.

## Raw Byte-Plane Diagnostic

When both `yuv422_y02` and `yuv422_y13` do not resemble the real scene, capture four raw byte planes:

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
