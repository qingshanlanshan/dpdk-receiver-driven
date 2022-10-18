#include "main.h"

void set_data_hdr(struct pkt_hdr *ack_hdr, uint32_t sequence_number)
{
    // ack_hdr->eth = app.hdr->eth;
    // ack_hdr->ip = app.hdr->ip;
    // ack_hdr->src_port = app.hdr->src_port;
    // ack_hdr->dst_port = app.hdr->dst_port;
    ack_hdr->flags.pull = 0;
    ack_hdr->sequence_number = sequence_number;
}
void set_pull_hdr(struct pkt_hdr *pull_hdr, uint32_t pull_number)
{
    // pull_hdr->eth = app.hdr->eth;
    // pull_hdr->ip = app.hdr->ip;
    // pull_hdr->src_port = app.hdr->src_port;
    // pull_hdr->dst_port = app.hdr->dst_port;
    pull_hdr->flags.pull = 1;
    pull_hdr->pull_number = pull_number;
}
void __rte_unused set_ack_hdr(struct pkt_hdr *ack_hdr, uint32_t sequence_number, bool ack)
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
