#include "main.h"

struct rte_mbuf *new_pkt(void)
{
    return rte_pktmbuf_alloc(app.pool);
}

void prepend_hdr(PKT *p, struct pkt_hdr *hdr)
{
    char *ret;
    ret = rte_pktmbuf_prepend(p, sizeof(struct pkt_hdr));
    rte_memcpy(ret, hdr, sizeof(struct pkt_hdr));
}

void append_data(PKT *p, char *data, int data_size)
{
    char *ret;
    ret = rte_pktmbuf_append(p, sizeof(char) * data_size);
    rte_memcpy(ret, data, data_size);
}
void append_data_zero(PKT *p, int data_size)
{
    char *ret = rte_pktmbuf_append(p, sizeof(char) * data_size);
    memset(ret, 0, sizeof(char) * data_size);
}
void enqueue_pkt(PKT *p)
{
    rte_ring_mp_enqueue(app.rings_tx, p);
}

struct pkt_hdr *rcv_pkt(struct rdp_params *rdp)
{
    if (rte_ring_sc_dequeue(app.rings_flow[rdp->flowid], (void **)rdp->worker_mbuf->array) == -ENOENT)
        return NULL;

    return rte_pktmbuf_mtod(rdp->worker_mbuf->array[0], struct pkt_hdr *);
}