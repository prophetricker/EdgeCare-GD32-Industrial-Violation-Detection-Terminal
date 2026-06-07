/*!
    \file    edgecare_app.h
    \brief   EdgeCare application state machine
*/

#ifndef EDGECARE_APP_H
#define EDGECARE_APP_H

#include <stdint.h>

void edgecare_app_init(void);
void edgecare_app_step(void);
void edgecare_app_advance_time(uint32_t elapsed_ms);

#endif /* EDGECARE_APP_H */
