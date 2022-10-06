#include "main.h"

static uint64_t u64_diff(uint64_t x, uint64_t y)
{
    return (x > y ? (x - y) : (y - x));
}

static void init_tx(void)
{
    uint32_t lcore = rte_lcore_id();
    uint64_t cpu_freq = rte_get_tsc_hz();
    uint64_t current_time = rte_get_tsc_cycles();
    uint64_t tx_rate_scale = 0;
    uint64_t tx_rate_bps, target_tx_rate_bps;

    tx_rate_scale = (((app.tx_rate_mbps >> 3) * (uint64_t)1e6) << RATE_SCALE) / cpu_freq;
    app.cpu_freq[lcore] = cpu_freq;
    app.prev_time = current_time;
    app.token = app.bucket_size;
    app.core_tx = lcore;
    app.tx_rate_scale = tx_rate_scale;
    tx_rate_bps = (app.tx_rate_scale * 8 * rte_get_tsc_hz()) >> RATE_SCALE;
    target_tx_rate_bps = app.tx_rate_mbps * (uint64_t)1e6;
    RTE_LOG(
        INFO, SWITCH,
        "%s: actual tx_rate of port %u: %lubps=%luMbps\n",
        __func__,
        app.ports,
        tx_rate_bps,
        tx_rate_bps / (uint64_t)1e6);
    if (u64_diff(tx_rate_bps, target_tx_rate_bps) > target_tx_rate_bps / 20)
    {
        RTE_LOG(
            ERR, SWITCH,
            "%s: Calculated tx_rate(%lubps) is significantly different from origin tx rate(%lubps). Integer overflow?\n",
            __func__,
            tx_rate_bps, target_tx_rate_bps);
    }
}

void app_main_loop_tx(void)
{
    // uint32_t i;

    RTE_LOG(INFO, SWITCH, "Core %u is doing TX\n", rte_lcore_id());

    init_tx();

    while(!force_quit)
    {
        if (rte_ring_count(app.rings_pull))
        {
            app_main_tx_port(app.rings_pull);
        }
        else
        {
            app_main_tx_port(app.rings_tx);
        }
    }
}

// void
// app_main_loop_tx_each_port(uint32_t port_id) {

//     RTE_LOG(INFO, SWITCH, "Core %u is doing TX for port %u\n", rte_lcore_id(), app.ports);
//     init_tx(port_id);
//     while(!force_quit) {
//         app_main_tx_port(port_id);
//     }
// }

void app_main_tx_port(struct rte_ring *ring)
{
    struct rte_mbuf *pkt;
    uint64_t current_time, prev_time = app.prev_time;
    uint64_t tx_rate_scale = app.tx_rate_scale;
    uint16_t n_mbufs, n_pkts;
    uint64_t token = app.token;
    int ret;

    n_mbufs = app.mbuf_tx.n_mbufs;

    current_time = rte_get_tsc_cycles();
    if (app.tx_rate_mbps > 0)
    {
        // tbf: generate tokens
        token += ((tx_rate_scale * (current_time - prev_time)) >> RATE_SCALE);
        token = MIN(token, (app.bucket_size << 1));
        app.prev_time = current_time;
        if (token < app.bucket_size)
        {
            app.token = token;
            return;
        }
    }
    ret = rte_ring_sc_dequeue(
        ring,
        (void **)&app.mbuf_tx.array[n_mbufs]);

    if (ret == -ENOENT)
    { /* no packets in tx ring */
        return;
    }

    pkt = app.mbuf_tx.array[n_mbufs];
    app.qlen_bytes_out += pkt->pkt_len;
    app.qlen_pkts_out++;
    if (app.tx_rate_mbps > 0)
    {
        token -= pkt->pkt_len;
        app.token = token;
    }

    n_mbufs++;

    RTE_LOG(
        DEBUG, SWITCH,
        "%s: port %u receive %u packets\n",
        __func__, app.ports, n_mbufs);

    if (n_mbufs < app.burst_size_tx_write)
    {
        app.mbuf_tx.n_mbufs = n_mbufs;
        return;
    }

    uint16_t k = 0;
    do
    {
        n_pkts = rte_eth_tx_burst(
            app.ports,
            0,
            &app.mbuf_tx.array[k],
            n_mbufs - k);
        k += n_pkts;
        if (k < n_mbufs)
        {
            RTE_LOG(
                DEBUG, SWITCH,
                "%s: Transmit ring is full in port %u\n",
                __func__, app.ports);
        }
    } while (k < n_mbufs);

    app.mbuf_tx.n_mbufs = 0;
    rte_pktmbuf_free(pkt);
}
