/*!
    \file    bsp_alarm.h
    \brief   Active-low alarm output for EdgeCare
*/

#ifndef BSP_ALARM_H
#define BSP_ALARM_H

#include <stdint.h>

void bsp_alarm_init(void);
void bsp_alarm_set_active(uint8_t active);

#endif /* BSP_ALARM_H */
