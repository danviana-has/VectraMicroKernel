# ==============================================================================
# Vectra OS - Makefile
# Target: kernel.akl & vectra-os.iso (x86 Bare-Metal / Multiboot 1)
# ==============================================================================

CC      := gcc
AS      := nasm
LD      := ld
XORRISO := xorriso
GRUB    := grub-mkrescue

# Flags de compilacao freestanding 32-bit (sem libc)
CFLAGS  := -m32 -std=gnu99 -ffreestanding -nostdlib -nostdinc -fno-builtin \
           -fno-stack-protector -fno-pie -no-pie -Wall -Wextra -O2 \
           -I./kernel/include -I./drivers -I./drivers/include \
           -I./net/include -I./net/lwip-STABLE-2_1_3_RELEASE/src/include

USER_CFLAGS := -m32 -std=gnu99 -ffreestanding -nostdlib -nostdinc -fno-builtin \
               -fno-stack-protector -fno-pie -no-pie -Wall -Wextra -O2 \
               -I./user

ASFLAGS := -f elf32

LDFLAGS := -m elf_i386 -T boot/linker.ld -nostdlib
USER_LDFLAGS := -m elf_i386 -T user/user.ld --oformat binary -nostdlib

BUILD_DIR  := build
KERNEL_BIN := kernel.akl
ISO_IMAGE  := vectra-os.iso

# Fontes e Objetos do Boot e Microkernel Core
KERNEL_C_SRCS := $(wildcard kernel/*.c)
KERNEL_OBJS   := $(patsubst kernel/%.c, $(BUILD_DIR)/kernel/%.o, $(KERNEL_C_SRCS)) \
                 $(BUILD_DIR)/boot/boot.o \
                 $(BUILD_DIR)/kernel/isr.o \
                 $(BUILD_DIR)/kernel/switch.o

# Fontes e Objetos dos Drivers de Ring 0 (reutilizando estritamente ./drivers/)
DRIVER_C_SRCS := drivers/keyboard.c \
                 drivers/serial.c \
                 drivers/speaker.c \
                 drivers/pci.c \
                 drivers/nvram.c \
                 drivers/video/video.c \
                 drivers/video/vga.c

DRIVER_OBJS   := $(patsubst %.c, $(BUILD_DIR)/%.o, $(DRIVER_C_SRCS))

# Fontes e Objetos da Pilha TCP/IP lwIP e Driver de Rede
NET_C_SRCS := net/netif_e1000.c \
              net/http_client.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/init.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/def.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/stats.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/dns.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/inet_chksum.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/ip.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/mem.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/memp.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/netif.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/pbuf.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/raw.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/sys.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/tcp.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/tcp_in.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/tcp_out.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/timeouts.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/udp.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/ipv4/dhcp.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/ipv4/etharp.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/ipv4/icmp.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/ipv4/ip4.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/ipv4/ip4_addr.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/core/ipv4/ip4_frag.c \
              net/lwip-STABLE-2_1_3_RELEASE/src/netif/ethernet.c

NET_OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(NET_C_SRCS))

ALL_OBJS := $(KERNEL_OBJS) $(DRIVER_OBJS) $(NET_OBJS)

# Binarios de Usuario (Ring 3)
USER_APPS := $(BUILD_DIR)/user/shell.bin $(BUILD_DIR)/user/hello.bin $(BUILD_DIR)/user/downloadfile.bin $(BUILD_DIR)/user/notepad.bin

.PHONY: all iso clean apps

all: $(KERNEL_BIN) $(USER_APPS)

# Assembly do Bootloader Multiboot
$(BUILD_DIR)/boot/boot.o: boot/boot.asm
	@mkdir -p $(dir $@)
	@echo " [AS]  $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Stubs de Interrupcoes (ISRs/IRQs)
$(BUILD_DIR)/kernel/isr.o: kernel/isr.asm
	@mkdir -p $(dir $@)
	@echo " [AS]  $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Context Switch em Assembly
$(BUILD_DIR)/kernel/switch.o: kernel/switch.asm
	@mkdir -p $(dir $@)
	@echo " [AS]  $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Modulos C do Kernel Core
$(BUILD_DIR)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	@echo " [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Compilacao dos drivers pre-existentes em ./drivers/
$(BUILD_DIR)/drivers/%.o: drivers/%.c
	@mkdir -p $(dir $@)
	@echo " [CC]  $< (Driver de Ring 0)"
	@$(CC) $(CFLAGS) -c $< -o $@

# Compilacao da pilha de rede lwIP
$(BUILD_DIR)/net/%.o: net/%.c
	@mkdir -p $(dir $@)
	@echo " [CC]  $< (Pilha TCP/IP lwIP)"
	@$(CC) $(CFLAGS) -c $< -o $@

# Ligacao final gerando kernel.akl (ELF32)
$(KERNEL_BIN): $(ALL_OBJS)
	@echo " [LD]  $@ (Formato ELF32 / Multiboot)"
	@$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)
	@echo " [OK]  Kernel $(KERNEL_BIN) gerado com sucesso!"

# Regras de construcao de executaveis Ring 3 (.bin)
$(BUILD_DIR)/user/crt0.o: user/crt0.asm
	@mkdir -p $(dir $@)
	@echo " [AS]  $< (User crt0)"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/user/%.o: user/%.c
	@mkdir -p $(dir $@)
	@echo " [CC]  $< (Ring 3 App)"
	@$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/user/shell.bin: $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/shell.o user/user.ld
	@mkdir -p $(dir $@)
	@echo " [LD]  $@ (Flat binary Ring 3)"
	@$(LD) $(USER_LDFLAGS) $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/shell.o -o $@

$(BUILD_DIR)/user/hello.bin: $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/hello.o user/user.ld
	@mkdir -p $(dir $@)
	@echo " [LD]  $@ (Flat binary Ring 3)"
	@$(LD) $(USER_LDFLAGS) $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/hello.o -o $@

$(BUILD_DIR)/user/downloadfile.bin: $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/downloadfile.o user/user.ld
	@mkdir -p $(dir $@)
	@echo " [LD]  $@ (Flat binary Ring 3)"
	@$(LD) $(USER_LDFLAGS) $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/downloadfile.o -o $@

$(BUILD_DIR)/user/notepad.bin: $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/notepad.o user/user.ld
	@mkdir -p $(dir $@)
	@echo " [LD]  $@ (Flat binary Ring 3)"
	@$(LD) $(USER_LDFLAGS) $(BUILD_DIR)/user/crt0.o $(BUILD_DIR)/user/notepad.o -o $@

apps: $(USER_APPS)

# Geracao da ISO bootavel com GRUB e modulos no VFS
iso: $(KERNEL_BIN) $(USER_APPS)
	@echo " [ISO] Preparando imagem ISO bootavel com modulos VFS..."
	@mkdir -p isodir/boot/grub isodir/apps
	@cp $(KERNEL_BIN) isodir/boot/$(KERNEL_BIN)
	@cp $(BUILD_DIR)/user/shell.bin isodir/apps/shell.bin
	@cp $(BUILD_DIR)/user/hello.bin isodir/apps/hello.bin
	@cp $(BUILD_DIR)/user/downloadfile.bin isodir/apps/downloadfile.bin
	@cp $(BUILD_DIR)/user/notepad.bin isodir/apps/notepad.bin
	@rm -f vectra-os-live.iso 2>/dev/null || true
	@$(GRUB) -o vectra-os-live.iso isodir
	@cp -f vectra-os-live.iso $(ISO_IMAGE) 2>/dev/null || echo " [NOTA] $(ISO_IMAGE) em uso na VM. Disponivel como vectra-os-live.iso"
	@echo " [OK]  Imagem ISO gerada com sucesso!"

clean:
	@echo " [CLEAN] Removendo artefatos de build..."
	@rm -rf $(BUILD_DIR) $(KERNEL_BIN) isodir/boot/$(KERNEL_BIN) isodir/apps/*.bin
	@rm -f $(ISO_IMAGE) 2>/dev/null || true
