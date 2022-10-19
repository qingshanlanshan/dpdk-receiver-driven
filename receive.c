#include "main.h"

void app_main_loop_rx(void)
{
    uint32_t i;
    int ret;
    struct pkt_hdr *hdr;
    RTE_LOG(INFO, SWITCH, "Core %u is doing RX\n", rte_lcore_id());

    app.cpu_freq[rte_lcore_id()] = rte_get_tsc_hz();
    for (i = 0; !force_quit; i = ((i + 1) & (app.n_ports - 1)))
    {
        uint16_t n_mbufs;

        n_mbufs = rte_eth_rx_burst(
            app.ports,
            0,
            app.mbuf_rx.array,
            app.burst_size_rx_read);
        if (n_mbufs >= app.burst_size_rx_read)
        {
            RTE_LOG(
                DEBUG, SWITCH,
                "%s: receive %u packets from port %u\n",
                __func__, n_mbufs, app.ports);
        }

        if (n_mbufs == 0)
            continue;

        for (int i = 0; i < n_mbufs; ++i)
        {
            hdr = rte_pktmbuf_mtod(app.mbuf_rx.array[i], struct pkt_hdr *);
            do
            {
                ret = rte_ring_sp_enqueue(
                    app.rings_flow[hdr->flowid],
                    app.mbuf_rx.array[i]);
            } while (ret == -ENOBUFS);
        }
    }
}
