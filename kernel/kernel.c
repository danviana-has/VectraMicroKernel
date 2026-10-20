#include "types.h"
#include "gdt.h"
#include "idt.h"
#include "drivers.h"
#include "multiboot.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "tss.h"
#include "task.h"
#include "ipc.h"
#include "timer.h"
#include "vfs.h"
#include "syscall.h"
#include "loader.h"
#include "netif_e1000.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

struct sysinfo_t lib_sysinfo = {
    .ser_ioport = 0x3F8, /* COM1 (padrao x86) */
    .ser_base   = 0
};

void (*reset_handler)(void) = NULL;

void console_add_input_driver(struct console_input_driver *driver) { (void)driver; }
void console_add_output_driver(struct console_output_driver *driver) { (void)driver; }

void udelay(unsigned int us) {
    for (volatile unsigned int i = 0; i < us * 10; i++) __asm__ volatile ("nop");
}

void mdelay(unsigned int ms) {
    for (volatile unsigned int i = 0; i < ms * 10000; i++) __asm__ volatile ("nop");
}

uint32_t sys_now(void) {
    return timer_get_ticks() * 10;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    (void)format;
    if (size > 0) str[0] = '\0';
    return 0;
}

/* Utilitario de impressao usando diretamente os drivers de video e serial de ./drivers/ */
void kprint(const char* str) {
    while (*str) {
        if (*str == '\n') {
            video_console_putchar('\r');
            int timeout = 10000;
            while ((inb(0x3F8 + 5) & 0x20) == 0 && --timeout > 0);
            if (timeout > 0) {
                outb('\r', 0x3F8);
            }
        }
        video_console_putchar((unsigned char)*str);
        int timeout = 10000;
        while ((inb(0x3F8 + 5) & 0x20) == 0 && --timeout > 0);
        if (timeout > 0) {
            outb((unsigned char)*str, 0x3F8);
        }
        str++;
    }
}

void kprint_dec(uint32_t num) {
    char buf[12];
    int i = 10;
    buf[11] = '\0';

    if (num == 0) {
        kprint("0");
        return;
    }

    while (num > 0) {
        buf[i--] = '0' + (num % 10);
        num /= 10;
    }
    kprint(&buf[i + 1]);
}

/*
 * Ponto de entrada C do Vectra OS (chamado a partir de boot/boot.asm)
 */
void kmain(uint32_t magic, uint32_t mbi_addr) {
    multiboot_info_t* mbi = (multiboot_info_t*)mbi_addr;

    /* 1. Verificacao do Magic Number do Multiboot */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    /* 2. Inicializacao das tabelas de segmentacao e interrupcoes */
    gdt_init();
    idt_init();

    /* 3. Inicializacao dos Drivers de Ring 0 a partir da pasta ./drivers/ */
    video_console_init();   /* Console de video (drivers/video/video.c & vga.c) */
    video_console_clear();  /* Limpeza da tela VGA (drivers/video/video.c) */
    serial_init();          /* UART serial COM1 (drivers/serial.c) */
    keyboard_init();        /* Teclado PS/2 (drivers/keyboard.c) */
    speaker_tone(1200, 30); /* Beep de boot via PIT canal 2 (drivers/speaker.c) */

    /* 4. Banner Inicial */
    kprint("====================================================\n");
    kprint("       Vectra OS v1.0 - Microkernel Core x86        \n");
    kprint("====================================================\n");
    kprint("[OK] Multiboot Header validado (Magic: 0x2BADB002)\n");
    kprint("[OK] Global Descriptor Table (GDT) carregada\n");
    kprint("[OK] Interrupt Descriptor Table (IDT) configurada\n");
    kprint("[OK] Drivers Ring 0 carregados de ./drivers/\n");

    /* 5. Modulo 1.2: Gerenciamento de Memoria Real */
    kprint("[PMM] Inicializando gerenciador de memoria fisica (Bitmap)...\n");
    pmm_init(mbi);
    kprint("      RAM Total Detectada: ");
    kprint_dec(pmm_get_total_memory() / (1024 * 1024));
    kprint(" MiB\n");

    kprint("[VMM] Inicializando paginacao x86 de 2 niveis (CR3/CR0)...\n");
    vmm_init();
    kprint("      [OK] Paginacao ativada com Identity Mapping (32 MiB)\n");

    kprint("[HEAP] Inicializando alocador dinamico do kernel (kmalloc/kfree)...\n");
    heap_init(0x01000000, 4 * 1024 * 1024); /* 4 MiB de Heap inicial */
    kprint("       [OK] Heap de 4 MiB mapeado com sucesso\n");

    /* 6. Modulo 1.3: TSS e Escalonador */
    kprint("[TSS] Configurando Task State Segment...\n");
    task_init();
    kprint("      [OK] TSS instalado na GDT (Seletor 0x28) e TR carregado\n");

    /* 7. Modulo 2.1: Syscalls, VFS e Carregamento de Aplicativos Externos */
    kprint("[SYSCALL] Registrando despachante int 0x80 (DPL=3)...\n");
    syscall_init();
    kprint("          [OK] Syscalls habilitadas para chamadas em Ring 3\n");

    kprint("[VFS] Montando sistema de arquivos a partir dos modulos da ISO...\n");
    vfs_init(mbi);
    kprint("      [OK] Modulos VFS carregados: ");
    kprint_dec(vfs_get_count());
    kprint("\n");
    vfs_list_files();

    /* 8. Modulo de Rede TCP/IP: Driver Intel e1000 e pilha lwIP */
    net_init();

    kprint("[DHCP] Solicitando endereco IP da rede...\n");
    for (int w = 0; w < 150 && !net_has_ip(); w++) {
        net_poll();
        mdelay(10);
    }
    if (net_has_ip()) {
        kprint("       [OK] IP obtido com sucesso: ");
        kprint(net_get_ip_str());
        kprint(" (Conectado)\n");
    } else {
        kprint("       [AVISO] DHCP ativo em segundo plano (use 'netinfo')\n");
    }

    kprint("[LOADER] Carregando shell externo (/apps/shell.bin) em Ring 3...\n");
    int32_t shell_pid = bin_load_and_run("shell.bin", false);
    if (shell_pid < 0) {
        kprint("[PANIC] Nao foi possivel iniciar o shell externo!\n");
    } else {
        kprint("         [OK] Shell iniciado em Ring 3 sob PID ");
        kprint_dec(shell_pid);
        kprint("\n");
    }

    kprint("[TIMER] Inicializando PIT (100 Hz) para preempcao multitarefa...\n");
    timer_init(100);
    kprint("        [OK] IRQ 0 mapeada para preempcao preemptiva\n");

    kprint("[VECTRA] Microkernel operacional. Cedendo CPU para o espaco de usuario.\n");

    /* 9. Habilita as interrupcoes da CPU para disparar a execucao do Shell em Ring 3 */
    __asm__ volatile ("sti");

    /* 10. Loop de ociosidade da thread principal (Idle) */
    for (;;) {
        net_poll();
        schedule();
        __asm__ volatile ("sti; hlt");
    }
}
