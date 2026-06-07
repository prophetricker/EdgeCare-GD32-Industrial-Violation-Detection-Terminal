/*!
    \file    edgecare_log.h
    \brief   EdgeCare UART log helpers
*/

#ifndef EDGECARE_LOG_H
#define EDGECARE_LOG_H

#include <stdint.h>

void edgecare_log_status(uint32_t ts_ms,
                         const char *state_name,
                         uint8_t radar_triggered,
                         uint32_t infer_ms,
                         uint8_t confidence_percent,
                         uint8_t alarm_active,
                         uint32_t seq);

#endif /* EDGECARE_LOG_H */
