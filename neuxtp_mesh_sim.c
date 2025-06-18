#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>
#include <rte_ip.h>
#include <rte_ether.h>
#include <rte_cycles.h>
#include <rte_atomic.h>
#include <rte_byteorder.h>

#define NB_MBUF       8192
#define NUM_DESC      1024
#define MAX_CORES     128
#define BURST_SIZE    32

#define IPv4(a, b, c, d) ((uint32_t)(((a) & 0xff) << 24 | \
                                     ((b) & 0xff) << 16 | \
                                     ((c) & 0xff) << 8  | \
                                     ((d) & 0xff)))

volatile bool keep_running = true;

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
    uint8_t rx_q;
    uint64_t tx_count;
    uint64_t rx_count;
};

static struct rte_mempool *mbuf_pool;
static struct core_conf core_stats[MAX_CORES];

static void handle_signal(int sig) {
    keep_running = false;
}

struct rte_mbuf *build_neuxtp_pkt(uint8_t ai_tag, uint8_t priority, uint16_t port_id) {
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!mbuf) return NULL;

    struct rte_ether_hdr *eth = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(eth + 1);
    struct neuxtp_hdr *neu = (struct neuxtp_hdr *)(ip + 1);

    mbuf->data_len = sizeof(*eth) + sizeof(*ip) + sizeof(*neu);
    mbuf->pkt_len = mbuf->data_len;

    // Ethernet Header
    memset(eth->dst_addr.addr_bytes, 0xff, RTE_ETHER_ADDR_LEN);  // Broadcast
    memset(eth->src_addr.addr_bytes, 0xaa, RTE_ETHER_ADDR_LEN);  // Dummy MAC
    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // IP Header
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = rte_cpu_to_be_16(sizeof(*ip) + sizeof(*neu));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = 253; // Custom protocol
    ip->hdr_checksum = 0;
    ip->src_addr = rte_cpu_to_be_32(IPv4(10, 0, port_id, 1));
    ip->dst_addr = rte_cpu_to_be_32(IPv4(10, 0, port_id ^ 1, 1));
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // NeuXTP Header
    neu->ai_tag = ai_tag;
    neu->priority = priority;
    neu->flags = 0;
    neu->session_id = rte_rdtsc();
    neu->timestamp = rte_rdtsc_precise();

    printf("[BUILD] Port %u | AI Tag: %u | Priority: %u | Session: %u\n", port_id, ai_tag, priority, neu->session_id);

    return mbuf;
}

int lcore_mesh_io(void *arg) {
    struct core_conf *conf = (struct core_conf *)arg;
    const uint16_t port = conf->port_id;
    const uint16_t tx_q = conf->tx_q;
    const uint16_t rx_q = conf->rx_q;
    const uint8_t core_id = rte_lcore_id();

    printf("[Core %u] Sending NeuXTP packets on port %u queue %u\n", core_id, port, tx_q);

    struct rte_mbuf *rx_bufs[BURST_SIZE];

    while (keep_running) {
        // Transmit
        struct rte_mbuf *pkt = build_neuxtp_pkt(core_id, 7 - core_id, port);
        if (pkt) {
            uint16_t sent = rte_eth_tx_burst(port, tx_q, &pkt, 1);
            if (sent) {
                conf->tx_count++;
                printf("[TX] Core %u | Port %u | Queue %u\n", core_id, port, tx_q);
            } else {
                rte_pktmbuf_free(pkt);
            }
        }

        // Receive
        uint16_t nb_rx = rte_eth_rx_burst(port, rx_q, rx_bufs, BURST_SIZE);
        if (nb_rx) {
            conf->rx_count += nb_rx;
            printf("[RX] Core %u | Port %u | Queue %u | Packets: %u\n", core_id, port, rx_q, nb_rx);
        }
        for (uint16_t i = 0; i < nb_rx; i++)
            rte_pktmbuf_free(rx_bufs[i]);

        rte_delay_us_block(100);
    }

    return 0;
}

int main(int argc, char **argv) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    uint16_t nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < 2)
        rte_exit(EXIT_FAILURE, "Need at least 2 ports\n");

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NB_MBUF,
                                        0, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Failed to create mempool\n");

    for (uint16_t port = 0; port < nb_ports; port++) {
        struct rte_eth_conf port_conf = {0};
        ret = rte_eth_dev_configure(port, 4, 4, &port_conf);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n", ret, port);

        for (int q = 0; q < 4; q++) {
            rte_eth_rx_queue_setup(port, q, NUM_DESC, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
            rte_eth_tx_queue_setup(port, q, NUM_DESC, rte_eth_dev_socket_id(port), NULL);
        }

        ret = rte_eth_dev_start(port);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Failed to start port %u\n", port);

        rte_eth_promiscuous_enable(port);
        printf("Initialized port %u\n", port);
    }

    unsigned i = 0;
    unsigned core_id;
    RTE_LCORE_FOREACH_WORKER(core_id) {
        core_stats[core_id].port_id = i % nb_ports;
        core_stats[core_id].tx_q = i % 4;
        core_stats[core_id].rx_q = i % 4;
        rte_eal_remote_launch(lcore_mesh_io, &core_stats[core_id], core_id);
        i++;
    }

    // Stats loop
    while (keep_running) {
        printf("\n==== Packet Stats (every 2s) ====\n");
        RTE_LCORE_FOREACH_WORKER(core_id) {
            printf("Core %u: Port %u | TX: %" PRIu64 " | RX: %" PRIu64 "\n",
                   core_id,
                   core_stats[core_id].port_id,
                   core_stats[core_id].tx_count,
                   core_stats[core_id].rx_count);
        }
        sleep(2);
    }

    rte_eal_mp_wait_lcore();
    return 0;
}
