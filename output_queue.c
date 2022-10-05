#include "main.h"

uint32_t
qlen_threshold_equal_division(void)
{
    uint32_t result = app.buff_size_bytes / app.n_ports;
    return result;
}

uint32_t
qlen_threshold_dt(void)
{
    return ((app.buff_size_bytes - get_buff_occu_bytes()) << app.dt_shift_alpha);
}

uint32_t get_qlen_bytes(void)
{
    return app.qlen_bytes_in - app.qlen_bytes_out;
}

uint32_t get_buff_occu_bytes(void)
{
    uint32_t result = 0;

    result += (app.qlen_bytes_in - app.qlen_bytes_out);

    return result;
    // return app.buff_bytes_in - app.buff_bytes_out;
}

int packet_enqueue( struct rte_mbuf *pkt)
{
    int ret = 0;
    uint32_t qlen_bytes = get_qlen_bytes();
    uint32_t threshold = 0;
    uint32_t qlen_enque = qlen_bytes + pkt->pkt_len;
    uint32_t buff_occu_bytes = 0;

    /*Check whether buffer overflows after enqueue*/
    if (app.shared_memory)
    {
        buff_occu_bytes = get_buff_occu_bytes();
        threshold = app.get_threshold();
        if (qlen_enque > threshold)
        {
            ret = -1;
        }
        else if (buff_occu_bytes + pkt->pkt_len > app.buff_size_bytes)
        {
            ret = -2;
        }
    }
    else if (qlen_enque > app.buff_size_per_port_bytes)
    {
        ret = -2;
    }

    if (ret == 0)
    {
        int enque_ret = rte_ring_sp_enqueue(
            app.rings_tx,
            pkt);
        if (enque_ret != 0)
        {
            RTE_LOG(
                ERR, SWITCH,
                "%s: packet cannot enqueue in port",
                __func__);
        }
        app.qlen_bytes_in += pkt->pkt_len;
        app.qlen_pkts_in++;
        /*app.buff_bytes_in += pkt->pkt_len;
        app.buff_pkts_in ++;*/
    }
    else
    {
        rte_pktmbuf_free(pkt);
    }
    switch (ret)
    {
    case 0:
        RTE_LOG(
            DEBUG, SWITCH,
            "%s: packet enqueue to port\n",
            __func__);
        break;
    case -1:
        RTE_LOG(
            DEBUG, SWITCH,
            "%s: Packet dropped due to queue length > threshold\n",
            __func__);
        break;
    case -2:
        RTE_LOG(
            DEBUG, SWITCH,
            "%s: Packet dropped due to buffer overflow\n",
            __func__);
    case -3:
        RTE_LOG(
            DEBUG, SWITCH,
            "%s: Cannot mark packet with ECN, drop packet\n",
            __func__);
    }
    return ret;
}
