#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* Modo Bare-Metal sem OS threads para o lwIP (cooperativo / polling) */
#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0
#define LWIP_TIMERS                     1
#define LWIP_TIMERS_CUSTOM              0
#define LWIP_STATS                      0

#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (64 * 1024)

#define MEMP_NUM_PBUF                   32
#define MEMP_NUM_RAW_PCB                4
#define MEMP_NUM_UDP_PCB                8
#define MEMP_NUM_TCP_PCB                16
#define MEMP_NUM_TCP_PCB_LISTEN         8
#define MEMP_NUM_TCP_SEG                32
#define MEMP_NUM_SYS_TIMEOUT            16

#define PBUF_POOL_SIZE                  64
#define PBUF_POOL_BUFSIZE               1536

/* Modulos de Rede Habilitados */
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_IPV4                       1
#define LWIP_ICMP                       1
#define LWIP_RAW                        1
#define LWIP_DHCP                       1
#define LWIP_DNS                        1
#define LWIP_UDP                        1
#define LWIP_TCP                        1

#define TCP_MSS                         1460
#define TCP_WND                         (8 * TCP_MSS)
#define TCP_SND_BUF                     (8 * TCP_MSS)

/* Desativa Sockets e Netconn BSD (requerem multithreading com semaforos/filas) */
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0

/* Desativa funcoes da libc que nao possuimos */
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS 1

#endif /* LWIPOPTS_H */
