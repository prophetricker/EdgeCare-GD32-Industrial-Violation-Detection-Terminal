/*!
    \file    edgecare_infer.h
    \brief   EdgeCare model inference boundary
*/

#ifndef EDGECARE_INFER_H
#define EDGECARE_INFER_H

#include <stdint.h>

typedef struct {
    uint8_t intrusion;
    uint8_t confidence_percent;
    uint8_t mean;
    uint8_t contrast;
} edgecare_infer_result_t;

uint8_t edgecare_infer(const uint8_t *input, edgecare_infer_result_t *result);

#endif /* EDGECARE_INFER_H */
