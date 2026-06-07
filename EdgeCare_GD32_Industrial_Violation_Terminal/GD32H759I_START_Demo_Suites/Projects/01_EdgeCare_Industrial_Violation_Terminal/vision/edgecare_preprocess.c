/*!
    \file    edgecare_preprocess.c
    \brief   EdgeCare image preprocessing
*/

#include "edgecare_preprocess.h"
#include "../board/board_config.h"

static uint8_t yuyv_frame_y_at(const uint8_t *frame, uint32_t pixel_index)
{
    uint32_t pair_offset = (pixel_index / 2U) * 4U;

    /* OV5640 0x4300 low bits select YUYV: even pixel Y at byte 0, odd pixel Y at byte 2. */
    if(0U == (pixel_index & 1U)) {
        return frame[pair_offset];
    }

    return frame[pair_offset + 2U];
}

void edgecare_preprocess_gray96_from_yuyv(const uint8_t *yuyv_frame,
                                          uint8_t *gray96,
                                          edgecare_preprocess_gray96_stats_t *stats)
{
    uint32_t x;
    uint32_t y;
    uint32_t checksum = 0U;
    uint32_t sum = 0U;
    uint8_t min_value = 0xFFU;
    uint8_t max_value = 0U;
    uint8_t value;

    for(y = 0U; y < MODEL_INPUT_HEIGHT; y++) {
        uint32_t src_y = (y * CAMERA_FRAME_HEIGHT) / MODEL_INPUT_HEIGHT;
        for(x = 0U; x < MODEL_INPUT_WIDTH; x++) {
            uint32_t src_x = (x * CAMERA_FRAME_WIDTH) / MODEL_INPUT_WIDTH;
            uint32_t src_pixel = (src_y * CAMERA_FRAME_WIDTH) + src_x;
            uint32_t dst_index = (y * MODEL_INPUT_WIDTH) + x;

            value = yuyv_frame_y_at(yuyv_frame, src_pixel);
            gray96[dst_index] = value;
            sum += value;
            checksum ^= ((uint32_t)value << ((dst_index & 3U) * 8U));
            checksum = (checksum << 3U) | (checksum >> 29U);

            if(value < min_value) {
                min_value = value;
            }
            if(value > max_value) {
                max_value = value;
            }
        }
    }

    stats->min_value = min_value;
    stats->max_value = max_value;
    stats->mean = sum / MODEL_INPUT_BYTES;
    stats->checksum = checksum;
    stats->center[0] = gray96[((MODEL_INPUT_HEIGHT / 2U) * MODEL_INPUT_WIDTH) + (MODEL_INPUT_WIDTH / 2U)];
    stats->center[1] = gray96[((MODEL_INPUT_HEIGHT / 2U) * MODEL_INPUT_WIDTH) + (MODEL_INPUT_WIDTH / 2U) + 1U];
    stats->center[2] = gray96[(((MODEL_INPUT_HEIGHT / 2U) + 1U) * MODEL_INPUT_WIDTH) + (MODEL_INPUT_WIDTH / 2U)];
    stats->center[3] = gray96[(((MODEL_INPUT_HEIGHT / 2U) + 1U) * MODEL_INPUT_WIDTH) + (MODEL_INPUT_WIDTH / 2U) + 1U];
}
