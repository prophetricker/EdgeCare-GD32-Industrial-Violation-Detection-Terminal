/*!
    \file    edgecare_infer.c
    \brief   EdgeCare model inference boundary
*/

#include "edgecare_infer.h"
#include "../board/board_config.h"

uint8_t edgecare_infer(const uint8_t *input, edgecare_infer_result_t *result)
{
    uint32_t i;
    uint32_t sum = 0U;
    uint8_t min_value = 0xFFU;
    uint8_t max_value = 0U;
    uint8_t value;

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

    result->mean = (uint8_t)(sum / MODEL_INPUT_BYTES);
    result->contrast = (uint8_t)(max_value - min_value);

    /*
       Placeholder until the PC-trained model is ready. It proves the model interface
       and gives a scene-sensitive number, but it is not a real intrusion classifier.
    */
    result->confidence_percent = (result->contrast > 120U) ? 90U : (uint8_t)((result->contrast * 3U) / 4U);
    result->intrusion = (result->confidence_percent >= 75U) ? 1U : 0U;

    return 1U;
}
