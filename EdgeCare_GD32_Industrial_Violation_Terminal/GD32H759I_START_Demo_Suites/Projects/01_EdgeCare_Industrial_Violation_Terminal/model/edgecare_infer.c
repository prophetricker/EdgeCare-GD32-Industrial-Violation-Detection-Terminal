/*!
    \file    edgecare_infer.c
    \brief   EdgeCare model inference boundary
*/

#include "edgecare_infer.h"
#include "edgecare_model_baseline.h"
#include "../board/board_config.h"

#define EDGECARE_INFER_THRESHOLD_PERCENT ((uint8_t)((EDGECARE_BASELINE_THRESHOLD_Q15 * 100 + 16384) / 32768))
#define EDGECARE_VOTE_INTRUSION_REQUIRED 2U
#define EDGECARE_VOTE_SAFE_RELEASE_REQUIRED 2U
#define EDGECARE_VOTE_WINDOW_FRAMES 3U
#define EDGECARE_Q15_ONE 32768

static int32_t clamp_int32(int64_t value)
{
    if(value > 2147483647LL) {
        return 2147483647L;
    }
    if(value < -2147483647LL - 1LL) {
        return (-2147483647L - 1L);
    }
    return (int32_t)value;
}

static uint8_t score_to_confidence_percent(int32_t score_q15)
{
    int64_t shifted = (int64_t)score_q15 - (int64_t)EDGECARE_BASELINE_LOGIT_THRESHOLD_Q15;

    if(shifted <= -32768LL) {
        return 0U;
    }
    if(shifted >= 32768LL) {
        return 100U;
    }

    return (uint8_t)((shifted + 32768LL) * 100LL / 65536LL);
}

static void edgecare_baseline_extract_features_q15(const uint8_t *input, int32_t *features_q15, uint8_t *mean_out, uint8_t *contrast_out)
{
    uint32_t i;
    uint32_t sum = 0U;
    uint8_t min_value = 0xFFU;
    uint8_t max_value = 0U;
    uint8_t value;
    uint32_t grid_y;
    uint32_t grid_x;

    for(i = 0U; i < MODEL_INPUT_BYTES; i++) {
        value = input[i];
        sum += value;
        if(value < min_value) {
            min_value = value;
        }
        if(value > max_value) {
            max_value = value;
        }
    }

    *mean_out = (uint8_t)(sum / MODEL_INPUT_BYTES);
    *contrast_out = (uint8_t)(max_value - min_value);

    features_q15[0] = EDGECARE_Q15_ONE;
    features_q15[1] = (int32_t)(((uint64_t)sum * (uint64_t)EDGECARE_Q15_ONE) / (255ULL * MODEL_INPUT_BYTES));
    features_q15[2] = (int32_t)(((uint32_t)min_value * (uint32_t)EDGECARE_Q15_ONE) / 255U);
    features_q15[3] = (int32_t)(((uint32_t)max_value * (uint32_t)EDGECARE_Q15_ONE) / 255U);
    features_q15[4] = (int32_t)((((uint32_t)max_value - (uint32_t)min_value) * (uint32_t)EDGECARE_Q15_ONE) / 255U);

    for(grid_y = 0U; grid_y < EDGECARE_BASELINE_GRID; grid_y++) {
        uint32_t y0 = (grid_y * MODEL_INPUT_HEIGHT) / EDGECARE_BASELINE_GRID;
        uint32_t y1 = ((grid_y + 1U) * MODEL_INPUT_HEIGHT) / EDGECARE_BASELINE_GRID;
        for(grid_x = 0U; grid_x < EDGECARE_BASELINE_GRID; grid_x++) {
            uint32_t x0 = (grid_x * MODEL_INPUT_WIDTH) / EDGECARE_BASELINE_GRID;
            uint32_t x1 = ((grid_x + 1U) * MODEL_INPUT_WIDTH) / EDGECARE_BASELINE_GRID;
            uint32_t grid_sum = 0U;
            uint32_t count = 0U;
            uint32_t y;
            uint32_t x;
            uint32_t feature_index = 5U + grid_y * EDGECARE_BASELINE_GRID + grid_x;

            for(y = y0; y < y1; y++) {
                uint32_t row = y * MODEL_INPUT_WIDTH;
                for(x = x0; x < x1; x++) {
                    grid_sum += input[row + x];
                    count++;
                }
            }
            features_q15[feature_index] = (int32_t)(((uint64_t)grid_sum * (uint64_t)EDGECARE_Q15_ONE) / (255ULL * count));
        }
    }
}

uint8_t edgecare_infer(const uint8_t *input, edgecare_infer_result_t *result)
{
    uint32_t i;
    int64_t dot_q30 = 0LL;
    int32_t score_q15;
    int32_t features_q15[EDGECARE_BASELINE_FEATURE_COUNT];

    if((input == 0) || (result == 0)) {
        return 0U;
    }

    edgecare_baseline_extract_features_q15(input, features_q15, &result->mean, &result->contrast);

    for(i = 0U; i < EDGECARE_BASELINE_FEATURE_COUNT; i++) {
        dot_q30 += (int64_t)features_q15[i] * (int64_t)edgecare_baseline_weights_q15[i];
    }

    score_q15 = clamp_int32((dot_q30 + 16384LL) / 32768LL);
    result->confidence_percent = score_to_confidence_percent(score_q15);
    result->intrusion = (score_q15 >= EDGECARE_BASELINE_LOGIT_THRESHOLD_Q15) ? 1U : 0U;
    result->inference_time_ms = 1U;
    result->model_name = EDGECARE_BASELINE_MODEL_NAME;
    result->is_placeholder = 0U;

    return 1U;
}

void edgecare_vote_reset(edgecare_vote_state_t *state)
{
    state->intrusion_votes = 0U;
    state->safe_votes = EDGECARE_VOTE_SAFE_RELEASE_REQUIRED;
    state->alarm_active = 0U;
}

uint8_t edgecare_vote_update(edgecare_vote_state_t *state, const edgecare_infer_result_t *result)
{
    if((result->intrusion != 0U) && (result->confidence_percent >= EDGECARE_INFER_THRESHOLD_PERCENT)) {
        if(state->intrusion_votes < EDGECARE_VOTE_WINDOW_FRAMES) {
            state->intrusion_votes++;
        }
        state->safe_votes = 0U;
    } else {
        if(state->safe_votes < EDGECARE_VOTE_SAFE_RELEASE_REQUIRED) {
            state->safe_votes++;
        }
        if(state->intrusion_votes > 0U) {
            state->intrusion_votes--;
        }
    }

    if(state->intrusion_votes >= EDGECARE_VOTE_INTRUSION_REQUIRED) {
        state->alarm_active = 1U;
    } else if(state->safe_votes >= EDGECARE_VOTE_SAFE_RELEASE_REQUIRED) {
        state->alarm_active = 0U;
    }

    return state->alarm_active;
}
