#ifndef VECTRA_DRIVERS_H
#define VECTRA_DRIVERS_H

#include "types.h"
#include <libpayload.h>
#include <video_console.h>
#include <pci.h>

/*
 * ==============================================================================
 * Vectra OS - Headers e Prototipos dos Drivers de Ring 0 (./drivers/)
 *
 * Utiliza estritamente os drivers ja implementados no projeto:
 * - drivers/video/video.c & drivers/video/vga.c (Console de Video VGA)
 * - drivers/serial.c (Porta Serial UART / COM1)
 * - drivers/keyboard.c (Teclado PS/2 i8042)
 * - drivers/speaker.c (PC Speaker e PIT Canal 2)
 * - drivers/pci.c (Barramento PCI)
 * ==============================================================================
 */

/* Console de Video (drivers/video/video.c e drivers/video/vga.c) */
int  video_console_init(void);
void video_console_putchar(unsigned int ch);
void video_console_clear(void);
void video_console_putc(u8 row, u8 col, unsigned int ch);
void video_console_set_cursor(unsigned int x, unsigned int y);
void video_console_cursor_enable(int state);

/* Porta Serial (drivers/serial.c) */
void serial_init(void);
void serial_putchar(unsigned int c);
int  serial_havechar(void);
int  serial_getchar(void);
void serial_clear(void);

/* Teclado PS/2 (drivers/keyboard.c) */
void keyboard_init(void);
int  keyboard_havechar(void);
int  keyboard_getchar(void);
int  keyboard_set_layout(char *country);

/* PC Speaker / PIT Canal 2 (drivers/speaker.c) */
void speaker_enable(u16 freq);
void speaker_disable(void);
void speaker_tone(u16 freq, unsigned int duration);

/* Barramento PCI (drivers/pci.c) */
u8   pci_read_config8(pcidev_t device, u16 reg);
u16  pci_read_config16(pcidev_t device, u16 reg);
u32  pci_read_config32(pcidev_t device, u16 reg);

/* NVRAM / CMOS (drivers/nvram.c) */
u8   nvram_read(u8 addr);
void nvram_write(u8 val, u8 addr);

/* Funcoes utilitarias de impressao do Kernel usando os drivers acima */
void kprint(const char* str);
void kprint_dec(uint32_t num);

#endif /* VECTRA_DRIVERS_H */
