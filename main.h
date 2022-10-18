/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <confuse.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>

#ifndef APP_MBUF_ARRAY_SIZE
#define APP_MBUF_ARRAY_SIZE 256
#endif

#define RTE_LOGTYPE_SWITCH RTE_LOGTYPE_USER1

#undef RTE_LOG_LEVEL
#define RTE_LOG_LEVEL RTE_LOG_DEBUG

#define FORWARD_ENTRY 10 // # of forwarding table entries
#define MAX_NAME_LEN 100
#define VALID_TIME INT_MAX // valid time (in ms) for a forwarding item
#define MEAN_PKT_SIZE 800  // used for calculate ring length and # of mbuf pools
#define RATE_SCALE 20      // the scale of tx rate
#define MAX_FLOW 16

#define MIN(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

typedef uint32_t (*get_threshold_callback_fn)(void);
struct app_mbuf_array
{
    struct rte_mbuf *array[APP_MBUF_ARRAY_SIZE];
    uint16_t n_mbufs;
};

#ifndef APP_MAX_PORTS
#define APP_MAX_PORTS 1
#endif

struct app_fwd_table_item
{
    uint8_t port_id;
    /* the time (in cpu frequency) when the item is added */
    uint64_t timestamp;
};

struct app_configs
{
    cfg_bool_t shared_memory;
    long buffer_size_kb;
    long dt_shift_alpha;
    char *bm_policy;
    cfg_bool_t log_qlen;
    long log_qlen_port;
    char *qlen_fname;
    cfg_bool_t ecn_enable;
    long ecn_thresh_kb;
    long tx_rate_mbps;
    long bucket_size; /* bucket size (in bytes) of TBF algorithm */
    struct header *hdr;
    int default_speed;
    int sender;
    cfg_t *cfg;
};

extern struct app_configs app_cfg;

extern volatile bool force_quit;
struct app_params
{
    uint64_t cpu_freq[RTE_MAX_LCORE];
    uint64_t start_cycle;
    /* CPU cores */
    uint32_t n_lcores;
    uint32_t core_rx;
    uint32_t core_worker;
    uint32_t core_tx;
    uint32_t core_log;

    /* Ports*/
    uint32_t ports;
    uint32_t n_ports;
    uint32_t port_rx_ring_size;
    uint32_t port_tx_ring_size;

    /* buffer size */
    uint32_t buff_size_bytes;
    uint32_t buff_size_per_port_bytes;
    uint32_t
        shared_memory : 1, /* whether enable shared memory */
        /* whether log queue length and the file to put log in */
        log_qlen : 1,
        /* the port to log queue length */
        log_qlen_port : 5,
        dt_shift_alpha : 14, /* parameter alpha of DT = 1 << dt_shift_alpha*/
        ecn_enable : 1,
        unused : 10;
    uint64_t qlen_start_cycle;

    FILE *qlen_file;
    /*rte_rwlock_t lock_bocu;*/
    rte_spinlock_t lock_buff;
    get_threshold_callback_fn get_threshold;

    /* buffer occupancy*/
    uint32_t buff_bytes_in;
    uint32_t buff_bytes_out;
    uint32_t buff_pkts_in;
    uint32_t buff_pkts_out;
    /* queue length*/
    /*rte_rwlock_t lock_qlen;*/
    uint32_t qlen_bytes_in;
    uint32_t qlen_bytes_out;
    uint32_t qlen_pkts_in;
    uint32_t qlen_pkts_out;
    /* Rings */
    // struct rte_ring *rings_rx;
    struct rte_ring *rings_tx;
    uint32_t ring_rx_size;
    uint32_t ring_tx_size;
    struct rte_ring *rings_pull;

    struct rte_ring *rings_flow[MAX_FLOW];

    /* Internal buffers */
    struct app_mbuf_array mbuf_rx;
    struct app_mbuf_array mbuf_tx;
    // struct app_mbuf_array mbuf_flow[MAX_FLOW];

    /* Buffer pool */
    struct rte_mempool *pool;
    uint32_t pool_buffer_size;
    uint32_t pool_size;
    uint32_t pool_cache_size;

    /* Burst sizes */
    uint32_t burst_size_rx_read;
    uint32_t burst_size_rx_write;
    uint32_t burst_size_worker_read;
    uint32_t burst_size_worker_write;
    uint32_t burst_size_tx_read;
    uint32_t burst_size_tx_write;

    /* App behavior */
    // uint32_t pipeline_type;

    /* things about forwarding table */
    struct app_fwd_table_item fwd_table[FORWARD_ENTRY];
    char ft_name[MAX_NAME_LEN]; /* forward table name */
    struct rte_hash *l2_hash;
    uint64_t fwd_item_valid_time; /* valide time of forward item, in CPU cycles */

    uint32_t ecn_thresh_kb;
    uint64_t tx_rate_scale; /* tx rate in bytes per CPU cycle * (1<<RATE_SCALE)*/
    uint64_t tx_rate_mbps;  /* the rate (in byte per second) from tx ring to tx queue */
    uint32_t bucket_size;   /* bucket size (in bytes) of TBF algorithm */
    uint64_t token;         /* # of tokens in each port */
    uint64_t prev_time;     /* previous time in cycles to generate tokens */
    struct header *hdr;
    uint32_t pull_number;
    uint32_t sequence_number;
    int default_speed;
    uint64_t pull_gen_time;
    int tot_pkt;
    int sender;
    int data_size;
    int start;
    int n_flow;
} __rte_cache_aligned;

extern struct app_params app;

int app_parse_args(int argc, char **argv);
void app_print_usage(void);
void app_init(void);
int app_lcore_main_loop(void *arg);

void app_main_loop_rx(void);
void app_main_loop_forwarding(void);
void app_main_loop_tx_each_port(uint32_t);
void app_main_loop_tx(void);
void app_main_tx_port(struct rte_ring *);
void app_main_loop_pkt_gen(void);
void app_main_loop_test(void);
void app_main_loop_distribute(void);
// void app_main_loop_in_pkt(void);
struct header
{
    struct ether_hdr eth;
    struct ipv4_hdr ip;
    uint32_t src_port;
    uint32_t dst_port;
};

/*
 * Initialize forwarding table.
 * Return 0 when OK, -1 when there is error.
 */
int app_init_forwarding_table(const char *tname);

/*
 * Return 0 when OK, -1 when there is error.
 */
int app_l2_learning(const struct ether_addr *srcaddr, uint8_t port);

/*
 * Return port id to forward (broadcast if negative)
 */
int app_l2_lookup(const struct ether_addr *addr);

uint32_t get_qlen_bytes(void);
uint32_t get_buff_occu_bytes(void);
/*
 * Wrapper for enqueue
 * Returns:
 *  0: succeed, < 0: packet dropped
 *  -1: queue length > threshold, -2: buffer overflow, -3: other unknown reason
 */
int packet_enqueue(struct rte_mbuf *pkt);

/*
 * Get port qlen threshold for a port
 * if queue length for a port is larger than threshold, then packets are dropped.
 */
uint32_t qlen_threshold_equal_division(void);
/* dynamic threshold */
uint32_t qlen_threshold_dt(void);

#define PKT struct rte_mbuf
#define HDR struct pkt_hdr



struct pkt_hdr
{
    // struct ether_hdr eth;
    // struct ipv4_hdr ip;
    // uint32_t src_port;
    // uint32_t dst_port;
    struct
    {
        bool pull; // 0=data 1=pull
        bool syn;
        bool end;
        bool ack;
        bool nack;
        bool stop;
    } flags;
    uint32_t sequence_number;
    int flowid;

} __attribute__((packed));

struct rdp_params
{
    uint64_t expected_sequence_number;
    struct pkt_hdr hdr;
    uint64_t n_pull;
    bool send;
    uint64_t timestamp;
    struct app_mbuf_array *worker_mbuf;
    int flowid;
};
// extern struct rdp_params rdp;

struct rte_mbuf *new_pkt(void);
void set_hdr(PKT *p, struct pkt_hdr *hdr);
void set_data_zero(PKT *p, int data_size);
void set_data(PKT *p, char *data, int data_size);
void enqueue_pkt(PKT *p);
struct pkt_hdr *rcv_pkt(struct rdp_params *);

void init(struct rdp_params *);
void rdp_sender(void);
void rdp_receiver(void);
void S_preloop(struct rdp_params*);
void S_loop(struct rdp_params*);
void R_preloop(struct rdp_params*);
void R_loop(struct rdp_params*);

#define APP_FLUSH 0
#ifndef APP_FLUSH
#define APP_FLUSH 0x3FF
#endif

#define APP_METADATA_OFFSET(offset) (sizeof(struct rte_mbuf) + (offset))

#endif /* _MAIN_H_ */
