#ifndef _LIBPAYLOAD_H
#define _LIBPAYLOAD_H

#include <libpayload-config.h>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char        s8;
typedef signed short       s16;
typedef signed int         s32;
typedef signed long long   s64;

typedef u32 pcidev_t;
typedef u32 size_t;

#define NULL ((void*)0)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define phys_to_virt(x) ((void*)(uintptr_t)(x))
typedef u32 uintptr_t;

/* I/O em Portas x86 */
static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(u8 val, u16 port) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 inw(u16 port) {
    u16 ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(u16 val, u16 port) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port) {
    u32 ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(u32 val, u16 port) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* Acesso a Memoria Mapeada (MMIO) */
static inline u8 readb(const volatile void *addr) {
    return *(const volatile u8 *)addr;
}

static inline void writeb(u8 val, volatile void *addr) {
    *(volatile u8 *)addr = val;
}

/* Informacoes de Sistema usadas pelos drivers de console */
struct sysinfo_t {
    unsigned int ser_ioport;
    unsigned long ser_base;
};

extern struct sysinfo_t lib_sysinfo;

/* Estruturas de console de entrada e saida */
struct console_input_driver {
    int (*havekey)(void);
    int (*getchar)(void);
};

struct console_output_driver {
    void (*putchar)(unsigned int c);
};

void console_add_input_driver(struct console_input_driver *driver);
void console_add_output_driver(struct console_output_driver *driver);

/* Prototipos de funcoes basicas de string e tempo */
int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
void mdelay(unsigned int ms);
void udelay(unsigned int us);
int snprintf(char *str, size_t size, const char *format, ...);
void *memset(void *s, int c, size_t n);

/* Definicoes RTC / NVRAM (MC146818) */
#define NVRAM_RTC_SECONDS       0x00
#define NVRAM_RTC_MINUTES       0x02
#define NVRAM_RTC_HOURS         0x04
#define NVRAM_RTC_DAY           0x07
#define NVRAM_RTC_MONTH         0x08
#define NVRAM_RTC_YEAR          0x09
#define NVRAM_RTC_FREQ_SELECT   0x0A
#define NVRAM_RTC_UIP           0x80

static inline u8 bcd2dec(u8 val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

/* Prototipos do driver serial (drivers/serial.c) */
int serial_havechar(void);
int serial_getchar(void);
void serial_putchar(unsigned int c);

/* Prototipos do driver PCI (drivers/pci.c) */
int pci_find_device(unsigned short vid, unsigned short did, pcidev_t *dev);
u32 pci_read_resource(pcidev_t dev, int bar);

extern void (*reset_handler)(void);

#endif /* _LIBPAYLOAD_H */
