#include "idt.h"

extern void kprint(const char* str);
extern void kprint_dec(uint32_t num);

static idt_entry_t idt_entries[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;
static isr_handler_t interrupt_handlers[IDT_ENTRIES];

/* Declaracoes externas dos stubs definidos em isr.asm */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

extern void isr128(void); /* Syscall int 0x80 */
extern void idt_flush(uint32_t ptr_addr);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low  = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel       = sel;
    idt_entries[num].always0   = 0;
    idt_entries[num].flags     = flags;
}

void register_interrupt_handler(uint8_t n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

/* Dispatcher para Excecoes da CPU */
void isr_handler(registers_t* regs) {
    if (interrupt_handlers[regs->int_no] != NULL) {
        interrupt_handlers[regs->int_no](regs);
    } else {
        kprint("\n====================================================\n");
        kprint("[CPU EXCEPTION] Falha ");
        kprint_dec(regs->int_no);
        kprint(" - Codigo: ");
        kprint_dec(regs->err_code);
        kprint("\n        EIP: 0x");
        char hex[9];
        for (int i = 7; i >= 0; i--) {
            uint8_t nibble = (regs->eip >> (i * 4)) & 0xF;
            hex[7 - i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        }
        hex[8] = '\0';
        kprint(hex);
        kprint("  CS: 0x");
        kprint_dec(regs->cs);
        kprint("\n====================================================\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }
}

/* Dispatcher para Interrupcoes de Hardware (IRQs) */
void irq_handler(registers_t* regs) {
    /* Envia End of Interrupt (EOI) para o PIC antes do handler,
     * permitindo que preempcao e troca de contexto nao bloqueiem novas IRQs */
    if (regs->int_no >= 40) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0xA0));
    }
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));

    /* Executa o handler registrado no driver, se existir */
    if (interrupt_handlers[regs->int_no] != NULL) {
        interrupt_handlers[regs->int_no](regs);
    }
}

/* Remapeia o PIC 8259 para vetores 32 (0x20) e 40 (0x28) */
static void pic_remap(void) {
    /* ICW1: inicia sequencia de inicializacao em cascata */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x11), "Nd"((uint16_t)0x20));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x11), "Nd"((uint16_t)0xA0));

    /* ICW2: Offset do Master (32 = 0x20) */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x21));
    /* ICW2: Offset do Slave (40 = 0x28) */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x28), "Nd"((uint16_t)0xA1));

    /* ICW3: Master possui Slave conectado na IRQ2 */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x04), "Nd"((uint16_t)0x21));
    /* ICW3: Identificador de cascata no Slave */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x02), "Nd"((uint16_t)0xA1));

    /* ICW4: Modo 8086/88 */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0x21));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x01), "Nd"((uint16_t)0xA1));

    /* Mascaras: permite IRQ0 (Timer) e IRQ1 (Teclado) */
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFC), "Nd"((uint16_t)0x21));
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFF), "Nd"((uint16_t)0xA1));
}

void idt_init(void) {
    /* 1. Remapeia o PIC para que as IRQs nao colidam com as excecoes da CPU (0x08..0x0F) */
    pic_remap();

    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_entries[i].base_low  = 0;
        idt_entries[i].base_high = 0;
        idt_entries[i].sel       = 0;
        idt_entries[i].always0   = 0;
        idt_entries[i].flags     = 0;
        interrupt_handlers[i]    = NULL;
    }

    /* 0x8E = 10001110b (Presente, Ring 0, 32-bit Interrupt Gate) */
    idt_set_gate(0,  (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    /* Mapeamento de Hardware IRQs (32..47) */
    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    /* 0xEF = 11101111b (Presente, Ring 3, 32-bit Interrupt Gate para Syscalls) */
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEF);

    /* Instala a IDT */
    idt_flush((uint32_t)&idt_ptr);
}
