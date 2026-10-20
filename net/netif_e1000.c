#include "netif_e1000.h"
#include "e1000.h"
#include "drivers.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"

static struct netif e1000_netif;
static bool         network_ready = false;
static bool         dhcp_configured = false;
static char         ip_str_buf[16] = "0.0.0.0";

static err_t e1000_netif_linkoutput(struct netif* netif, struct pbuf* p) {
    (void)netif;
    if (!p) return ERR_ARG;

    /* Se o pbuf estiver em cadeia, junta os bytes em um buffer contiguo */
    if (p->next != NULL) {
        uint8_t temp_buf[2048];
        uint16_t copied = 0;
        for (struct pbuf* q = p; q != NULL; q = q->next) {
            if (copied + q->len > sizeof(temp_buf)) break;
            const uint8_t* src = (const uint8_t*)q->payload;
            for (uint16_t i = 0; i < q->len; i++) {
                temp_buf[copied++] = src[i];
            }
        }
        if (e1000_send(temp_buf, copied) < 0) {
            return ERR_IF;
        }
    } else {
        if (e1000_send(p->payload, p->len) < 0) {
            return ERR_IF;
        }
    }

    return ERR_OK;
}

static err_t netif_e1000_init_fn(struct netif* netif) {
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->output     = etharp_output;
    netif->linkoutput = e1000_netif_linkoutput;
    netif->mtu        = 1500;
    netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;

    e1000_get_mac(netif->hwaddr);
    netif->hwaddr_len = 6;

    return ERR_OK;
}

bool net_init(void) {
    kprint("[NET] Inicializando pilha TCP/IP (lwIP 2.1.3)...\n");

    /* 1. Inicializa o hardware da Intel e1000 */
    if (!e1000_init()) {
        kprint("[NET] Falha ao inicializar o driver da placa de rede e1000.\n");
        return false;
    }

    /* 2. Inicializa o nucleo do lwIP */
    lwip_init();

    /* 3. Adiciona a interface de rede do lwIP */
    ip4_addr_t ipaddr, netmask, gw;
    ip4_addr_set_zero(&ipaddr);
    ip4_addr_set_zero(&netmask);
    ip4_addr_set_zero(&gw);

    netif_add(&e1000_netif, &ipaddr, &netmask, &gw, NULL, netif_e1000_init_fn, ethernet_input);
    netif_set_default(&e1000_netif);
    netif_set_up(&e1000_netif);

    /* 4. Dispara o cliente DHCP para obter IP automatico */
    kprint("[DHCP] Solicitando endereco IP ao servidor da rede...\n");
    dhcp_start(&e1000_netif);

    network_ready = true;
    return true;
}

static uint8_t poll_rx_buffer[2048];
static volatile bool in_net_poll = false;

void net_poll(void) {
    if (!network_ready || in_net_poll) return;
    in_net_poll = true;

    /* 1. Recebe pacotes pendentes na placa de rede */
    int len;
    while ((len = e1000_receive(poll_rx_buffer, sizeof(poll_rx_buffer))) > 0) {
        struct pbuf* p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if (p != NULL) {
            pbuf_take(p, poll_rx_buffer, len);
            if (e1000_netif.input(p, &e1000_netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }

    /* 2. Atualiza timers internos do lwIP (ARP, DHCP, TCP) */
    sys_check_timeouts();

    /* 3. Notifica quando o DHCP obtiver o IP */
    if (!dhcp_configured && dhcp_supplied_address(&e1000_netif)) {
        dhcp_configured = true;

        uint32_t addr = ip4_addr_get_u32(netif_ip4_addr(&e1000_netif));
        uint8_t b1 = addr & 0xFF;
        uint8_t b2 = (addr >> 8) & 0xFF;
        uint8_t b3 = (addr >> 16) & 0xFF;
        uint8_t b4 = (addr >> 24) & 0xFF;

        /* Formata a string do IP */
        char* p = ip_str_buf;
        #define APPEND_INT(val) do { \
            int v = (val); \
            if (v >= 100) *p++ = '0' + (v / 100); \
            if (v >= 10)  *p++ = '0' + ((v / 10) % 10); \
            *p++ = '0' + (v % 10); \
        } while(0)

        APPEND_INT(b1); *p++ = '.';
        APPEND_INT(b2); *p++ = '.';
        APPEND_INT(b3); *p++ = '.';
        APPEND_INT(b4); *p = '\0';
    }

    in_net_poll = false;
}

bool net_has_ip(void) {
    return dhcp_configured;
}

uint32_t net_get_ip(void) {
    if (!dhcp_configured) return 0;
    return ip4_addr_get_u32(netif_ip4_addr(&e1000_netif));
}

const char* net_get_ip_str(void) {
    return ip_str_buf;
}
