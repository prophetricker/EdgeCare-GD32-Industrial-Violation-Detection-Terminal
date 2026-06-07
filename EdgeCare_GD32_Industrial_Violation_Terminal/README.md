# EdgeCare GD32 Industrial Violation Detection Terminal

This Keil MDK project is the first hardware bring-up project for the GD32 edge-computing contest topic:

```text
EdgeCare VisionGuard Industrial Violation Detection Terminal
```

The current firmware is a Day 3 bring-up build. It verifies that the GD32H759 Keil project can build, USART0 can print EdgeCare status logs, the active-low alarm lamp can be controlled, an LD2410 digital OUT signal can trigger the alarm path, and the OV5640-class camera SCCB/I2C bus can be probed.

## Wiring

### Alarm Lamp

- Alarm `VCC` -> board power pin that matches the alarm module label.
- Alarm `GND` -> board `GND`.
- Alarm `IN` -> default `PA8`.

The alarm input is active-low, so the firmware drives `PA8` low to turn the alarm on and high to turn it off.

If `IN` is connected to another MCU pin, edit these macros:

```c
#define ALARM_GPIO_PORT    GPIOA
#define ALARM_GPIO_PIN     GPIO_PIN_8
#define ALARM_GPIO_RCU     RCU_GPIOA
```

Source file:

```text
GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/main.c
```

### LD2410 Presence Output

For the initial demo, use the adapter board's three pins:

```text
LD2410 V -> 5V power
LD2410 G -> board GND
LD2410 O -> GD32 PF8
```

Important checks before connecting `O` to the GD32:

1. Power the LD2410 from `V/G`.
2. Use a multimeter to measure `O` against `G`.
3. Confirm the high level on `O` is about `3.3 V`, not `5 V`.
4. Connect `O` to `PF8` only after the high level is confirmed safe.

This firmware treats `O` as active-high: no person means `radar=0`, detected person means `radar=1`. If your module outputs the opposite polarity, edit:

```c
#define RADAR_ACTIVE_HIGH  1U
```

Default radar GPIO macros:

```c
#define RADAR_GPIO_PORT    GPIOF
#define RADAR_GPIO_PIN     GPIO_PIN_8
#define RADAR_GPIO_RCU     RCU_GPIOF
```

Timing in the current firmware:

- The firmware polls `PF8` every `500 ms`.
- When `PF8` is high, `PA8` is driven low in the same polling cycle, so firmware-side alarm latency is `0-500 ms`.
- There is no extra alarm hold time in the firmware. The alarm stays on only while LD2410 `O` stays high.
- LD2410B has a configurable "no-person duration" parameter. The official serial protocol describes it as the delay before changing from occupied to unoccupied after the detection area remains empty; the range is `0-65535 s`, and the protocol example uses `5 s`.

To reduce the alarm-off delay during bring-up, set LD2410B no-person duration to `1-2 s`.

Fast options:

1. Use the official `HLKRadarToolAPP` over Bluetooth and set no-person duration to `1` or `2`.
2. Use the official PC tool in `doc/hardware_manuals/HLK_LD2410B_official/HLK-LD2410 Tool.zip`.
3. Temporarily connect a USB-TTL adapter to LD2410B UART and run:

```powershell
cd "D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST"
python .\tools\ld2410_set_no_person_duration.py --port COM8 --seconds 2
```

Temporary UART wiring for option 3:

```text
USB-TTL GND -> LD2410 GND
USB-TTL RXD -> LD2410 TX
USB-TTL TXD -> LD2410 RX
LD2410 VCC  -> 5V
```

LD2410B default UART is `256000 baud, 8N1`. Replace `COM8` with the USB-TTL serial port used for the radar.

### LD2410 Detection Area

Primary reference:

```text
doc/hardware_manuals/HLK_LD2410B_official/HLK LD2410B生命存在感应模组说明书 V1.0 9.pdf
```

Useful pages:

- Page 3: product overview and typical wall-mounted usage diagram.
- Page 7: pin definition, `OUT` active-high behavior, and 5V power input.
- Page 17: electrical/performance parameters.

Important range facts from the official manual:

- The module is intended for indoor human-presence detection.
- Max sensing distance is up to `6 m`.
- Configurable detection distance is `0.75 m - 6 m`.
- Detection angle is approximately `±60 deg`.
- Distance resolution is `0.75 m`.
- It can detect moving and static/micro-moving human targets.

For parameter tuning, read:

```text
doc/hardware_manuals/HLK_LD2410B_official/LD2410B 串口通信协议 V1.08.pdf
```

Key protocol pages:

- Page 5: max distance gate, per-gate sensitivity, and no-person duration.
- Page 9: `0x0060` command for max motion/static gate and no-person duration.

For the current EdgeCare demo, treat the radar coverage as a forward-facing fan/cone area. It is not a precise rectangular boundary. Use it as a low-power trigger, then let the fixed camera ROI decide whether the person is actually inside the dangerous zone.

### OV5640-Class Camera SCCB ID Probe

Do not connect the full DVP camera bus first. The current firmware only probes the camera ID over SCCB/I2C so that wiring mistakes do not involve all data pins at once.

Minimum camera wiring:

```text
Camera 3V3  -> board 3V3
Camera GND  -> board GND
Camera SCL  -> GD32 PB10 / I2C1_SCL
Camera SDA  -> GD32 PB11 / I2C1_SDA
Camera RES  -> GD32 PD0
Camera PWON -> GD32 PD1
```

Important:

- Use `3.3 V`, not `5 V`, for the camera module.
- Leave `D0-D7`, `PCLK`, `HREF`, and `SYNC` disconnected until SCCB ID read works.
- The firmware assumes `RES` high releases reset and `PWON/PWDN` low powers the sensor. If ID read keeps timing out after wiring is verified, invert `PD1` first.
- `PH4` from the EVAL camera example is not exposed on the START board, so this project uses `PB10/PB11`.
- `PC9` is connected to START board LED1, so later DCI_D3 should use `PG11`, not `PC9`.

Expected boot log after reset:

```text
camera_sccb: SCL=PB10 SDA=PB11 RES=PD0 PWON=PD1
camera_id: ov5640_regs[0x300A,0x300B]=0x56 0x40
```

Once `0x56 0x40` is printed, the camera is confirmed as OV5640 and the firmware skips the fallback OV2640 ID probe.

If the camera is not connected or the power/reset direction is wrong, the firmware should print:

```text
camera_id: OV5640 ID read timeout/no ack on PB10/PB11
```

The periodic radar/alarm status logs should continue even if camera ID read fails.

## Keil Project

Open:

```text
GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/MDK-ARM/EdgeCare_GD32_Terminal.uvprojx
```

Target:

```text
EdgeCare_GD32_Terminal
```

Build output:

```text
GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/MDK-ARM/Objects/EdgeCare_GD32_Terminal.axf
```

Command-line build:

```powershell
.\build_keil.ps1
```

## Keil Download

Before the first download, check the probe settings in Keil:

1. Open `Options for Target`.
2. Go to `Debug`.
3. Select the debugger used by GD-Link. In most setups this is `CMSIS-DAP Debugger`.
4. Click `Settings` and confirm that SWD can detect the device.
5. Go to `Utilities`.
6. Enable `Use Debug Driver`.
7. Click `Settings` and confirm the flash algorithm is `GD32H7xx_3840KB`, start address `0x08000000`.
8. Click `Download`.

Expected result: the alarm lamp turns on when LD2410 `O` is high and turns off when LD2410 `O` is low.

## Serial Log

The firmware uses the official `EVAL_COM` port. On the `GD32H759I-START` board, the START demo defines it as `USART0` on `PF4/PF5`.

When using an external USB-TTL module instead of the board's USB serial path:

```text
USB-TTL GND -> board GND
USB-TTL RXD -> GD32 PF4 / USART0_TX
USB-TTL TXD -> GD32 PF5 / USART0_RX, optional for the current log-only test
```

Use a 3.3 V TTL adapter. Do not connect a 5 V TTL signal to the MCU UART pins.

Serial settings:

```text
115200 baud, 8 data bits, no parity, 1 stop bit
```

Expected boot log:

```text
edgecare-01 boot: EdgeCare GD32H759 terminal bring-up
log_format: [ts_ms] state=... radar=... infer_ms=... conf=... alarm=... seq=...
radar_input: LD2410 OUT active-high on PF8, alarm active-low on PA8
camera_sccb: SCL=PB10 SDA=PB11 RES=PD0 PWON=PD1
```

Expected periodic log:

```text
[0] state=IDLE radar=0 infer_ms=0 conf=0.00 alarm=0 seq=1
[500] state=ALARM radar=1 infer_ms=0 conf=1.00 alarm=1 seq=2
```

If VS Code Serial Monitor shows unreadable bytes such as `07 00 00 00 00 f1 e7 ff ...`, check in this order:

1. Set the baud rate to `115200`, not `230400`.
2. Select text display mode, 8N1, and no flow control.
3. Reset the board after opening the serial port.
4. For log-only testing on `GD32H759I-START`, connect only `GND` and `USB-TTL RXD -> PF4`; leave `TXD` disconnected.
5. Confirm the selected COM port is the USB-TTL adapter, not GD-Link or another device.
6. Confirm the current `EdgeCare_GD32_Terminal` firmware was actually downloaded: the alarm lamp should toggle every 500 ms.

If Keil reports `No ULINK device found`, the project is still using the official demo's ULINK configuration. Change the debugger to `CMSIS-DAP Debugger` and retry.
