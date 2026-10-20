#include "e1000.h"
#include "heap.h"
#include "vmm.h"
#include "drivers.h"
#include "pci.h"

/* Registradores da Intel e1000 */
#define REG_CTRL          0x0000
#define E1000_REG_STATUS  0x0008
#define REG_EEPROM     0x0014
#define REG_ICR        0x00C0
#define REG_IMS        0x00D0
#define REG_RCTL       0x0100
#define REG_TCTL       0x0400
#define REG_RDBAL      0x2800
#define REG_RDBAH      0x2804
#define REG_RDLEN      0x2808
#define REG_RDH        0x2810
#define REG_RDT        0x2818
#define REG_TDBAL      0x3800
#define REG_TDBAH      0x3804
#define REG_TDLEN      0x3808
#define REG_TDH        0x3810
#define REG_TDT        0x3818
#define REG_RAL        0x5400
#define REG_RAH        0x5404

/* Flags de RCTL */
#define RCTL_EN        (1 << 1)
#define RCTL_SBP       (1 << 2)
#define RCTL_UPE       (1 << 3)
#define RCTL_MPE       (1 << 4)
#define RCTL_BAM       (1 << 15)
#define RCTL_SECRC     (1 << 26)

/* Flags de TCTL */
#define TCTL_EN        (1 << 1)
#define TCTL_PSP       (1 << 3)

/* Flags de Tx Command */
#define CMD_EOP        (1 << 0)
#define CMD_IFCS       (1 << 1)
#define CMD_RS         (1 << 3)

static uint32_t mmio_base = 0;
static uint8_t  e1000_mac[6];
static bool     e1000_found = false;

static struct e1000_rx_desc rx_descs[E1000_NUM_RX_DESC] __attribute__((aligned(16)));
static uint8_t              rx_buffers[E1000_NUM_RX_DESC][E1000_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint16_t              cur_rx = 0;

static struct e1000_tx_desc tx_descs[E1000_NUM_TX_DESC] __attribute__((aligned(16)));
static uint8_t              tx_buffers[E1000_NUM_TX_DESC][E1000_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint16_t              cur_tx = 0;

static inline void e1000_write(uint16_t reg, uint32_t val) {
    *(volatile uint32_t*)(mmio_base + reg) = val;
}

static inline uint32_t e1000_read(uint16_t reg) {
    return *(volatile uint32_t*)(mmio_base + reg);
}

void e1000_get_mac(uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        mac[i] = e1000_mac[i];
    }
}

bool e1000_is_link_up(void) {
    if (!e1000_found) return false;
    return (e1000_read(E1000_REG_STATUS) & (1 << 1)) != 0;
}

bool e1000_init(void) {
    pcidev_t dev;

    /* 1. Procura a placa de rede Intel e1000 no barramento PCI */
    if (!pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID, &dev)) {
        /* Tenta IDs alternativos emulados pelo QEMU / VirtualBox */
        if (!pci_find_device(E1000_VENDOR_ID, 0x1004, &dev) &&
            !pci_find_device(E1000_VENDOR_ID, 0x100F, &dev)) {
            kprint("[e1000] Placa de rede Intel e1000 nao encontrada no barramento PCI.\n");
            return false;
        }
    }

    kprint("[e1000] Placa Intel e1000 Gigabit detectada no barramento PCI!\n");

    /* 2. Le o BAR0 (MMIO Base) e habilita Bus Mastering */
    uint32_t bar0 = pci_read_resource(dev, 0) & ~0xF;
    mmio_base = bar0;

    pci_set_bus_master(dev);
    uint16_t cmd = pci_read_config16(dev, REG_COMMAND);
    pci_write_config16(dev, REG_COMMAND, cmd | 0x0007); /* I/O, Mem, Bus Master */

    /* 3. Mapeia a regiao MMIO de 128 KiB no VMM */
    for (uint32_t i = 0; i < 32; i++) {
        vmm_map_page(bar0 + (i * 4096), bar0 + (i * 4096), PAGE_PRESENT | PAGE_WRITE);
    }

    /* 4. Le o endereco MAC do hardware */
    uint32_t ral = e1000_read(REG_RAL);
    uint32_t rah = e1000_read(REG_RAH);
    e1000_mac[0] = ral & 0xFF;
    e1000_mac[1] = (ral >> 8) & 0xFF;
    e1000_mac[2] = (ral >> 16) & 0xFF;
    e1000_mac[3] = (ral >> 24) & 0xFF;
    e1000_mac[4] = rah & 0xFF;
    e1000_mac[5] = (rah >> 8) & 0xFF;

    kprint("        MAC Address: ");
    for (int i = 0; i < 6; i++) {
        char hex[3];
        uint8_t byte = e1000_mac[i];
        hex[0] = "0123456789ABCDEF"[(byte >> 4) & 0xF];
        hex[1] = "0123456789ABCDEF"[byte & 0xF];
        hex[2] = '\0';
        kprint(hex);
        if (i < 5) kprint(":");
    }
    kprint("\n");

    /* 5. Inicializa descritores de recepcao (Rx) */
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].addr     = (uint32_t)&rx_buffers[i][0];
        rx_descs[i].status   = 0;
        rx_descs[i].errors   = 0;
        rx_descs[i].length   = 0;
        rx_descs[i].checksum = 0;
        rx_descs[i].special  = 0;
    }

    e1000_write(REG_RDBAL, (uint32_t)&rx_descs[0]);
    e1000_write(REG_RDBAH, 0);
    e1000_write(REG_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    e1000_write(REG_RDH, 0);
    e1000_write(REG_RDT, E1000_NUM_RX_DESC - 1);
    cur_rx = 0;

    /* Habilita Recepcao: Broadcast, Unicast, Strip CRC */
    e1000_write(REG_RCTL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_BAM | RCTL_SECRC);

    /* 6. Inicializa descritores de transmissao (Tx) */
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr    = (uint32_t)&tx_buffers[i][0];
        tx_descs[i].cmd     = 0;
        tx_descs[i].status  = (1 << 0); /* Descriptor Done */
        tx_descs[i].length  = 0;
        tx_descs[i].cso     = 0;
        tx_descs[i].css     = 0;
        tx_descs[i].special = 0;
    }

    e1000_write(REG_TDBAL, (uint32_t)&tx_descs[0]);
    e1000_write(REG_TDBAH, 0);
    e1000_write(REG_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    e1000_write(REG_TDH, 0);
    e1000_write(REG_TDT, 0);
    cur_tx = 0;

    /* Habilita Transmissao: Pad Short Packets, Collision Threshold 15, Distance 64 */
    e1000_write(REG_TCTL, TCTL_EN | TCTL_PSP | (15 << 4) | (64 << 12));

    /* 7. Habilita o Link da placa de rede (SLU - Set Link Up) */
    e1000_write(REG_CTRL, e1000_read(REG_CTRL) | (1 << 6));

    /* Limpa e desativa interrupcoes da e1000 (usamos polling integrado ao loop do microkernel) */
    e1000_write(REG_IMS, 0);
    e1000_read(REG_ICR);

    e1000_found = true;
    kprint("        [OK] Anéis de DMA Rx (32) e Tx (16) ativados com sucesso (Link UP)\n");
    return true;
}

int e1000_send(const void* data, uint16_t len) {
    if (!e1000_found || !data || len == 0 || len > E1000_RX_BUF_SIZE) return -1;

    /* Aguarda o descritor atual estar livre (Descriptor Done) */
    int timeout = 50000;
    while (!(tx_descs[cur_tx].status & (1 << 0)) && --timeout > 0);
    if (timeout <= 0) return -1;

    /* Copia os dados para o buffer de DMA da placa */
    uint8_t* dest = tx_buffers[cur_tx];
    const uint8_t* src = (const uint8_t*)data;
    for (uint16_t i = 0; i < len; i++) {
        dest[i] = src[i];
    }

    tx_descs[cur_tx].length = len;
    tx_descs[cur_tx].cmd    = CMD_EOP | CMD_IFCS | CMD_RS;
    tx_descs[cur_tx].status = 0;

    /* Notifica o hardware atualizando o Tail */
    cur_tx = (cur_tx + 1) % E1000_NUM_TX_DESC;
    e1000_write(REG_TDT, cur_tx);

    return 0;
}

int e1000_receive(void* buffer, uint16_t max_len) {
    if (!e1000_found || !buffer) return 0;

    /* Verifica se o descritor atual contem um pacote (DD bit) */
    if (!(rx_descs[cur_rx].status & (1 << 0))) {
        return 0; /* Nenhum pacote disponivel */
    }

    uint16_t pkt_len = rx_descs[cur_rx].length;
    uint16_t to_copy = (pkt_len < max_len) ? pkt_len : max_len;

    uint8_t* src = rx_buffers[cur_rx];
    uint8_t* dest = (uint8_t*)buffer;
    for (uint16_t i = 0; i < to_copy; i++) {
        dest[i] = src[i];
    }

    /* Reseta o status do descritor */
    rx_descs[cur_rx].status = 0;

    /* Devolve o buffer para a placa atualizando o RDT */
    uint16_t old_rx = cur_rx;
    cur_rx = (cur_rx + 1) % E1000_NUM_RX_DESC;
    e1000_write(REG_RDT, old_rx);

    return (int)to_copy;
}
