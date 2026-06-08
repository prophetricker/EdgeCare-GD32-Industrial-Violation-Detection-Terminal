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
#define CAMERA_SCCB_GPIO_RCU   RCU_GPIOB
#define CAMERA_SCCB_GPIO_PORT  GPIOB
#define CAMERA_SCCB_SCL_PIN    GPIO_PIN_10
#define CAMERA_SCCB_SDA_PIN    GPIO_PIN_11
#define CAMERA_SCCB_AF         GPIO_AF_4

#define CAMERA_CTRL_GPIO_PORT  GPIOD
#define CAMERA_CTRL_GPIO_RCU   RCU_GPIOD
#define CAMERA_RES_PIN         GPIO_PIN_0
#define CAMERA_PWON_PIN        GPIO_PIN_1

#define CAMERA_SCCB_ADDR       0x78U
#define CAMERA_TIMEOUT         1000000U
#define CAMERA_FRAME_WIDTH     320U
#define CAMERA_FRAME_HEIGHT    240U
#define CAMERA_FRAME_BPP       2U
#define CAMERA_CAPTURE_BYTES   (CAMERA_FRAME_WIDTH * CAMERA_FRAME_HEIGHT * CAMERA_FRAME_BPP)
#define CAMERA_CAPTURE_WORDS   (CAMERA_CAPTURE_BYTES / 4U)
#define CAMERA_CAPTURE_TIMEOUT 60000000U

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
#define EDGECARE_ENABLE_CAMERA_BYTE_PLANE_DUMP 1U

/* Camera bring-up diagnostics. Set all three to 0 before real dataset capture. */
#define EDGECARE_ENABLE_CAMERA_TEST_PATTERN 1U
/* 0: off, 1: OV5640 ISP color bar via 0x503D, 2: DVP 8-bit test pattern via 0x4741. */
#define EDGECARE_CAMERA_TEST_PATTERN_MODE 1U
#define EDGECARE_ENABLE_CAMERA_SLOW_PCLK 1U
#define EDGECARE_CAMERA_PCLK_DIV_REG 0x08U
#define EDGECARE_ENABLE_CAMERA_CAPTURE_SWEEP 1U
#define EDGECARE_ENABLE_CAMERA_SNAPSHOT_CAPTURE 1U
/* OV5640 0x4740: HREF valid high, VSYNC valid low, data updates on PCLK falling edge. */
#define EDGECARE_CAMERA_DVP_POLARITY_REG 0x20U
/* OV5640 0x4745: x1 maps original D[9:2] onto physical D[7:0] for 8-bit MCU capture. */
#define EDGECARE_CAMERA_DATA_ORDER_DEFAULT 0x02U
/* Sweep all 0x4745 data-order/debug options, including bit-reversed output variants. */
#define EDGECARE_ENABLE_CAMERA_DATA_ORDER_SWEEP 1U

#endif /* BOARD_CONFIG_H */
