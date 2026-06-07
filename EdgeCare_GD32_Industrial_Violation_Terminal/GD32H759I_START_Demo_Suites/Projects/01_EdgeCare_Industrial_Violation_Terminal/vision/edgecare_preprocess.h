/*!
    \file    edgecare_preprocess.h
    \brief   EdgeCare image preprocessing
*/

#ifndef EDGECARE_PREPROCESS_H
#define EDGECARE_PREPROCESS_H

#include <stdint.h>

typedef struct {
    uint8_t min_value;
    uint8_t max_value;
    uint32_t mean;
    uint32_t checksum;
    uint8_t center[4];
} edgecare_preprocess_gray96_stats_t;

void edgecare_preprocess_gray96_from_yuyv(const uint8_t *yuyv_frame,
                                          uint8_t *gray96,
                                          edgecare_preprocess_gray96_stats_t *stats);

#endif /* EDGECARE_PREPROCESS_H */
