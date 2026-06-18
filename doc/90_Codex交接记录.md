# EdgeCare Codex Handoff

Updated: 2026-06-16

Use this file when opening a fresh Codex session or switching accounts. The new session should read this file first, then `AGENTS.md`, `CODING_STYLE.md`, `看板.md`, and the camera notes linked below.

## Project Root

```text
D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST
```

Do not work in the old Chinese-path project folder unless the user explicitly asks. The active Git branch is:

```text
feature/firmware-module-refactor
```

## One-Sentence Goal

Build an initial-round demo for the GigaDevice graduate electronics contest: `GD32H759I-START` performs fixed-camera industrial dangerous-zone intrusion detection, drives a local alarm, and later sends structured alert JSON through VW553 without streaming video.

## Current Hardware State

- Board: `GD32H759I-START`.
- Keil target: `EdgeCare_GD32_Terminal`.
- User serial log: `COM8`, `115200 8N1`.
- Alarm: active-low alarm input on `PA8`.
- Radar: LD2410 `O -> PF8`, active high, no-person duration set to `1 s` in HLK app.
- Camera: OV5640 DVP module, SCCB ID confirmed as `0x56 0x40`.
- J-Link PLUS is connected and validated through the board SWD header.
- Safe J-Link device string for this workflow: `GD32H759IMT6`; do not use Keil's shorter `GD32H759IM` as a SEGGER `-Device` value.
- 2026-06-16 current dataset state: `OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00` produces recognizable real-scene raw `gray96` with `scale=raw`, `min=0`, `max=255`. This is suitable for starting MVP dataset collection. Do not mix old `gray96_lshift2_mvp` samples into the new dataset.
- 2026-06-16 root cause closed for the previous low byte range: full-frame `jpeg_yuv_order_00..07` sweep showed `order=0x00` is `not_right_shift2_like`, while old `0x02/0x06` remain `right_shift2_like`. Current defaults are `EDGECARE_CAMERA_DATA_ORDER_DEFAULT=0x00U` and `EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION=0U`.
- 2026-06-16 current evidence: `.embeddedskills/logs/serial/edgecare_camera_current_sweep_20260616_112146.log`, `.embeddedskills/logs/serial/edgecare_camera_order00_normal_20260616_112516.log`, `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_order00_frame_20260616_112553.bin`, decoded previews under `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260616_112553_order00/`.
- 2026-06-12 replacement START board baseline: J-Link flash at `100 kHz` succeeded, CH340 log port is `COM8`, and OV5640 minimum six-wire SCCB/control wiring (`3V3/GND/SCL/SDA/RES/PWON`) reads `camera_id: ov5640_regs[0x300A,0x300B]=0x56 0x40`. With only these six wires connected, `pclk_edges=0`, floating DVP data reads, and DCI capture timeout are expected.
- 2026-06-13 full DVP retest on the replacement START board: SCCB remains stable, PCLK is active, forced HREF/VSYNC are visible at both GPIO and DCI, data-pad forced-output had already passed on the current mapping, and applying `0x471D=0x00` makes DCI/DMA complete a full QVGA frame. The remaining blocker is that the OV5640 natural/test-pattern pixel stream is constant (`0x3F` on raw PCLK and DMA capture under `data_order=0x02`), so the next investigation should focus on OV5640 output source/path registers rather than wiring.
- 2026-06-13 RAW8 transport path is validated on hardware: OV5640 `0x501F=0x04` SNR RAW output, `0x471D=0x00`, `0x4740=0x22`, and GD32 DCI `PCLK rising + HS blank high + VS blank high` produce a complete QVGA DMA frame. This is transport proof, not photo proof: the saved real-scene RAW8 candidates look like high-frequency noise. Latest clean RAW8 transport log: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_012250.log`.
- 2026-06-13 `OV5640_JPEG_TO_YUV_REF` produced the first recognizable real-scene grayscale frame. The image is still diagnostic quality only: dark/low dynamic range, horizontal striping, and unresolved color/YUV ordering. Clean proof log: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_170316.log`. Clean proof frame: `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_frame_20260613_170316_clean.bin`.
- 2026-06-13 `href_gate24` (`0x4740=0x24`) was saved and decoded as a true hardware run. It still shows real-scene geometry but is visually worse than the clean `0x20` baseline, with lower useful dynamic range and heavier speckle. Do not pin `0x24` as the normal path. Evidence frame: `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_href_gate24_frame_20260613_173850.bin`; decoded preview: `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_173850_href_gate24/raw_yuv422_yuyv_y_320x240_norm.bmp`; log: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_173850.log`.
- 2026-06-13 manual exposure `3503=0x07`, exposure `0x0375`, gain `0x02A` was tested and rejected. It was darker and worse than auto exposure. Keep `EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE=0`. Evidence log: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_202337.log`.
- 2026-06-13 `0x4745` data-order raw-PCLK sweep was tested and rejected as the main fix. Raw-PCLK samples stayed mostly constant per order; it did not explain the real-frame striping. Keep `EDGECARE_ENABLE_CAMERA_DATA_ORDER_SOURCE_SWEEP=0`. Evidence log: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_203147.log`.
- Local OV5640 manuals are available under `doc/hardware_manuals/`. Important references: `OV5640_Datasheet.pdf`, `OV5640_camera_module_software_application_notes_1.pdf`, `OV5640_CSP3_DS_1.0卓越科_.pdf`, and `FD5640-500W-V11自动对焦.pdf`. Do not state that local OV5640 datasheet/app-note material is missing.
- Datasheet reference: `OV5640_Datasheet.pdf` p.135 says `0x4745 DATA ORDER` bit[2:1] options are `00: Data[9:0]`, `10: {data[7:0],data[9:8]}`, and `x1: {data[1:0],data[9:2]}`; bit0 reverses output bit order. Historical default `0x4745=0x02` made the low 8-bit MCU capture receive the wrong slice for this path and caused the observed `right_shift2_like` `/4` byte scale. Current fixed default is `0x4745=0x00`.
- 2026-06-14 added a default-off full-frame JPEG-to-YUV data-order diagnostic: `EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP=0`. When enabled, it captures `jpeg_yuv_order_00..07` on the recognizable `OV5640_JPEG_TO_YUV_REF + DCI rising` path and prints one `camera_byte_scale[...]` line per order with phase0..phase3 raw/lshift2 min/max/mean plus `hint=right_shift2_like|not_right_shift2_like`. Use these serial lines as the primary eight-order comparison, because one RAM save after the sweep only preserves the final captured frame. `tools/build_camera_jpeg_yuv_order_sweep.ps1 -Build` temporarily enables the sweep for one Keil build and restores `board_config.h`; it never flashes. Run it as the final build step before any manual diagnostic flash, because a later normal build/verify will rebuild the default non-sweep image. `tools/check_camera_jpeg_yuv_order_sweep_build.ps1` is a read-only artifact check; only flash the diagnostic build manually if it prints `ready_to_flash_sweep=1`. After manual flash, `tools/capture_camera_jpeg_yuv_order_sweep_log.ps1 -DurationSec 60` is the capture-only path: it verifies `ready_to_flash_sweep=1`, resets/runs the target through `jlink_reset_capture_serial.ps1`, and runs `tools/analyze_camera_byte_scale_log.py`; it does not build or flash. `tools/jlink_reset_capture_serial.ps1` keeps `jpeg_yuv_order_capture_sweep` and `camera_byte_scale[...]` in its interesting-line summary and prints the follow-up analyzer command when byte-scale records are present. The log analyzer summarizes the eight lines into `candidate_orders`, `missing_orders`, `timeout_orders`, and `conclusion=4745_mapping_candidate_found|all_orders_still_right_shift2_like|incomplete_sweep_log`; `timeout_orders` means firmware printed `skipped=capture_timeout` for that order. The 2026-06-16 sweep already used this path to identify `0x4745=0x00` as the fixed default. Do not enable the sweep for normal MVP/data-collection firmware.
- 2026-06-13 DCI PCLK rising-edge sampling became the current best JPEG-to-YUV baseline. It keeps the OV5640 side at `0x4740=0x20` and improves the visible real-scene grayscale image versus falling-edge capture. Current default: `EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING=1`. Confirmation log: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_204732.log`; saved frame: `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_rising_default_frame_20260613_204756.bin`; decoded preview: `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_204756_rising_default/raw_yuv422_yuyv_y_320x240_norm.bmp`.
- 2026-06-13 two normal-path JPEG-to-YUV VFIFO/PCLK candidates were tested and rejected as default changes. `460B=0x35,460C=0x22,3824=0x04` evidence: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_205729.log`, `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_vfifo_ref_candidate_frame_20260613_205753.bin`, and same-scene recheck decoded under `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_210331_vfifo_ref_candidate_recheck/`. `460B=0x37,460C=0x20,3824=0x08` evidence: `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_210739.log`, `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_pclkdiv08_candidate_frame_20260613_210803.bin`, decoded under `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_210803_pclkdiv08_candidate/`. Both were visually and metrically equivalent to `baseline20_recheck`; keep `EDGECARE_CAMERA_JPEG_TO_YUV_VFIFO_REF_CANDIDATE=0` and `EDGECARE_CAMERA_JPEG_TO_YUV_PCLKDIV08_CANDIDATE=0`.
- 2026-06-14 `tools/analyze_camera_byte_scale.py` now gives a focused offline verdict for saved raw frames. Existing recognizable JPEG-to-YUV frames (`rising_default`, `window_readback`, `lshift2`, and `baseline20_recheck`) all classify as `right_shift2_like`: Y phases remain around max `48..52`, U/V phases have raw mean `31`, and `lshift2` U/V mean becomes `124`. This makes a single loose high data bit less likely than a systematic OV5640 DVP byte-scale/data-slice/output bit-width issue. Still treat it as evidence, not a root-cause fix.
- 2026-06-13 focused `0x3034=0x18` bit-mode A/B was tested and rejected as a default fix. Log `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_213104.log` read back `3034=0x18`, but frame `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_force_8bit3034_frame_20260613_213104.bin` still had Y phases only `2..44` and U/V near `31`. Decoded output is under `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_213104_force_8bit3034/`. Default was rebuilt and flashed back to baseline; `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_213715.log` confirms `3034=0x1A`, `3824=0x04`, `460b=0x37`, `460c=0x20`, `4740=0x20`, `4745=0x02`, DCI rising, and `dma=done`. Keep `EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP=0`.
- 2026-06-13 firmware-side `gray96` left-shift-by-2 compensation was enabled as a temporary MVP workaround, not as a camera root-cause fix. It is now disabled by default: `EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION=0`. Historical log `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_221540.log` showed `scale=lshift2`; keep those samples only as evidence and do not use them for the current dataset.
- 2026-06-13 read-only `0x3800..0x3815` window diagnostics were added and verified on hardware. Latest log `.embeddedskills/logs/serial/edgecare_reset_capture_20260613_225918.log` shows `camera_window_readback[normal_jpeg_to_yuv_ref]: crop=0,4-2623,1947 sensor_span=2624x1944 output=320x240 hts=1896 vts=984 offset=16,6 inc=0x31,0x31`, then `camera_capture[normal_jpeg_to_yuv_ref]: dma=done words=38400 remain=0 checksum=0xE83E2040` and `preprocess_gray96 ... scale=lshift2 ... min=8 max=192 mean=92`. Saved frame: `data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_window_readback_frame_20260613_225918.bin`; decoded preview directory: `data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_225918_window_readback/`. Interpretation: a simply wrong output window is unlikely to be the current root cause; raw Y is still only about `2..49`, so focus next on DVP data-bit mapping / byte-scale / physical D0-D7 anomalies.
- Current camera wiring to keep fixed:

```text
PCLK -> PA6
HERF/HREF -> PA4
SYNC -> PB7
D0 -> PC6
D1 -> PC7
D2 -> PC8
D3 -> PG11
D4 -> PC11
D5 -> PB6
D6 -> PE5
D7 -> PB9
SCL -> PF1
SDA -> PF0
RES -> PD0
PWON -> PD1
```

## Latest Camera Debug Conclusion

Do not continue random pin probing. Do not claim final color photo quality yet.

What is proven: SCCB ID, current remapped DVP wiring, GD32 DCI/DMA full-frame transfer, `OV5640_JPEG_TO_YUV_REF`, DCI rising-edge sampling, and `0x4745=0x00` together produce recognizable real-scene raw `96x96` grayscale data suitable for MVP dataset collection.

What is not proven: final color photo quality. RGB/YUV color interpretation still has artifacts, so the dataset should be collected from raw `gray96`, not from color images.

Latest clean real-scene grayscale proof:

```text
.embeddedskills/logs/serial/edgecare_camera_order00_normal_20260616_112516.log
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_order00_frame_20260616_112553.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260616_112553_order00/raw_yuv422_yuyv_y_320x240.bmp
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260616_112553_order00/gray96_y02_96x96.bmp
```

Key proof lines:

```text
camera_capture_probe: ... data_order_default=0x00 ... jpeg_yuv_order_capture_sweep=0
camera_capture[normal_jpeg_to_yuv_ref]: data_order=0x00/read0x00 dma=done words=38400 remain=0
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=raw ... min=0 max=255 mean=66
infer_probe: model=stat_placeholder ... note=replace_with_trained_model
```

Current default firmware is a focused JPEG-to-YUV cleanup build:

```text
EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF=1
EDGECARE_CAMERA_NORMAL_OUTPUT_RGB565=0
EDGECARE_CAMERA_NORMAL_OUTPUT_ISP_YUV=0
EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN=0
EDGECARE_ENABLE_CAMERA_CAPTURE_SWEEP=0
EDGECARE_ENABLE_CAMERA_SYNC_TIMING_DIAGS=0
EDGECARE_ENABLE_CAMERA_RAW_PCLK_SAMPLE=0
EDGECARE_ENABLE_CAMERA_PATTERN_SOURCE_SWEEP=0
EDGECARE_ENABLE_CAMERA_OUTPUT_MUX_SWEEP=0
EDGECARE_ENABLE_CAMERA_RAW_DCI_MATRIX=0
EDGECARE_ENABLE_CAMERA_ISP_PATH_SWEEP=0
EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_TUNE_SWEEP=0
EDGECARE_ENABLE_CAMERA_DATA_PAD_SWEEP=0
EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE=0
EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE=0
EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING=1
EDGECARE_CAMERA_JPEG_TO_YUV_VFIFO_REF_CANDIDATE=0
EDGECARE_CAMERA_JPEG_TO_YUV_PCLKDIV08_CANDIDATE=0
EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP=0
EDGECARE_CAMERA_DATA_ORDER_DEFAULT=0x00
EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION=0
EDGECARE_ENABLE_GRAY96_DUMP=1
```

The narrow sweep on the already-proven `OV5640_JPEG_TO_YUV_REF` path is currently off. Keep the variants available for future A/B checks, but the latest visual conclusion is that the `0x20` baseline is better than `href_gate24/0x24`:

```text
camera_jpeg_to_yuv_tune_sweep: enabled source=OV5640_JPEG_TO_YUV_REF variants=6 note=compare_stripe_brightness_window
camera_jpeg_to_yuv_tune_sweep[jpeg_tune_base_ref]: request 460b=0x37 460c=0x20 3824=0x04 471d=0x00 4740=0x20
camera_jpeg_to_yuv_tune_sweep[jpeg_tune_vfifo_init]: request 460b=0x35 460c=0x22 3824=0x04 471d=0x00 4740=0x20
camera_jpeg_to_yuv_tune_sweep[jpeg_tune_pclkdiv08]: request 460b=0x37 460c=0x20 3824=0x08 471d=0x00 4740=0x20
camera_jpeg_to_yuv_tune_sweep[jpeg_tune_pclkdiv02]: request 460b=0x37 460c=0x20 3824=0x02 471d=0x00 4740=0x20
camera_jpeg_to_yuv_tune_sweep[jpeg_tune_polarity22_rise_hh]: request 460b=0x37 460c=0x20 3824=0x04 471d=0x00 4740=0x22
camera_jpeg_to_yuv_tune_sweep[jpeg_tune_href_gate24]: request 460b=0x37 460c=0x20 3824=0x04 471d=0x00 4740=0x24
camera_capture[normal_jpeg_to_yuv_ref]: ...
```

Important: J-Link RAM save captures only the final normal frame, not every sweep candidate. If the sweep is re-enabled later, use the serial summaries first; if a candidate looks better by checksum/repeated/quality metrics, pin that candidate as the normal path, rebuild/flash, then save and decode a frame. Do not rely on the numeric score alone; `href_gate24` scored higher but looked worse.

Photo evidence files:

```text
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_frame_20260613_170316_clean.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_170316_clean/raw_yuv422_yuyv_y_320x240_norm.bmp
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_170316_clean/raw_640x240_norm.bmp
data/photo_evidence_jpeg_to_yuv_ref/edgecare_jpeg_to_yuv_ref_href_gate24_frame_20260613_173850.bin
data/photo_evidence_jpeg_to_yuv_ref/decoded_20260613_173850_href_gate24/raw_yuv422_yuyv_y_320x240_norm.bmp
data/photo_evidence_real_raw8/edgecare_real_raw8_frame_20260613_101207.bin
data/photo_evidence_real_raw8/decoded_20260613_101207/raw_stride2_phase0_160x240_norm.bmp
data/photo_evidence_dvp_pattern/edgecare_dvp_pattern_frame_20260613_093617.bin
data/photo_evidence_dvp_pattern/decoded_20260613_093617/raw_320x240.bmp
```

Interpretation: the JPEG-to-YUV frame is the first recognizable real-scene grayscale proof. The DVP pattern file proves the bus/capture pipeline can carry structured bytes. The real RAW8 decoded file is noise-like and is not acceptable as a photo or training sample.

The older ISP/YUV path produced constant `0x02020202` frames and RGB565 did not become the winning path. Continue from `OV5640_JPEG_TO_YUV_REF` and clean striping/brightness/color before collecting a dataset.

## Previous Camera Debug Notes

Historical replacement-board log before the JPEG-to-YUV breakthrough:

```text
.embeddedskills/logs/serial/edgecare_reset_capture_20260613_000154.log
```

It proves the MCU-side capture path now works:

```text
camera_id: ov5640_regs[0x300A,0x300B]=0x56 0x40
camera_dvp_gpio: pclk_edges=2481769 ...
camera_timing_sweep_best: tag=href_default_vsync0 apply=1 reason=vsync_edges_diag ...
camera_capture_mode_sweep[snapshot]: ... dma_cnt=38400->0 ...
camera_capture[pclk_falling_hs_blank_low_vs_blank_high]: data_order=0x02/read0x02 dma=done words=38400 remain=0 ...
```

But the pixel payload is still constant:

```text
camera_raw_pclk_sample[all]: samples=32768 nonzero=32768 changes=0 min=0x3F max=0x3F ...
camera_capture[selected_best_for_dump]: ... repeated=38399 ... first=3F3F3F3F ... last=3F3F3F3F
```

Interpretation at that stage: new board, J-Link, COM8, SCCB, DVP sync pins, DCI, DMA, and the physical data-bus mapping were proven enough to stop random wiring changes and move into OV5640 internal output source/path configuration.

The latest `camera_sync_sweep[...]` result proved the standard sync wiring:

- Forced HREF high changed `PA4`/`href_high`.
- Forced VSYNC high changed `PB7`/`sync_high`.
- Therefore `HERF/HREF -> PA4` reaches the OV5640 HREF pad, and `SYNC -> PB7` reaches the OV5640 VSYNC pad.

That historical stage still had no recognizable real-scene image. Since then, the `OV5640_JPEG_TO_YUV_REF` path has produced a recognizable grayscale real-scene frame, so the current work is cleanup rather than proof-of-life: reduce striping, improve brightness/dynamic range, and resolve color/YUV ordering.

The full OV5640 app-note VGA YUV reference init plus QVGA override was tested earlier (`camera_init_path=full_reference_vga_then_qvga`) and did not by itself produce a usable photo. The current default uses the proven JPEG-to-YUV reference path and a narrow tune sweep rather than the older RGB565/source-sweep path.

The latest user log also shows `camera_dvp_mode_sweep_best: tag=none apply=0`, so the small OV5640 DVP/JPEG/VFIFO output-mode matrix has been ruled out too:

```text
0x300E, 0x302E, 0x4713
```

The optional `camera_data_pad_sweep[...]` diagnostic can still be re-enabled if the current JPEG-to-YUV path regresses or the known internal pattern stops being structured. It temporarily forces OV5640 data pads through datasheet registers `0x3017/0x3018/0x301A/0x301B/0x301D/0x301E`, then reads the MCU-side D0-D7 GPIO bus. `0x301E[7:2]` is required for D[5:0] output select; an earlier diagnostic missed this register and only truly forced D[9:6].

After that, `camera_raw_pclk_sample:` samples D0-D7 as GPIO on PCLK edges before DCI/DMA, gated by the current HREF level. Together, these diagnostics distinguish "physical data pads/wires cannot be driven" from "natural OV5640 streaming/test-pattern output is stuck or blanking-like".

Latest auto flash/capture on 2026-06-11 used:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\jlink_flash_edgecare.ps1 -Flash
powershell -ExecutionPolicy Bypass -File .\tools\jlink_reset_capture_serial.ps1 -DurationSec 45
```

Captured log:

```text
.embeddedskills/logs/serial/edgecare_reset_capture_20260611_005211.log
```

Key evidence:

```text
camera_data_pad_sweep: set_output_dir read_3017=0xFF read_3018=0xFF read_301d=0x0F read_301e=0xFC
camera_data_pad_sweep[pad_d9_d2_01]: read=0x13 strong_match=0 read_down=0x03
camera_data_pad_sweep[pad_d9_d2_02]: read=0x10 strong_match=0 read_down=0x00
camera_data_pad_sweep[pad_d9_d2_04]: read=0x14 strong_match=1 read_down=0x04
camera_data_pad_sweep[pad_d9_d2_08]: read=0x18 strong_match=1 read_down=0x08
camera_data_pad_sweep[pad_d9_d2_10]: read=0x10 strong_match=0 read_down=0x00
camera_data_pad_sweep[pad_d9_d2_20]: read=0x30 strong_match=1 read_down=0x20
camera_data_pad_sweep[pad_d9_d2_40]: read=0x50 strong_match=1 read_down=0x40
camera_data_pad_sweep[pad_d9_d2_80]: read=0x90 strong_match=1 read_down=0x80
camera_data_pad_summary[pad_d9_d2]: bit_tests=8 full_matches=2 strong_matches=6 present_mask=0xEF strong_mask=0xED missing_mask=0x12 extra_mask=0x02 zero_float_mask=0x10 ff_missing_mask=0x10
camera_raw_pclk_sample: pclk_edges=4096 href_gated=4096 nonzero=4096 changes=0 min=0x3F max=0x3F first=3F,3F,3F,3F
camera_capture_mode_sweep[snapshot]: fv_polls=0 ef_seen=0 vsif_seen=0 elif_seen=0 dma_cnt=38400->38400
camera_capture_mode_sweep[continuous]: fv_polls=0 ef_seen=0 vsif_seen=0 elif_seen=0 dma_cnt=38400->38400
```

Interpretation update: after fixing the software-side `0x301E` omission, the old-board log showed OV5640 forced output reaching MCU-side D0, D2, D3, D5, D6, and D7 under the `pad_d9_d2` mapping. It also showed expected bit0 raising MCU D1 (`extra_mask=0x02`) and expected bit1 not appearing by itself (`missing_mask=0x02`). The user later confirmed this came from the previous board's PC6/PC7 short. Do not carry the D0/D1 coupling conclusion forward as a new-board root cause unless a fresh new-board data-pad sweep reproduces it. D4 weak/contact uncertainty may still be checked, and the new board's current Dupont-male-into-unpopulated-header wiring should be treated as a possible contact/pin-position risk even if it feels tight.

## What To Do Next

The user needs the project ready for dataset collection. Keep wiring fixed. The normal JPEG-to-YUV path has been rebuilt, flashed, and confirmed on the `0x20` + DCI rising-edge + `0x4745=0x00` baseline:

```text
camera_capture[normal_jpeg_to_yuv_ref]: data_order=0x00/read0x00 dma=done words=38400 remain=0
preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=raw ... min=0 max=255
```

Latest confirmation log:

```text
.embeddedskills/logs/serial/edgecare_camera_order00_normal_20260616_112516.log
```

Interpretation:

- Start MVP dataset collection from raw `jpeg_to_yuv_ref_y02` `gray96` after visual BMP checks. Use `doc/01_数据集采集全流程手册.md` as the operator manual.
- Do not spend more time on manual exposure `0x0375/0x02A`, `0x4745` raw-PCLK data-order sweep, `460B/460C=0x35/0x22`, `3824=0x08`, or `0x3034=0x18` unless a later regression invalidates this evidence.
- Do not spend more time on a simple `0x3800..0x3815` output-window hypothesis unless later evidence contradicts the 2026-06-13 readback.
- Remaining camera cleanup target is color/YUV interpretation, not grayscale dataset viability. Treat D0/D1 coupling as old-board PC6/PC7-short history, not a current new-board fact.
- If it regresses to timeout/constant/noise, compare against the saved proof frame before changing wiring.
- Do not reopen data-pad or sync wiring diagnostics unless the normal JPEG-to-YUV capture regresses badly.

## Important Files

```text
AGENTS.md
CODING_STYLE.md
看板.md
doc/03_快速接线表.md
doc/04_OV5640相机调试记录.md
doc/06_开发工作流_VSCode_Codex_Keil.md
EdgeCare_GD32_Industrial_Violation_Terminal/GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/board/board_config.h
EdgeCare_GD32_Industrial_Violation_Terminal/GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/bsp/bsp_camera_ov5640.c
```

## Verification Command

Run this before claiming the firmware still builds:

```powershell
cd "D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST"
powershell -ExecutionPolicy Bypass -File .\tools\verify_edgecare.ps1
```

Latest verified result:

```text
0 Error(s), 0 Warning(s)
Program Size: Code=23374 RO-data=6886 RW-data=8 ZI-data=176216
```

## J-Link State

Read-only probe command:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\jlink_probe.ps1 -Device GD32H759IMT6
```

Validated output includes:

```text
VTref=3.221V
Device "GD32H759IMT6" selected.
Found SW-DP with ID 0x0BD12477
Found Cortex-M7 r1p2
E000ED00 = 411FC272
```

Available automation scripts:

```text
tools/jlink_probe.ps1                read-only USB/SWD/CPUID probe
tools/jlink_flash_edgecare.ps1       dry-run by default; requires -Flash to program
tools/jlink_gdbserver_edgecare.ps1   starts JLinkGDBServerCL on port 2331
tools/jlink_reset_capture_serial.ps1 opens COM8, resets by J-Link, captures startup logs
```

Do not auto-flash unless the user explicitly asks. `tools/jlink_flash_edgecare.ps1` refuses to flash when `-Device Cortex-M7` is passed, because internal Flash programming needs a SEGGER-known GD32H7 device.

## New Session Prompt

Paste this to the new Codex session:

```text
We are continuing the EdgeCare GD32H759I-START industrial violation detection project.
Project root: D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST
First read: doc/90_Codex交接记录.md, AGENTS.md, CODING_STYLE.md, 看板.md.
Current task: collect the first MVP dataset using the fixed `OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00 + raw gray96` path. Read `doc/01_数据集采集全流程手册.md`. Keep `EDGECARE_ENABLE_GRAY96_DUMP=1`, collect `jpeg_to_yuv_ref_y02` samples for `empty/safe/intrusion`, and visually inspect BMPs before scaling up. Do not mix old `gray96_lshift2_mvp` data. Color/YUV RGB is still unresolved and is not required for the first dataset.
```
