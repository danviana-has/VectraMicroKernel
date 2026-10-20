#include "timer.h"
#include "task.h"
#include <libpayload.h>

static volatile uint32_t timer_ticks = 0;

extern void net_poll(void);

static void timer_callback(registers_t* regs) {
    (void)regs;
    timer_ticks++;

    net_poll();

    /* Preempcao multitarefa do escalonador Round-Robin */
    schedule();
}

void timer_init(uint32_t frequency_hz) {
    if (frequency_hz == 0) frequency_hz = 100;

    uint32_t divisor = 1193180 / frequency_hz;

    /* Envia comando para o PIT 8254: Canal 0, Modo 3 (onda quadrada), Acesso LSB/MSB */
    outb(0x36, 0x43);
    outb((uint8_t)(divisor & 0xFF), 0x40);
    outb((uint8_t)((divisor >> 8) & 0xFF), 0x40);

    /* Registra o callback no vetor de interrupcao 32 (IRQ 0) */
    register_interrupt_handler(32, timer_callback);
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}
