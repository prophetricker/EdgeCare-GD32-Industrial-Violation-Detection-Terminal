/*!
    \file    bsp_alarm.c
    \brief   Active-low alarm output for EdgeCare
*/

#include "bsp_alarm.h"
#include "../board/board_config.h"

void bsp_alarm_init(void)
{
    rcu_periph_clock_enable(ALARM_GPIO_RCU);

    gpio_mode_set(ALARM_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ALARM_GPIO_PIN);
    gpio_output_options_set(ALARM_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, ALARM_GPIO_PIN);

    /* The alarm module is active-low, so drive high to keep it off at boot. */
    gpio_bit_set(ALARM_GPIO_PORT, ALARM_GPIO_PIN);
}

void bsp_alarm_set_active(uint8_t active)
{
    if(active) {
        gpio_bit_reset(ALARM_GPIO_PORT, ALARM_GPIO_PIN);
    } else {
        gpio_bit_set(ALARM_GPIO_PORT, ALARM_GPIO_PIN);
    }
}
