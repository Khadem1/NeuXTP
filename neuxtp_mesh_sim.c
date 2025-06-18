#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_cycles.h>

#define NB_MBUF 8192
#define NUM_DESC 1024
#define MAX_TX_QUEUES RTE_MAX_LCORE

#define IPv4(a, b, c, d) ((uint32_t)(((a & 0xff) << 24) | ((b & 0xff) << 16) | ((c & 0xff) << 8) | (d & 0xff)))

struct neuxtp_hdr {
    uint8_t ai_tag;
    uint8_t priority;
    uint16_t flags;
    uint32_t session_id;
    uint64_t timestamp;
} __attribute__((__packed__));

struct core_conf {
    uint16_t port_id;
    uint8_t tx_q;
};

static struct rte_mempool *mbuf_pool;

struct rte_mbuf *build_neuxtp_pkt(uint8_t ai_tag, uint8_t priority, uint16_t port_id) {
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!mbuf) return NULL;

    struct rte_ether_hdr *eth = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);
    struct neuxtp_hdr *neu = (struct neuxtp_hdr *)(ip + 1);

    mbuf->data_len = sizeof(*eth) + sizeof(*ip) + sizeof(*neu);
    mbuf->pkt_len = mbuf->data_len;

    // Ethernet
    memset(eth->dst_addr.addr_bytes, 0xff, 6);
    memset(eth->src_addr.addr_bytes, 0xaa, 6);
    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // IPv4
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = rte_cpu_to_be_16(sizeof(*ip) + sizeof(*neu));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = 253;
    ip->hdr_checksum = 0;
    ip->src_addr = rte_cpu_to_be_32(IPv4(10, 0, port_id, 1));
    ip->dst_addr = rte_cpu_to_be_32(IPv4(10, 0, port_id ^ 1, 1));
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // NeuXTP header
    neu->ai_tag = ai_tag;
    neu->priority = priority;
    neu->flags = 0;
    neu->session_id = rte_rdtsc();
    neu->timestamp = rte_rdtsc_precise();

    return mbuf;
}

int lcore_mesh_tx(void *arg) {
    struct core_conf *conf = (struct core_conf *)arg;
    uint8_t core_id = rte_lcore_id();

    printf("[Core %u] Sending NeuXTP packets on port %u queue %u\n", core_id, conf->port_id, conf->tx_q);

    while (1) {
        struct rte_mbuf *pkt = build_neuxtp_pkt(core_id, 7 - core_id, conf->port_id);
        if (pkt)
            rte_eth_tx_burst(conf->port_id, conf->tx_q, &pkt, 1);
        rte_delay_us_block(10);  // 10 µs delay
    }
    return 0;
}

int main(int argc, char **argv) {
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    uint16_t nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 2)
        rte_exit(EXIT_FAILURE, "Need at least 2 ports\n");

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NB_MBUF, 0, 0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Failed to create mempool\n");

    unsigned nb_lcores = rte_lcore_count() - 1;
    unsigned num_tx_qs = nb_lcores;

    struct rte_eth_conf port_conf = {0};

    for (uint16_t port = 0; port < nb_ports; port++) {
        rte_eth_dev_configure(port, 0, num_tx_qs, &port_conf);
        for (uint16_t q = 0; q < num_tx_qs; q++) {
            rte_eth_tx_queue_setup(port, q, NUM_DESC, rte_eth_dev_socket_id(port), NULL);
        }
        rte_eth_dev_start(port);
        printf("Initialized port %u\n", port);
    }

    static struct core_conf confs[MAX_TX_QUEUES];
    unsigned i = 0;
    unsigned core_id;
    RTE_LCORE_FOREACH_WORKER(core_id) {
        confs[i].port_id = i % nb_ports;
        confs[i].tx_q = i;  // unique tx_q per core
        rte_eal_remote_launch(lcore_mesh_tx, &confs[i], core_id);
        i++;
    }

    rte_eal_mp_wait_lcore();
    return 0;
}
