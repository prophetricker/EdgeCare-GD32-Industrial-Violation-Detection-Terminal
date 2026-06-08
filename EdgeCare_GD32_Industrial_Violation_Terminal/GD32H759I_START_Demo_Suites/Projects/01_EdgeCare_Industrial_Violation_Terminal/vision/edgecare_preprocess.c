/*!
    \file    edgecare_preprocess.c
    \brief   EdgeCare image preprocessing
*/

#include "edgecare_preprocess.h"
#include "../board/board_config.h"

static uint8_t yuv422_frame_y_at(const uint8_t *frame, uint32_t pixel_index, uint8_t y_phase)
{
    uint32_t pair_offset = (pixel_index / 2U) * 4U;
    uint32_t y_offset = pair_offset + ((pixel_index & 1U) ? 2U : 0U);

    if(0U != y_phase) {
        y_offset++;
    }

    return frame[y_offset];
}

static void update_stats(edgecare_preprocess_gray96_stats_t *stats,
                         uint8_t value,
                         uint32_t index,
                         uint32_t *sum,
                         uint32_t *checksum)
{
    *sum += value;
    *checksum ^= ((uint32_t)value << ((index & 3U) * 8U));
    *checksum = (*checksum << 3U) | (*checksum >> 29U);

    if(value < stats->min_value) {
        stats->min_value = value;
    }
    if(value > stats->max_value) {
        stats->max_value = value;
    }
}

void edgecare_preprocess_gray96_from_yuyv(const uint8_t *yuyv_frame,
                                          uint8_t *gray96,
                                          edgecare_preprocess_gray96_stats_t *stats)
{
    edgecare_preprocess_gray96_from_yuv422_phase(yuyv_frame, 0U, gray96, stats);
}

void edgecare_preprocess_gray96_from_yuv422_phase(const uint8_t *yuv422_frame,
                                                  uint8_t y_phase,
                                                  uint8_t *gray96,
                                                  edgecare_preprocess_gray96_stats_t *stats)
{
    uint32_t x;
    uint32_t y;
    uint32_t checksum = 0U;
    uint32_t sum = 0U;
    uint8_t value;

    stats->min_value = 0xFFU;
    stats->max_value = 0U;

    for(y = 0U; y < MODEL_INPUT_HEIGHT; y++) {
        uint32_t src_y = (y * CAMERA_FRAME_HEIGHT) / MODEL_INPUT_HEIGHT;
        for(x = 0U; x < MODEL_INPUT_WIDTH; x++) {
            uint32_t src_x = (x * CAMERA_FRAME_WIDTH) / MODEL_INPUT_WIDTH;
            uint32_t src_pixel = (src_y * CAMERA_FRAME_WIDTH) + src_x;
            uint32_t dst_index = (y * MODEL_INPUT_WIDTH) + x;

            value = yuv422_frame_y_at(yuv422_frame, src_pixel, y_phase);
            gray96[dst_index] = value;
            update_stats(stats, value, dst_index, &sum, &checksum);
        }
    }

    stats->mean = sum / MODEL_INPUT_BYTES;
    stats->checksum = checksum;
    stats->center[0] = gray96[((MODEL_INPUT_HEIGHT / 2U) * MODEL_INPUT_WIDTH) + (MODEL_INPUT_WIDTH / 2U)];
    stats->center[1] = gray96[((MODEL_INPUT_HEIGHT / 2U) * MODEL_INPUT_WIDTH) + (MODEL_INPUT_WIDTH / 2U) + 1U];
    stats->center[2] = gray96[(((MODEL_INPUT_HEIGHT / 2U) + 1U) * MODEL_INPUT_WIDTH) + (MODEL_INPUT_WIDTH / 2U)];
    stats->center[3] = gray96[(((MODEL_INPUT_HEIGHT / 2U) + 1U) * MODEL_INPUT_WIDTH) + (MODEL_INPUT_WIDTH / 2U) + 1U];
}

void edgecare_preprocess_byte_plane_from_frame(const uint8_t *frame,
                                               uint8_t byte_phase,
                                               uint8_t *plane,
                                               edgecare_preprocess_gray96_stats_t *stats)
{
    uint32_t x;
    uint32_t y;
    uint32_t checksum = 0U;
    uint32_t sum = 0U;

    stats->min_value = 0xFFU;
    stats->max_value = 0U;

    for(y = 0U; y < CAMERA_DIAG_PLANE_HEIGHT; y++) {
        uint32_t src_y = y * 2U;
        for(x = 0U; x < CAMERA_DIAG_PLANE_WIDTH; x++) {
            uint32_t src_x = x * 2U;
            uint32_t src_pair = (src_y * (CAMERA_FRAME_WIDTH / 2U)) + (src_x / 2U);
            uint32_t src_offset = (src_pair * 4U) + (byte_phase & 3U);
            uint32_t dst_index = (y * CAMERA_DIAG_PLANE_WIDTH) + x;
            uint8_t value = frame[src_offset];

            plane[dst_index] = value;
            update_stats(stats, value, dst_index, &sum, &checksum);
        }
    }

    stats->mean = sum / CAMERA_DIAG_PLANE_BYTES;
    stats->checksum = checksum;
    stats->center[0] = plane[((CAMERA_DIAG_PLANE_HEIGHT / 2U) * CAMERA_DIAG_PLANE_WIDTH) + (CAMERA_DIAG_PLANE_WIDTH / 2U)];
    stats->center[1] = plane[((CAMERA_DIAG_PLANE_HEIGHT / 2U) * CAMERA_DIAG_PLANE_WIDTH) + (CAMERA_DIAG_PLANE_WIDTH / 2U) + 1U];
    stats->center[2] = plane[(((CAMERA_DIAG_PLANE_HEIGHT / 2U) + 1U) * CAMERA_DIAG_PLANE_WIDTH) + (CAMERA_DIAG_PLANE_WIDTH / 2U)];
    stats->center[3] = plane[(((CAMERA_DIAG_PLANE_HEIGHT / 2U) + 1U) * CAMERA_DIAG_PLANE_WIDTH) + (CAMERA_DIAG_PLANE_WIDTH / 2U) + 1U];
}
