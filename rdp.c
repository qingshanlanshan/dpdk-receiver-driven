#include "main.h"

void init(struct rdp_params *rdp)
{
    rdp->expected_sequence_number = 0;
    rdp->hdr.sequence_number = 0;
    rdp->hdr.flags.pull = !app.sender;
    rdp->n_pull = 0;
    rdp->send = 1;
    rdp->timestamp = 0;
    rdp->worker_mbuf = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),RTE_CACHE_LINE_SIZE, rte_socket_id());
}

void S_preloop(struct rdp_params *rdp)
{
    rdp->hdr.flags.syn = 1;
    for (int i = 0; i < 10 && !force_quit; ++i)
    {
        PKT *p = new_pkt();
        prepend_hdr(p, &rdp->hdr);
        append_data_zero(p, app.data_size);
        enqueue_pkt(p);
    }
    rdp->hdr.flags.syn = 0;
}

void S_loop(struct rdp_params *rdp)
{
    if (rdp->n_pull && rdp->send)
    {
        PKT *p = new_pkt();
        rdp->hdr.sequence_number++;
        prepend_hdr(p, &rdp->hdr);
        append_data_zero(p, app.data_size);
        enqueue_pkt(p);
        rdp->n_pull--;
    }
    HDR *hdr = rcv_pkt(rdp);
    if (hdr)
    {
        if (hdr->flags.pull)
        {
            rdp->n_pull++;
        }
        if (hdr->flags.stop)
        {
            rdp->send = 0;
        }
        if (hdr->flags.syn)
        {
            rdp->send = 1;
        }
    }
}
__rte_unused void S_postloop(__rte_unused struct rdp_params *rdp)
{

}

void R_preloop(struct rdp_params *rdp)
{
    HDR *hdr;
    while (!force_quit)
    {
        hdr = rcv_pkt(rdp);
        if (hdr && hdr->flags.syn)
        {
            rdp->expected_sequence_number = hdr->sequence_number ? 0 : 1;
            rdp->n_pull++;
            rdp->hdr.sequence_number = rdp->expected_sequence_number;
            break;
        }
    }
}

void R_loop(struct rdp_params *rdp)
{
    uint64_t now_time = rte_get_tsc_cycles();
    if (rdp->send && rdp->n_pull && (app.pull_gen_time || now_time - rdp->timestamp > app.pull_gen_time))
    {
        rdp->timestamp = now_time;
        PKT *p = new_pkt();
        prepend_hdr(p, &rdp->hdr);
        rdp->hdr.sequence_number++;
        enqueue_pkt(p);
        rdp->n_pull--;
    }
    HDR *hdr = rcv_pkt(rdp);
    if (hdr)
    {
        rdp->n_pull++;
        if (hdr->flags.end)
        {
            rdp->send = 0;
        }
    }
}
__rte_unused void R_postloop(__rte_unused struct rdp_params *rdp)
{
    
}