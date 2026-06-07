/*!
    \file    bsp_radar_ld2410.h
    \brief   LD2410 digital OUT input for EdgeCare
*/

#ifndef BSP_RADAR_LD2410_H
#define BSP_RADAR_LD2410_H

#include <stdint.h>

void bsp_radar_ld2410_init(void);
uint8_t bsp_radar_ld2410_is_triggered(void);

#endif /* BSP_RADAR_LD2410_H */
