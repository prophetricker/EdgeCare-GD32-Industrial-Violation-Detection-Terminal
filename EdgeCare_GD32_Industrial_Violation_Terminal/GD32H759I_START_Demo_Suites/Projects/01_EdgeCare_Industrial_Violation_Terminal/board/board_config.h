/*!
    \file    board_config.h
    \brief   EdgeCare GD32H759I-START board-level configuration
*/

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "gd32h7xx.h"

#define ALARM_GPIO_PORT        GPIOA
#define ALARM_GPIO_PIN         GPIO_PIN_8
#define ALARM_GPIO_RCU         RCU_GPIOA

#define RADAR_GPIO_PORT        GPIOF
#define RADAR_GPIO_PIN         GPIO_PIN_8
#define RADAR_GPIO_RCU         RCU_GPIOF
#define RADAR_ACTIVE_HIGH      1U

#define CAMERA_SCCB            I2C1
#define CAMERA_SCCB_IDX        IDX_I2C1
#define CAMERA_SCCB_RCU        RCU_I2C1
#define CAMERA_SCCB_GPIO_RCU   RCU_GPIOF
#define CAMERA_SCCB_GPIO_PORT  GPIOF
#define CAMERA_SCCB_SCL_PIN    GPIO_PIN_1
#define CAMERA_SCCB_SDA_PIN    GPIO_PIN_0
#define CAMERA_SCCB_AF         GPIO_AF_4

#define CAMERA_CTRL_GPIO_PORT  GPIOD
#define CAMERA_CTRL_GPIO_RCU   RCU_GPIOD
#define CAMERA_RES_PIN         GPIO_PIN_0
#define CAMERA_PWON_PIN        GPIO_PIN_1

#define CAMERA_SCCB_ADDR       0x78U
#define CAMERA_TIMEOUT         1000000U
/* The module silk is PWON, not PWDN. Keep this switch visible while validating the power-control polarity. */
#define EDGECARE_CAMERA_PWON_ACTIVE_HIGH 0U
#define CAMERA_FRAME_WIDTH     320U
#define CAMERA_FRAME_HEIGHT    240U
#define CAMERA_FRAME_BPP       2U
#define CAMERA_CAPTURE_BYTES   (CAMERA_FRAME_WIDTH * CAMERA_FRAME_HEIGHT * CAMERA_FRAME_BPP)
#define CAMERA_CAPTURE_WORDS   (CAMERA_CAPTURE_BYTES / 4U)
#define CAMERA_CAPTURE_TIMEOUT 60000000U
/* Proven OV5640 SNR RAW path captures a 2-byte-per-pixel DVP window; phase 0 scored best. */
#define CAMERA_RAW8_BYTE_STRIDE 2U
#define CAMERA_RAW8_BYTE_PHASE 0U
/* Internal OV5640 DVP pattern proof only; keep 0 when collecting real-scene evidence. */
#define EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN 0U
/* RGB565 is the current photo-proof experiment because prior YUV422 frames were constant. */
#define EDGECARE_CAMERA_NORMAL_OUTPUT_RGB565 0U
#define EDGECARE_CAMERA_NORMAL_OUTPUT_ISP_YUV 0U
/* Focused photo-proof path: OV5640 app-note JPEG-to-YUV register set produced the first non-constant ISP frame. */
#define EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF 1U

#define MODEL_INPUT_WIDTH      96U
#define MODEL_INPUT_HEIGHT     96U
#define MODEL_INPUT_BYTES      (MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT)

#define CAMERA_DIAG_PLANE_WIDTH  (CAMERA_FRAME_WIDTH / 2U)
#define CAMERA_DIAG_PLANE_HEIGHT (CAMERA_FRAME_HEIGHT / 2U)
#define CAMERA_DIAG_PLANE_BYTES  (CAMERA_DIAG_PLANE_WIDTH * CAMERA_DIAG_PLANE_HEIGHT)

#define EDGECARE_DEVICE_ID     "edgecare-01"
#define EDGECARE_LOG_PERIOD_MS 500U

/* Set to 1 only during dataset capture. UART dumps are intentionally verbose. */
#define EDGECARE_ENABLE_GRAY96_DUMP      1U
#define EDGECARE_GRAY96_DUMP_CHUNK_BYTES 32U
#define EDGECARE_ENABLE_GRAY96_DUMP_VARIANTS 1U
#define EDGECARE_ENABLE_CAMERA_BYTE_PLANE_DUMP 0U
/* Keep disabled after fixing OV5640 0x4745 DVP byte mapping to a full-range 8-bit order. */
#define EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION 0U

/* Camera bring-up diagnostics. Keep heavy sweeps off unless a focused hardware log needs them. */
#define EDGECARE_ENABLE_CAMERA_TEST_PATTERN 0U
/* Use the full OV5640 app-note VGA YUV reference table, then override output to QVGA. */
#define EDGECARE_ENABLE_OV5640_FULL_REFERENCE_INIT 1U
/* Diagnostic: overlay ST's DVP enable / QVGA / YUV422 / polarity sequence for real-scene photo proof. */
#define EDGECARE_ENABLE_OV5640_ST_DVP_REFERENCE_PATH 1U
/* Experimental real-scene check: release OV5640 sensor/ISP clock-reset gates before normal RAW8 capture. */
#define EDGECARE_CAMERA_RELEASE_CLOCK_RESET_FOR_RAW 0U
/* 0: off, 1: OV5640 ISP color bar via 0x503D, 2: DVP 8-bit test pattern via 0x4741. */
#define EDGECARE_CAMERA_TEST_PATTERN_MODE 1U
#define EDGECARE_ENABLE_CAMERA_SLOW_PCLK 1U
#define EDGECARE_CAMERA_PCLK_DIV_REG 0x08U
#define EDGECARE_ENABLE_CAMERA_CAPTURE_SWEEP 0U
#define EDGECARE_ENABLE_CAMERA_SNAPSHOT_CAPTURE 1U
/* Temporarily force and sweep OV5640 VSYNC/HREF output pads during bring-up. */
#define EDGECARE_ENABLE_CAMERA_SYNC_OUTPUT_SWEEP 1U
/* Sweep OV5640 DVP timing/sync mode registers and keep the best line-sync candidate. */
#define EDGECARE_ENABLE_CAMERA_TIMING_SWEEP 1U
/* Diagnostic only: also keep a timing candidate when it restores VSYNC edges. */
#define EDGECARE_ENABLE_CAMERA_APPLY_VSYNC_EDGE_TIMING 1U
/* Keep the proven sync/timing diagnostics together for one-switch A/B bring-up. */
#define EDGECARE_ENABLE_CAMERA_SYNC_TIMING_DIAGS 0U
/* Sweep OV5640 DVP/JPEG/VFIFO output-mode controls after sync/timing diagnostics. */
#define EDGECARE_ENABLE_CAMERA_DVP_MODE_SWEEP 0U
/* Sample raw D0-D7 on GPIO at PCLK edges before DCI/DMA, to isolate sensor data bus from frame sync. */
#define EDGECARE_ENABLE_CAMERA_RAW_PCLK_SAMPLE 0U
/* Lightweight evidence sweep: compare ISP color bar, DVP pattern, and real scene before the normal capture. */
#define EDGECARE_ENABLE_CAMERA_PATTERN_SOURCE_SWEEP 0U
/* Narrow diagnostic: sweep OV5640 0x4745 data order for ISP color bar and real scene at raw PCLK level. */
#define EDGECARE_ENABLE_CAMERA_DATA_ORDER_SOURCE_SWEEP 0U
/* Capture SNR RAW through all 0x4745 data-order options and compare frame-structure scores. */
#define EDGECARE_ENABLE_CAMERA_RAW_DATA_ORDER_CAPTURE_SWEEP 0U
/* Sweep OV5640 0x501F output mux: ISP YUV422, ISP RAW, SNR RAW, ISP RAW CIP. */
#define EDGECARE_ENABLE_CAMERA_OUTPUT_MUX_SWEEP 0U
/* Try the proven-changing SNR RAW source through all DCI PCLK/HS/VS polarities. */
#define EDGECARE_ENABLE_CAMERA_RAW_DCI_MATRIX 0U
/* Sweep OV5640 upstream sensor/ISP path controls after DVP pattern proves the bus. */
#define EDGECARE_ENABLE_CAMERA_ISP_PATH_SWEEP 0U
/* Keep sweep off while pinning the current best JPEG-to-YUV candidate as the normal RAM frame. */
#define EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_TUNE_SWEEP 0U
/* Focused A/B only. Default stays on OV5640 auto exposure because the manual 0x0375/0x02A run was darker. */
#define EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE 0U
#define EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES 0x0375U
#define EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_GAIN 0x02AU
/* Current JPEG-to-YUV baseline: DCI rising edge reduces horizontal striping versus falling. */
#define EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING 1U
/* Focused A/B: keep JPEG-to-YUV + DCI rising, only try the reference VFIFO 0x460B/0x460C pair. */
#define EDGECARE_CAMERA_JPEG_TO_YUV_VFIFO_REF_CANDIDATE 0U
/* Focused A/B: keep JPEG-to-YUV + DCI rising, only try 0x3824=0x08 PCLK divider. */
#define EDGECARE_CAMERA_JPEG_TO_YUV_PCLKDIV08_CANDIDATE 0U
/* Focused A/B: force OV5640 SC PLL CONTROL0 MIPI bit mode from 10-bit (0x1A) to 8-bit DVP-style (0x18). */
#define EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP 0U
/* Focused root-cause diagnostic: run JPEG-to-YUV full-frame DMA capture for 0x4745 orders 0..7. */
#define EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP 0U
/* Read-only window/active-line diagnostics for OV5640 0x3800..0x3815. */
#define EDGECARE_ENABLE_CAMERA_WINDOW_READBACK 1U
/* Temporarily force OV5640 D[9:2] pads and read the MCU-side D0-D7 GPIO bus. */
#define EDGECARE_ENABLE_CAMERA_DATA_PAD_SWEEP 0U
/* Poll DCI/DMA live registers during capture timeout to isolate AF/DCI from GPIO-level activity. */
#define EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE 0U
/* Compare DCI snapshot vs continuous capture semantics before the full-frame capture attempt. */
#define EDGECARE_ENABLE_CAMERA_CAPTURE_MODE_SWEEP 0U
/* OV5640 0x4740: HREF valid high, VSYNC valid low, data updates on PCLK falling edge. */
#define EDGECARE_CAMERA_DVP_POLARITY_REG 0x20U
/* OV5640 0x4745: 0x00 keeps DVP Data[9:0]; current 8-bit bus captures full-range YUV here. */
#define EDGECARE_CAMERA_DATA_ORDER_DEFAULT 0x00U
/* Sweep all 0x4745 data-order/debug options, including bit-reversed output variants. */
#define EDGECARE_ENABLE_CAMERA_DATA_ORDER_SWEEP 0U

#endif /* BOARD_CONFIG_H */
