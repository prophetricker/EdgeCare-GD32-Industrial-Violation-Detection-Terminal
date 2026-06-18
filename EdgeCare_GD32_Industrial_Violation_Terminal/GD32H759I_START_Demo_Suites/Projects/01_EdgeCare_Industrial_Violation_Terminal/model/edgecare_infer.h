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
    uint32_t inference_time_ms;
    const char *model_name;
    uint8_t is_placeholder;
} edgecare_infer_result_t;

typedef struct {
    uint8_t intrusion_votes;
    uint8_t safe_votes;
    uint8_t alarm_active;
} edgecare_vote_state_t;

uint8_t edgecare_infer(const uint8_t *input, edgecare_infer_result_t *result);
void edgecare_vote_reset(edgecare_vote_state_t *state);
uint8_t edgecare_vote_update(edgecare_vote_state_t *state, const edgecare_infer_result_t *result);

#endif /* EDGECARE_INFER_H */
