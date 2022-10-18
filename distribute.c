#include "main.h"

void app_main_loop_distribute(void)
{   
    
    struct app_mbuf_array *worker_mbuf = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),RTE_CACHE_LINE_SIZE, rte_socket_id());
    if(worker_mbuf==NULL)RTE_LOG(DEBUG,SWITCH,"malloc error\n");
    int ret;
    
    while (!force_quit)
    {
        ret = rte_ring_sc_dequeue(
        app.rings_rx,
        (void **)worker_mbuf->array);
        if (ret == -ENOENT)
            continue;
        struct pkt_hdr *hdr = rte_pktmbuf_mtod(worker_mbuf->array[0], struct pkt_hdr *);
        rte_ring_sp_enqueue(app.rings_flow[hdr->flowid], worker_mbuf->array[0]);
        
    }
}