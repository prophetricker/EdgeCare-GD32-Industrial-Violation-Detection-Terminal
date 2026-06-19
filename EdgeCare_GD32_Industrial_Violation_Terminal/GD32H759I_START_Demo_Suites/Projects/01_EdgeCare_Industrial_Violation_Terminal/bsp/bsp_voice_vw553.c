/*!
    \file    bsp_voice_vw553.c
    \brief   UART command output to the GD32VW553 voice warning board
*/

#include "bsp_voice_vw553.h"
#include "../board/board_config.h"

#include <stdint.h>

static uint8_t bsp_voice_vw553_wait_flag(uint32_t flag)
{
    uint32_t timeout = VOICE_VW553_TX_TIMEOUT;

    while((RESET == usart_flag_get(VOICE_VW553_USART, flag)) && (timeout > 0U)) {
        timeout--;
    }

    return (timeout > 0U) ? 1U : 0U;
}

void bsp_voice_vw553_init(void)
{
    rcu_periph_clock_enable(VOICE_VW553_GPIO_RCU);
    rcu_periph_clock_enable(VOICE_VW553_USART_RCU);

    gpio_af_set(VOICE_VW553_GPIO_PORT,
                VOICE_VW553_GPIO_AF,
                VOICE_VW553_TX_PIN | VOICE_VW553_RX_PIN);

    gpio_mode_set(VOICE_VW553_GPIO_PORT,
                  GPIO_MODE_AF,
                  GPIO_PUPD_PULLUP,
                  VOICE_VW553_TX_PIN | VOICE_VW553_RX_PIN);
    gpio_output_options_set(VOICE_VW553_GPIO_PORT,
                            GPIO_OTYPE_PP,
                            GPIO_OSPEED_60MHZ,
                            VOICE_VW553_TX_PIN | VOICE_VW553_RX_PIN);

    usart_deinit(VOICE_VW553_USART);
    usart_baudrate_set(VOICE_VW553_USART, VOICE_VW553_BAUDRATE);
    usart_receive_config(VOICE_VW553_USART, USART_RECEIVE_ENABLE);
    usart_transmit_config(VOICE_VW553_USART, USART_TRANSMIT_ENABLE);
    usart_enable(VOICE_VW553_USART);
}

void bsp_voice_vw553_send_danger(void)
{
    const char *command = VOICE_VW553_DANGER_COMMAND;

    while('\0' != *command) {
        if(!bsp_voice_vw553_wait_flag(USART_FLAG_TBE)) {
            return;
        }

        usart_data_transmit(VOICE_VW553_USART, (uint8_t)*command);
        command++;
    }

    (void)bsp_voice_vw553_wait_flag(USART_FLAG_TC);
}
