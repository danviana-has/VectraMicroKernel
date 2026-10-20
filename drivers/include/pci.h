#ifndef _PCI_H
#define _PCI_H

#include <libpayload.h>

#define PCI_DEV(bus, dev, fn) (0x80000000UL | (((bus) & 0xFF) << 16) | (((dev) & 0x1F) << 11) | (((fn) & 0x07) << 8))

#define REG_VENDOR_ID       0x00
#define REG_DEVICE_ID       0x02
#define REG_COMMAND         0x04
#define REG_COMMAND_BM      0x0004
#define REG_STATUS          0x06
#define REG_HEADER_TYPE     0x0E
#define REG_PRIMARY_BUS     0x18
#define REG_SECONDARY_BUS   0x19
#define REG_SUBORDINATE_BUS 0x1A

#define HEADER_TYPE_NORMAL  0x00
#define HEADER_TYPE_BRIDGE  0x01
#define HEADER_TYPE_CARDBUS 0x02

u8  pci_read_config8(pcidev_t device, u16 reg);
u16 pci_read_config16(pcidev_t device, u16 reg);
u32 pci_read_config32(pcidev_t device, u16 reg);
void pci_write_config8(pcidev_t device, u16 reg, u8 val);
void pci_write_config16(pcidev_t device, u16 reg, u16 val);
void pci_write_config32(pcidev_t device, u16 reg, u32 val);

int pci_find_device(unsigned short vid, unsigned short did, pcidev_t *dev);
u32 pci_read_resource(pcidev_t dev, int bar);
void pci_set_bus_master(pcidev_t dev);

#endif /* _PCI_H */
