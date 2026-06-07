/*!
    \file    edgecare_log.c
    \brief   EdgeCare UART log helpers
*/

#include "edgecare_log.h"
#include "gd32h7xx.h"
#include "gd32h759i_start.h"
#include <stdio.h>

void edgecare_log_status(uint32_t ts_ms,
                         const char *state_name,
                         uint8_t radar_triggered,
                         uint32_t infer_ms,
                         uint8_t confidence_percent,
                         uint8_t alarm_active,
                         uint32_t seq)
{
    printf("[%lu] state=%s radar=%u infer_ms=%lu conf=%u.%02u alarm=%u seq=%lu\r\n",
           (unsigned long)ts_ms,
           state_name,
           radar_triggered,
           (unsigned long)infer_ms,
           confidence_percent / 100U,
           confidence_percent % 100U,
           alarm_active,
           (unsigned long)seq);

    while(RESET == usart_flag_get(EVAL_COM, USART_FLAG_TC)) {
    }
}
