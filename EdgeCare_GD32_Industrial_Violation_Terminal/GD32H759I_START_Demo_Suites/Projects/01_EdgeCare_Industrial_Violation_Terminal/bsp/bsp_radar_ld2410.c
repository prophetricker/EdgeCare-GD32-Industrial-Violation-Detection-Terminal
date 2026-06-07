/*!
    \file    bsp_radar_ld2410.c
    \brief   LD2410 digital OUT input for EdgeCare
*/

#include "bsp_radar_ld2410.h"
#include "../board/board_config.h"

void bsp_radar_ld2410_init(void)
{
    rcu_periph_clock_enable(RADAR_GPIO_RCU);

    gpio_mode_set(RADAR_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, RADAR_GPIO_PIN);
}

uint8_t bsp_radar_ld2410_is_triggered(void)
{
    FlagStatus level = gpio_input_bit_get(RADAR_GPIO_PORT, RADAR_GPIO_PIN);

#if RADAR_ACTIVE_HIGH
    return (SET == level) ? 1U : 0U;
#else
    return (RESET == level) ? 1U : 0U;
#endif
}
