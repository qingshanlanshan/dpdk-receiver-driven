#include "main.h"

// user functions & data
struct rcved_seq
{
    uint32_t sequence_number;
    struct rcved_seq *next;
};
void append(struct rcved_seq *l, uint32_t seq)
{
    struct rcved_seq *temp = rte_malloc_socket(NULL, sizeof(struct rcved_seq), RTE_CACHE_LINE_SIZE, rte_socket_id());
    temp->sequence_number = seq;
    temp->next = l->next;
    l->next = temp;
}
bool del(struct rcved_seq *l, uint32_t seq, bool any)
{
    struct rcved_seq *p = l;
    while (!force_quit && p->next)
    {
        if (p->next->sequence_number == seq || any)
        {
            struct rcved_seq *t = p->next;
            p->next = p->next->next;
            rte_free(t);
            if (!any)
                return 1;
        }
        p = p->next;
    }
    return 0;
}
struct rdp_info
{
    uint32_t expected_sequence_number;
    struct pkt_hdr hdr;
    uint64_t n_pull;
    bool send;
    uint64_t timestamp;
    uint64_t pull_gen_time;
    uint64_t RTT;
    uint64_t last_credit_feedback_ts;
    double w;
    double cur_rate;
    uint32_t credit_tot;
    uint32_t credit_dropped;
    uint64_t rtx_ts;
    bool phase;
    struct rcved_seq *list;
};

// module required functions
void init(struct rdp_params *rdp)
{
    rdp->info = rte_malloc_socket(NULL, sizeof(struct rdp_info), RTE_CACHE_LINE_SIZE, rte_socket_id());
    rdp->info->expected_sequence_number = 0;
    rdp->info->hdr.sequence_number = 0;
    rdp->info->hdr.flags.pull = !app.sender;
    rdp->info->n_pull = 0;
    rdp->info->send = 1;
    rdp->info->timestamp = rte_get_tsc_cycles();
    rdp->info->pull_gen_time = 1.0 * rdp->cpu_freq / app.default_speed * 2 * 8 * (sizeof(struct pkt_hdr) + app.data_size * sizeof(char)) / (1 << 20);
    rdp->info->RTT = 0;
    rdp->info->last_credit_feedback_ts = 0;
    rdp->info->w = 0.5;
    rdp->info->cur_rate = app.default_speed;
    rdp->info->credit_tot = 0;
    rdp->info->credit_dropped = 0;
    rdp->info->rtx_ts = rte_get_tsc_cycles();
    rdp->info->phase = 1;
    // rdp->info->list = rte_malloc_socket(NULL, sizeof(struct rcved_seq), RTE_CACHE_LINE_SIZE, rte_socket_id());
    // rdp->info->list->next = NULL;
}

void S_preloop(struct rdp_params *rdp)
{
    rdp->info->hdr.flags.syn = 1;
    uint64_t now_time;
    while (!force_quit)
    {
        now_time = rte_get_tsc_cycles();
        if (now_time - rdp->info->timestamp > rdp->cpu_freq)
        {
            HDR *hdr = get_hdr(rcv_pkt(rdp));
            if (hdr && hdr->flags.pull)
            {
                rdp->info->hdr.flags.syn = 0;
                PKT *p = new_pkt();
                rdp->info->hdr.sequence_number = hdr->sequence_number;
                prepend_hdr(p, &rdp->info->hdr);
                append_data_zero(p, app.data_size);
                enqueue_pkt(p);
                break;
            }
            rdp->info->timestamp = now_time;
            PKT *p = new_pkt();
            prepend_hdr(p, &rdp->info->hdr);
            append_data_zero(p, app.data_size);
            enqueue_pkt(p);
        }
    }
}

void S_loop(struct rdp_params *rdp)
{
    PKT *p;
    HDR *hdr = get_hdr(rcv_pkt(rdp));
    if (hdr)
    {

        if (hdr->flags.stop)
        {
            rdp->info->send = 0;
        }
        if (hdr->flags.syn)
        {
            rdp->info->send = 1;
        }
        if (hdr->flags.pull && rdp->info->send)
        {
            p = new_pkt();
            rdp->info->hdr.sequence_number = hdr->sequence_number;
            prepend_hdr(p, &rdp->info->hdr);
            append_data_zero(p, app.data_size);
            enqueue_pkt(p);
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
        hdr = get_hdr(rcv_pkt(rdp));
        if (hdr && hdr->flags.syn)
        {
            rdp->info->expected_sequence_number = 0;
            rdp->info->hdr.sequence_number = rdp->info->expected_sequence_number;
            break;
        }
    }
}

void R_loop(struct rdp_params *rdp)
{
    uint64_t now_time = rte_get_tsc_cycles();
    if (rdp->info->send && now_time - rdp->info->timestamp > rdp->info->pull_gen_time)
    {
        rdp->info->credit_tot++;
        PKT *p = new_pkt();
        rdp->info->hdr.sequence_number++;
        rdp->info->timestamp = now_time;
        prepend_hdr(p, &rdp->info->hdr);
        enqueue_pkt(p);
    }
    HDR *hdr = get_hdr(rcv_pkt(rdp));
    if (hdr)
    {
        if (hdr->flags.end)
            rdp->info->send = 0;

        if (hdr->sequence_number == rdp->info->expected_sequence_number)
        {
            rdp->info->expected_sequence_number++;
            // while (!force_quit && del(rdp->info->list, rdp->info->expected_sequence_number, 0))
            // {
            //     rdp->info->expected_sequence_number++;
            //     rdp->info->rtx_ts = rte_get_tsc_cycles();
            // }
        }
        else if (hdr->sequence_number > rdp->info->expected_sequence_number)
        {
            rdp->info->credit_dropped += (hdr->sequence_number - rdp->info->expected_sequence_number);
            // append(rdp->info->list, hdr->sequence_number);
            rdp->info->hdr.sequence_number = rdp->info->expected_sequence_number;
        }
        else
        {
            RTE_LOG(WARNING, SWITCH, "pkt seq < expected seq\n");
        }
    }
    // now_time = rte_get_tsc_cycles();
    // if (now_time - rdp->info->rtx_ts > rdp->cpu_freq * 2)
    // {
    //     rdp->info->hdr.sequence_number = rdp->info->expected_sequence_number;
    //     rdp->info->rtx_ts = now_time;
    //     del(rdp->info->list, 0, 1);
    // }
    now_time = rte_get_tsc_cycles();
    if (now_time - rdp->info->last_credit_feedback_ts > rdp->info->RTT)
    {
        double credit_loss = 1.0 * rdp->info->credit_dropped / rdp->info->credit_tot;
        if (credit_loss <= 0.1)
        {
            if (rdp->info->phase)
            {
                rdp->info->w = (rdp->info->w + 0.5) / 2;
            }
            rdp->info->cur_rate = (1 - rdp->info->w) * rdp->info->cur_rate + rdp->info->w * app.default_speed * (1 + 0.1);
            rdp->info->phase = 1;
        }
        else
        {
            rdp->info->cur_rate *= (1 - credit_loss) * (1 + 0.1);
            rdp->info->w = rdp->info->w > 0.01 ? rdp->info->w : 0.01;
            rdp->info->phase = 0;
        }
        rdp->info->cur_rate = rdp->info->cur_rate < app.default_speed / 2 ? app.default_speed / 2 : rdp->info->cur_rate;
        rdp->info->pull_gen_time = 1.0 * rdp->cpu_freq / rdp->info->cur_rate * 8 * (sizeof(struct pkt_hdr) + app.data_size * sizeof(char)) / (1 << 20);
        rdp->info->credit_dropped = 0;
        rdp->info->credit_tot = 0;
    }
}

__rte_unused void R_postloop(__rte_unused struct rdp_params *rdp)
{
}