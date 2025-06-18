#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

// NeuXTP header definition
struct neuxtp_hdr {
    uint8_t version;
    uint8_t flags;
    uint8_t ai_tag;    // AI-specific metadata
    uint8_t priority;  // Packet priority level
    uint32_t session_id;
} __attribute__((__packed__));

#define NEUXTP_PROTO_ID 0xFD  // Custom protocol number for IP header

// Helper to encode IPv4 address
#define IPv4(a, b, c, d) ((uint32_t)(((a & 0xff) << 24) | ((b & 0xff) << 16) | ((c & 0xff) << 8) | (d & 0xff)))

// Create a NeuXTP packet
struct rte_mbuf *build_neuxtp_packet(struct rte_mempool *mbuf_pool, uint8_t ai_tag, uint8_t priority) {
    const uint16_t pkt_size = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct neuxtp_hdr);
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!mbuf) return NULL;

    mbuf->data_len = pkt_size;
    mbuf->pkt_len = pkt_size;

    uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);

    // Ethernet
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)pkt_data;
    memset(eth->dst_addr.addr_bytes, 0xff, RTE_ETHER_ADDR_LEN);
    memset(eth->src_addr.addr_bytes, 0xaa, RTE_ETHER_ADDR_LEN);
    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // IPv4
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);
    memset(ip, 0, sizeof(struct rte_ipv4_hdr));
    ip->version_ihl = 0x45;
    ip->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct neuxtp_hdr));
    ip->time_to_live = 64;
    ip->next_proto_id = NEUXTP_PROTO_ID;
    ip->src_addr = rte_cpu_to_be_32(IPv4(10, 0, 0, 1));
    ip->dst_addr = rte_cpu_to_be_32(IPv4(10, 0, 0, 2));
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // NeuXTP header
    struct neuxtp_hdr *neu = (struct neuxtp_hdr *)(ip + 1);
    neu->version = 1;
    neu->flags = 0;
    neu->ai_tag = ai_tag;
    neu->priority = priority;
    neu->session_id = rte_cpu_to_be_32(rand());

    return mbuf;
}

// Simulate basic NeuXTP test
void simulate_neuxtp_tests(struct rte_mempool *mbuf_pool) {
    printf("\n Simulating NeuXTP Packets with AI Tags...\n\n");
    for (int i = 0; i < 5; ++i) {
        struct rte_mbuf *pkt = build_neuxtp_packet(mbuf_pool, i, 5 - i);
        if (pkt) {
            printf("[+] Packet %d created: AI_TAG=%d, PRIORITY=%d\n", i, i, 5 - i);
            rte_pktmbuf_free(pkt);
        } else {
            printf("[-] Packet %d failed to allocate\n", i);
        }
    }
}

int main(int argc, char **argv) {
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Cannot init EAL\n");

    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
        NUM_MBUFS, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    simulate_neuxtp_tests(mbuf_pool);
    return 0;
}
