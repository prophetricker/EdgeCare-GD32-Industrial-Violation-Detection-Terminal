/*!
    \file    main.c
    \brief   EdgeCare state-machine bring-up for GD32H759

    Current bring-up scope:
      - cache, SysTick and USART0 stdio initialization
      - EdgeCare application state machine loop
*/

#include "gd32h7xx.h"
#include "gd32h759i_start.h"
#include "systick.h"
#include "app/edgecare_app.h"
#include "board/board_config.h"
#include <stdio.h>

static void cache_enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

int main(void)
{
    cache_enable();
    systick_config();
    gd_eval_com_init(EVAL_COM);
    edgecare_app_init();

    while(1) {
        edgecare_app_step();
        delay_1ms(EDGECARE_LOG_PERIOD_MS);
        edgecare_app_advance_time(EDGECARE_LOG_PERIOD_MS);
    }
}

#ifdef GD_ECLIPSE_GCC
int __io_putchar(int ch)
{
    usart_data_transmit(EVAL_COM, (uint8_t)ch);
    while(RESET == usart_flag_get(EVAL_COM, USART_FLAG_TBE)) {
    }
    return ch;
}
#else
int fputc(int ch, FILE *f)
{
    (void)f;
    usart_data_transmit(EVAL_COM, (uint8_t)ch);
    while(RESET == usart_flag_get(EVAL_COM, USART_FLAG_TBE)) {
    }
    return ch;
}
#endif
