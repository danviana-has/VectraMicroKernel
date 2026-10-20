#ifndef NETIF_E1000_H
#define NETIF_E1000_H

#include "types.h"
#include "lwip/netif.h"

bool        net_init(void);
void        net_poll(void);
bool        net_has_ip(void);
uint32_t    net_get_ip(void);
const char* net_get_ip_str(void);

#endif /* NETIF_E1000_H */
