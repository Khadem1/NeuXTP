// neuxtp_dpdk.c
// Hybrid NeuXTP Protocol: DPDK + Python AI

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define MAX_PKT_BURST 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {.max_rx_pkt_len = RTE_ETHER_MAX_LEN}
};

static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
    struct rte_eth_conf port_conf = port_conf_default;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    int retval = rte_eth_dev_info_get(port, &dev_info);
    if (retval != 0) return retval;

    retval = rte_eth_dev_configure(port, 1, 1, &port_conf);
    if (retval != 0) return retval;

    retval = rte_eth_rx_queue_setup(port, 0, RX_RING_SIZE,
                                    rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0) return retval;

    txconf = dev_info.default_txconf;
    retval = rte_eth_tx_queue_setup(port, 0, TX_RING_SIZE,
                                    rte_eth_dev_socket_id(port), &txconf);
    if (retval < 0) return retval;

    retval = rte_eth_dev_start(port);
    if (retval < 0) return retval;

    rte_eth_promiscuous_enable(port);
    return 0;
}

static int
lcore_main(__attribute__((unused)) void *arg) {
    const uint16_t port = 0;
    struct rte_mbuf *bufs[BURST_SIZE];

    while (1) {
        const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);
        if (nb_rx == 0)
            continue;

        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *mbuf = bufs[i];
            uint8_t *data = rte_pktmbuf_mtod(mbuf, uint8_t *);
            size_t len = rte_pktmbuf_data_len(mbuf);

            FILE *fp = fopen("/tmp/neuxtp_payload.bin", "wb");
            fwrite(data, 1, len, fp);
            fclose(fp);

            int priority = system("python3 ./ai_predictor.py > /tmp/neuxtp_priority.txt");

            char result[10];
            FILE *rf = fopen("/tmp/neuxtp_priority.txt", "r");
            fgets(result, sizeof(result), rf);
            fclose(rf);

            if (atoi(result) > 70) {
                rte_eth_tx_burst(port, 0, &mbuf, 1);
            } else {
                rte_pktmbuf_free(mbuf);
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * 2,
        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    if (port_init(0, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port\n");

    rte_eal_remote_launch(lcore_main, NULL, 1);
    rte_eal_mp_wait_lcore();
    return 0;
}
