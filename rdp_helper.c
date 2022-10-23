#include "main.h"

// return the pointer to a new pkt allocated from mempool
struct rte_mbuf *new_pkt(void)
{
    return rte_pktmbuf_alloc(app.pool);
}

// prepend header to a pkt
void prepend_hdr(PKT *p, struct pkt_hdr *hdr)
{
    char *ret;
    ret = rte_pktmbuf_prepend(p, sizeof(struct pkt_hdr));
    rte_memcpy(ret, hdr, sizeof(struct pkt_hdr));
}

// append data_size bytes of data to a pkt
void append_data(PKT *p, char *data, int data_size)
{
    char *ret;
    ret = rte_pktmbuf_append(p, sizeof(char) * data_size);
    rte_memcpy(ret, data, data_size);
}

// append data_size bytes of zero to a pkt
void append_data_zero(PKT *p, int data_size)
{
    char *ret = rte_pktmbuf_append(p, sizeof(char) * data_size);
    memset(ret, 0, sizeof(char) * data_size);
}

// enqueue a pkt to transmit ring
void enqueue_pkt(PKT *p)
{
    rte_ring_mp_enqueue(app.rings_tx, p);
}

// fill rdp->worker_mbuf->array with received pkt
// return pointer to the pkt, or NULL if failed
PKT *rcv_pkt(struct rdp_params *rdp)
{
    if (rte_ring_sc_dequeue(app.rings_flow[rdp->flowid], (void **)rdp->worker_mbuf->array) == -ENOENT)
        return NULL;

    return rdp->worker_mbuf->array[0];
}

// get the pointer to the hdr of a pkt
HDR* get_hdr(PKT *pkt)
{
    if(!pkt)
        return NULL;
    return rte_pktmbuf_mtod(pkt, struct pkt_hdr *);
}

// get the pointer to the data of a pkt
char* get_data(PKT *pkt)
{
    if(!pkt)
        return NULL;
    return rte_pktmbuf_mtod_offset(pkt, char *,sizeof(struct pkt_hdr));
}