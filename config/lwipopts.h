#pragma once

// LexxOS currently uses the Pico SDK CYW43 threadsafe-background backend.
// That backend drives lwIP in NO_SYS mode from the Pico async context, so
// netconn/socket APIs stay disabled and commands should use the raw lwIP APIs
// under cyw43_arch_lwip_begin()/cyw43_arch_lwip_end().

#include <stdlib.h>

// -----------------------------------------------------------------------------
// Core execution model
// -----------------------------------------------------------------------------

#define NO_SYS 1
#define SYS_LIGHTWEIGHT_PROT 1

// -----------------------------------------------------------------------------
// Protocols required by wifi-connect and rtc-sync
// -----------------------------------------------------------------------------

#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 0
#define LWIP_DHCP 1
#define LWIP_AUTOIP 0
#define LWIP_DNS 1

// The current LexxOS network commands use lwIP raw UDP/DNS APIs. Enabling
// netconn/socket APIs in NO_SYS mode is invalid and would pull in unsupported
// sys_arch dependencies on this target.
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0

// -----------------------------------------------------------------------------
// Network interface behaviour
// -----------------------------------------------------------------------------

#define LWIP_NETIF_HOSTNAME 1
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_TX_SINGLE_PBUF 1

// Avoid extra ARP conflict checks during DHCP. This keeps connection time and
// memory use predictable on the constrained PicoCalc build.
#define DHCP_DOES_ARP_CHECK 0
#define LWIP_DHCP_DOES_ACD_CHECK 0

// -----------------------------------------------------------------------------
// Memory pools
// -----------------------------------------------------------------------------

// PICO_CYW43_ARCH_THREADSAFE_BACKGROUND requires lwIP to keep its allocator and
// memp pools inside lwIP. Do not route lwIP through libc malloc here: Pico SDK
// rejects MEM_LIBC_MALLOC for this backend at compile time.
#define MEM_LIBC_MALLOC 0
#define MEMP_MEM_MALLOC 0
#define MEM_ALIGNMENT 4

// The current network milestone needs Wi-Fi association, DHCP, DNS and one
// small UDP/NTP exchange. DHCP packets are not tiny: typical offers/replies are
// large enough that the earlier 384-byte pbuf pool made Pico W reach
// CYW43_LINK_NOIP and then time out. Keep this conservative, but large enough
// for one full Ethernet frame plus the small DHCP/DNS/NTP control traffic.
#define MEM_SIZE (6 * 1024)

#define MEMP_NUM_PBUF 10
#define MEMP_NUM_RAW_PCB 1
#define MEMP_NUM_UDP_PCB 3
#define MEMP_NUM_TCP_PCB 0
#define MEMP_NUM_TCP_PCB_LISTEN 0
#define MEMP_NUM_TCP_SEG 0
#define MEMP_NUM_REASSDATA 0
#define MEMP_NUM_FRAG_PBUF 0
#define MEMP_NUM_ARP_QUEUE 10
#define MEMP_NUM_IGMP_GROUP 0
#define MEMP_NUM_SYS_TIMEOUT 12
#define MEMP_NUM_NETBUF 0
#define MEMP_NUM_NETCONN 0
#define MEMP_NUM_NETDB 0
#define MEMP_NUM_TCPIP_MSG_API 0
#define MEMP_NUM_TCPIP_MSG_INPKT 0

// Keep a small number of full-size receive buffers. DHCP/ARP can require a full
// Ethernet-sized pbuf, but the PicoCalc RP2040 build cannot afford eight or more
// of them in .bss alongside the LCD console, SD stack and diagnostics. Six full
// buffers keeps the ETH_PAD_SIZE fix while staying within the RAM budget.
#define PBUF_POOL_SIZE 6
#define PBUF_POOL_BUFSIZE 1536

// The current commands use unfragmented local-link/DNS/NTP traffic. Disable
// fragmentation/reassembly support to avoid extra pools.
#define IP_REASSEMBLY 0
#define IP_FRAG 0
#define IP_FORWARD 0
#define ARP_QUEUEING 0

// DNS is only used to resolve the NTP host; keep the cache intentionally small.
#define DNS_TABLE_SIZE 2
#define DNS_MAX_SERVERS 2
#define DNS_MAX_NAME_LENGTH 64

// Avoid client-data slots and optional APIs that are not used by LexxOS yet.
#define LWIP_NUM_NETIF_CLIENT_DATA 0
#define LWIP_NETIF_API 0

// -----------------------------------------------------------------------------
// Checksum and packet layout
// -----------------------------------------------------------------------------

#define LWIP_CHKSUM_ALGORITHM 3
#define CHECKSUM_GEN_IP 1
#define CHECKSUM_GEN_UDP 1
#define CHECKSUM_GEN_TCP 0
#define CHECKSUM_CHECK_IP 1
#define CHECKSUM_CHECK_UDP 1
#define CHECKSUM_CHECK_TCP 0

// Keep the packet layout aligned with the official Pico W examples. The CYW43
// lwIP examples intentionally do not enable ETH_PAD_SIZE=2; enabling it here can
// shift Ethernet/IP payload offsets and make DHCP replies disappear after the
// Wi-Fi association reaches CYW43_LINK_NOIP.
#define ETH_PAD_SIZE 0

// -----------------------------------------------------------------------------
// Diagnostics
// -----------------------------------------------------------------------------

#define LWIP_STATS 0
#define LWIP_STATS_DISPLAY 0
#define MEM_STATS 0
#define MEMP_STATS 0
#define LINK_STATS 0
#define IP_STATS 0
#define ICMP_STATS 0
#define UDP_STATS 0
#define TCP_STATS 0
#define DHCP_STATS 0
#define DNS_STATS 0

#define LWIP_DEBUG 0

// lwIP uses this for transaction identifiers and some protocol randomisation.
#define LWIP_RAND() ((u32_t) rand())

// -----------------------------------------------------------------------------
// Optional features disabled for the first PicoCalc network milestone
// -----------------------------------------------------------------------------

#define LWIP_MDNS_RESPONDER 0
#define LWIP_IGMP 0
#define LWIP_SNMP 0
#define LWIP_DNS_SUPPORT_MDNS_QUERIES 0
#define LWIP_HAVE_LOOPIF 0
#define LWIP_NETIF_LOOPBACK 0
#define LWIP_LOOPBACK_MAX_PBUFS 0

// The Pico SDK still compiles broad lwIP source groups, but these options keep
// the unused modules empty and avoid pulling in their static state.
#define LWIP_TCPIP_CORE_LOCKING 0
#define LWIP_TCPIP_CORE_LOCKING_INPUT 0
#define LWIP_TCP_KEEPALIVE 0
#define LWIP_LISTEN_BACKLOG 0
#define LWIP_CALLBACK_API 1
#define LWIP_EVENT_API 0
#define LWIP_ALTCP 0
#define LWIP_WND_SCALE 0
#define LWIP_IPV6_AUTOCONFIG 0
#define LWIP_IPV6_DHCP6 0
#define LWIP_IPV6_MLD 0
#define LWIP_IPV6_REASS 0
#define LWIP_IPV6_FRAG 0

#define PPP_SUPPORT 0
#define PPPOE_SUPPORT 0
#define PPPOS_SUPPORT 0
#define LWIP_PPP_API 0
#define LWIP_6LOWPAN 0
#define LWIP_BRIDGEIF 0
#define LWIP_SLIP 0
#define LWIP_ZEPIF 0

#define LWIP_COMPAT_MUTEX 0
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_SO_RCVTIMEO 0
#define LWIP_SO_SNDTIMEO 0
#define LWIP_SO_RCVBUF 0
#define SO_REUSE 0
