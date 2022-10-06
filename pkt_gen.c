#include "main.h"

static void set_data_hdr(struct pkt_hdr *ack_hdr, uint32_t sequence_number)
{
    // ack_hdr->eth = app.hdr->eth;
    // ack_hdr->ip = app.hdr->ip;
    // ack_hdr->src_port = app.hdr->src_port;
    // ack_hdr->dst_port = app.hdr->dst_port;
    ack_hdr->flags.pull = 0;
    ack_hdr->sequence_number = sequence_number;
}
static void set_pull_hdr(struct pkt_hdr *pull_hdr, uint32_t pull_number)
{
    // pull_hdr->eth = app.hdr->eth;
    // pull_hdr->ip = app.hdr->ip;
    // pull_hdr->src_port = app.hdr->src_port;
    // pull_hdr->dst_port = app.hdr->dst_port;
    pull_hdr->flags.pull = 1;
    pull_hdr->pull_number = pull_number++;
}
static void set_ack_hdr(struct pkt_hdr *ack_hdr, uint32_t sequence_number, bool ack)
{
    // ack_hdr->eth = app.hdr->eth;
    // ack_hdr->ip = app.hdr->ip;
    // ack_hdr->src_port = app.hdr->src_port;
    // ack_hdr->dst_port = app.hdr->dst_port;
    ack_hdr->flags.pull = 0;
    if (ack)
    {
        ack_hdr->flags.ack = 1;
        ack_hdr->flags.nack = 0;
    }
    else
    {
        ack_hdr->flags.ack = 0;
        ack_hdr->flags.nack = 1;
    }
    ack_hdr->sequence_number = sequence_number;
}

void app_main_loop_pkt_gen(void)
{
    RTE_LOG(DEBUG, SWITCH, "%d\n", app.sender);
    if (app.sender) // sender
    {
        uint64_t last_time = rte_get_tsc_cycles();
        app.cpu_freq[rte_lcore_id()] = rte_get_tsc_hz();
        // app.default_speed = 100;
        int last_sequence_number = 0;
        int pull_to_gen = 0;
        int last_pull_number = -1;
        char *ret;




        // SYN pkt
        struct rte_mbuf *p = rte_pktmbuf_alloc(app.pool);
        struct pkt_hdr *data_hdr = (struct pkt_hdr *)malloc(sizeof(struct pkt_hdr));

        set_data_hdr(data_hdr, last_sequence_number++);
        data_hdr->flags.syn = 1;

        // header
        ret = rte_pktmbuf_prepend(p, sizeof(struct pkt_hdr));
        RTE_LOG(DEBUG, SWITCH, "%lu\n", ret);
        memcpy(ret, data_hdr, sizeof(struct pkt_hdr));

        // data
        ret = rte_pktmbuf_append(p, sizeof(char) * app.data_size);
        RTE_LOG(DEBUG, SWITCH, "%lu, data size=%d\n", ret, app.data_size);
        memset(ret, 0, sizeof(char) * app.data_size);

        // check pkt
        RTE_LOG(DEBUG, SWITCH, "%d, %lu\n", p->data_len, sizeof(struct pkt_hdr));
        struct pkt_hdr *hdr = rte_pktmbuf_mtod(p, struct pkt_hdr *);
        RTE_LOG(DEBUG, SWITCH, "syn=%d,seq=%d\n", hdr->flags.syn, hdr->sequence_number);

        rte_ring_sp_enqueue(app.rings_tx, p);
        data_hdr->flags.syn = 0;
        struct app_mbuf_array *worker_mbuf;
        uint32_t i;
        worker_mbuf = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),
                                        RTE_CACHE_LINE_SIZE, rte_socket_id());

        for (i = 0; !force_quit; i = ((i + 1) & (app.n_ports - 1)))
        {
            if (pull_to_gen > 0)
            {
                uint64_t now_time = rte_get_tsc_cycles();
                // if (now_time - last_time < app.pull_gen_time)
                //     continue;
                last_time = now_time;
                struct rte_mbuf *p = rte_pktmbuf_alloc(app.pool);
                // struct pkt_hdr *data_hdr;
                set_data_hdr(data_hdr, last_sequence_number++);

                memcpy(rte_pktmbuf_prepend(p, sizeof(struct pkt_hdr)), data_hdr, sizeof(struct pkt_hdr));
                memset(rte_pktmbuf_append(p, sizeof(char) * app.data_size), 0, sizeof(char) * app.data_size);
                rte_ring_sp_enqueue(app.rings_tx, p);
                pull_to_gen--;
            }

            int ret = rte_ring_sc_dequeue(
                app.rings_rx,
                (void **)worker_mbuf->array);

            if (ret == -ENOENT)
                continue;
            struct pkt_hdr *hdr = rte_pktmbuf_mtod(worker_mbuf->array[0], struct pkt_hdr *);
            RTE_LOG(DEBUG, SWITCH, "Received pkt: %s, SYN = %d, %s = %lu\n", hdr->flags.pull ? "PULL" : "DATA", hdr->flags.syn, hdr->flags.pull ? "PULL number" : "SEQ number", hdr->flags.pull ? hdr->pull_number : hdr->sequence_number);
            if (hdr->flags.pull)
            {
                if (hdr->pull_number > last_pull_number)
                {
                    pull_to_gen += hdr->pull_number - last_pull_number;
                    last_pull_number = hdr->pull_number;
                }
            }
        }
        free(data_hdr);
    }
    else // receiver
    {
        uint64_t last_time = rte_get_tsc_cycles();
        app.cpu_freq[rte_lcore_id()] = rte_get_tsc_hz();

        app.pull_gen_time = app.cpu_freq[rte_lcore_id()] / app.default_speed  * 8 * (sizeof(struct pkt_hdr) + app.data_size * sizeof(char))/(1 << 20);
        RTE_LOG(DEBUG,SWITCH,"cpu_freq=%lu, pull_gen_time=%lu\n",app.cpu_freq[ret_lcore_id()],app.pull_gen_time);
        bool start = 0;
        int last_sequence_number = 0;
        int pull_to_gen = 10;
        uint32_t pull_number = 0;
        struct app_mbuf_array *worker_mbuf;
        worker_mbuf = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),
                                        RTE_CACHE_LINE_SIZE, rte_socket_id());
        uint64_t now_time=rte_get_tsc_cycles();
        // int i;
        struct pkt_hdr *pull_hdr = (struct pkt_hdr *)malloc(sizeof(struct pkt_hdr));

        while (!force_quit)
        {

            now_time = rte_get_tsc_cycles();
            if (start && pull_to_gen && now_time - last_time > app.pull_gen_time)
            {
                last_time = now_time;
                struct rte_mbuf *p = rte_pktmbuf_alloc(app.pool);

                set_pull_hdr(pull_hdr, pull_number++);

                memcpy(rte_pktmbuf_prepend(p, sizeof(struct pkt_hdr)), pull_hdr, sizeof(struct pkt_hdr));

                RTE_LOG(DEBUG, SWITCH, "Sent pkt: %s, SYN = %d, %s = %lu\n", pull_hdr->flags.pull ? "PULL" : "DATA", pull_hdr->flags.syn, pull_hdr->flags.pull ? "PULL number" : "SEQ number", pull_hdr->flags.pull ? pull_hdr->pull_number : pull_hdr->sequence_number);

                rte_ring_sp_enqueue(app.rings_tx, p);
                pull_to_gen--;
            }

            int ret = rte_ring_sc_dequeue(
                app.rings_rx,
                (void **)worker_mbuf->array);

            if (ret == -ENOENT)
                continue;
            struct pkt_hdr *hdr = rte_pktmbuf_mtod(worker_mbuf->array[0], struct pkt_hdr *);
            RTE_LOG(DEBUG, SWITCH, "Received pkt: %s, SYN = %d, %s = %lu\n", hdr->flags.pull ? "PULL" : "DATA", hdr->flags.syn, hdr->flags.pull ? "PULL number" : "SEQ number", hdr->flags.pull ? hdr->pull_number : hdr->sequence_number);
            if (!start)
            {
                start = hdr->flags.syn;
                if (start)
                {
                    pull_to_gen++;
                }
                else
                {
                    continue;
                }
            }
            else if (hdr->flags.end)
            {
                start = 0;
                break;
            }
            else
            {
                pull_to_gen++;
            }

            // ack and nack

            // struct rte_mbuf *p = rte_pktmbuf_alloc(app.pool);
            // struct pkt_hdr *ack_hdr;
            // if (hdr->sequence_number - last_sequence_number > 1)
            //     set_ack_hdr(ack_hdr, hdr->sequence_number, 0);
            // else
            //     set_ack_hdr(ack_hdr, hdr->sequence_number, 1);

            // memcpy(rte_pktmbuf_mtod(p, void *), ack_hdr, sizeof(struct pkt_hdr));

            // rte_ring_sp_enqueue(app.rings_pull, p);
        }
        free(pull_hdr);
    }
}