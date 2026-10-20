#ifndef VECTRA_TIMER_H
#define VECTRA_TIMER_H

#include "types.h"
#include "idt.h"

void timer_init(uint32_t frequency_hz);
uint32_t timer_get_ticks(void);

#endif /* VECTRA_TIMER_H */
