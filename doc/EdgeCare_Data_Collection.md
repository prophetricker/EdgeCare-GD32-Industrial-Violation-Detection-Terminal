# EdgeCare Gray96 Data Collection

This workflow captures the exact `96x96` grayscale tensor currently used by the firmware model boundary.

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
- `.json`: label and capture metadata

The `data/` directory is intentionally ignored by Git.

## Practical Capture Rules

- Keep camera position fixed.
- Keep the dangerous-zone ROI scene stable during one capture.
- Collect balanced samples: start with at least 30 `empty/safe` and 30 `intrusion`, then expand toward 100+ each.
- Include lighting changes, background movement, and partial body entry samples.
- Do not train from the current `stat_placeholder` confidence; train from the saved `gray96` image data.
