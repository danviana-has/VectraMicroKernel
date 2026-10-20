#ifndef VECTRA_E1000_H
#define VECTRA_E1000_H

#include "types.h"

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E /* 82540EM Gigabit Ethernet Controller (QEMU padrao) */

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 16
#define E1000_RX_BUF_SIZE 2048

/* Descritor de Recepcao (Rx) da e1000 */
struct e1000_rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t  status;
    volatile uint8_t  errors;
    volatile uint16_t special;
} __attribute__((packed));

/* Descritor de Transmissao (Tx) da e1000 */
struct e1000_tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t  cso;
    volatile uint8_t  cmd;
    volatile uint8_t  status;
    volatile uint8_t  css;
    volatile uint16_t special;
} __attribute__((packed));

bool e1000_init(void);
int  e1000_send(const void* data, uint16_t len);
int  e1000_receive(void* buffer, uint16_t max_len);
void e1000_get_mac(uint8_t mac[6]);
bool e1000_is_link_up(void);

#endif /* VECTRA_E1000_H */
