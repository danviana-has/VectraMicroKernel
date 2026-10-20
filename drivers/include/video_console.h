#ifndef _VIDEO_CONSOLE_H
#define _VIDEO_CONSOLE_H

#include <libpayload.h>

struct video_console {
    int (*init)(void);
    void (*putc)(u8 row, u8 col, unsigned int c);
    void (*clear)(void);
    void (*scroll_up)(void);
    void (*get_cursor)(unsigned int *x, unsigned int *y, unsigned int *en);
    void (*set_cursor)(unsigned int x, unsigned int y);
    void (*enable_cursor)(int state);
    int columns;
    int rows;
};

int video_console_init(void);
void video_console_putchar(unsigned int ch);
void video_console_clear(void);
void video_console_putc(u8 row, u8 col, unsigned int ch);
void video_console_set_cursor(unsigned int x, unsigned int y);
void video_console_cursor_enable(int state);

#endif
