/*!
    \file    bsp_camera_ov5640.h
    \brief   OV5640 SCCB/DCI/DMA camera bring-up for EdgeCare
*/

#ifndef BSP_CAMERA_OV5640_H
#define BSP_CAMERA_OV5640_H

#include <stdint.h>

uint8_t bsp_camera_ov5640_id_probe(void);
uint8_t bsp_camera_ov5640_qvga_stream_init(void);
uint8_t bsp_camera_ov5640_capture_probe(void);
const uint8_t *bsp_camera_ov5640_frame(void);

#endif /* BSP_CAMERA_OV5640_H */