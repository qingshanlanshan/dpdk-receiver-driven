#include "main.h"

struct app_params app = {
    /* Ports*/
    .n_ports = 1,
    .port_rx_ring_size = 128,
    .port_tx_ring_size = 32,

    /* switch buffer */
    .shared_memory = 0,
    .buff_size_bytes = (256 << 10),
    .buff_bytes_in = 0,
    .buff_pkts_in = 0,
    .buff_bytes_out = 0,
    .buff_pkts_out = 0,
    .log_qlen = 0,
    .qlen_file = NULL,
    .get_threshold = qlen_threshold_equal_division,
    .dt_shift_alpha = 0,
    .qlen_start_cycle = 0,

    /* Rings */
    .ring_rx_size = 128,
    /* Notes: this value can be changed in function app_init_rings()*/
    .ring_tx_size = 128,

    /* Buffer pool */
    .pool_buffer_size = 2048 + RTE_PKTMBUF_HEADROOM,
    /* Notes: this value can be changed in function app_init_mbuf_pools*/
    .pool_size = 32 * 1024 - 1,
    .pool_cache_size = 256,

    /* Burst sizes */
    .burst_size_rx_read = 64,
    .burst_size_rx_write = 64,
    .burst_size_worker_read = 1,
    .burst_size_worker_write = 1,
    .burst_size_tx_read = 1,
    .burst_size_tx_write = 1,

    /* forwarding things */
    .ft_name = "Forwarding Table",
    .l2_hash = NULL,

    .ecn_enable = 0,
    .ecn_thresh_kb = 0,
    .tx_rate_mbps = 0,
    .bucket_size = 3200,
};
// struct rdp_params rdp = {
//     .expected_sequence_number = 0,
// };
static struct rte_eth_conf port_conf = {
    .rxmode = {
        .split_hdr_size = 0,
        .header_split = 0,   /* Header Split disabled */
        .hw_ip_checksum = 1, /* IP checksum offload enabled */
        .hw_vlan_filter = 0, /* VLAN filtering disabled */
        .jumbo_frame = 0,    /* Jumbo Frame Support disabled */
        .hw_strip_crc = 1,   /* CRC stripped by hardware */
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
            .rss_hf = ETH_RSS_IP,
        },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    },
};

static struct rte_eth_rxconf rx_conf = {
    .rx_thresh = {
        .pthresh = 8,
        .hthresh = 8,
        .wthresh = 4,
    },
    .rx_free_thresh = 64,
    .rx_drop_en = 0,
};

static struct rte_eth_txconf tx_conf = {
    .tx_thresh = {
        .pthresh = 36,
        .hthresh = 0,
        .wthresh = 0,
    },
    .tx_free_thresh = 24,
    .tx_rs_thresh = 16,
};

/*
 * Change an arbitary value to a power of 2.
 * E.g., 0x5 --> 0x8, 0x36 --> 0x40
 */
static uint32_t
topower2(uint32_t x)
{
    uint32_t result = 0x1;
    while (((-result) & x) != 0)
    {
        result = result << 1;
    }
    return result;
}

static void
app_init_locks(void)
{
    /*uint32_t i;
    rte_rwlock_init(&app.lock_bocu);
    for (i = 0; i < app.n_ports; i++) {
        rte_rwlock_init(&app.lock_qlen[i]);
    }*/
    rte_spinlock_init(&app.lock_buff);
}

static void
app_init_mbuf_pools(void)
{
    /* Init the buffer pool */
    RTE_LOG(INFO, SWITCH, "Creating the mbuf pool ...\n");
    uint32_t temp_pool_size = (topower2(app.buff_size_bytes / MEAN_PKT_SIZE) << 2) - 1;
    app.pool_size = (app.pool_size < temp_pool_size ? temp_pool_size : app.pool_size);
    app.pool = rte_pktmbuf_pool_create("mempool", app.pool_size,
                                       app.pool_cache_size, 0, app.pool_buffer_size, rte_socket_id());
    if (app.pool == NULL)
        rte_panic("Cannot create mbuf pool\n");
}

static void
app_init_rings(void)
{
    app.ring_rx_size = (topower2(app.buff_size_bytes / MEAN_PKT_SIZE) << 2);

    // app.rings_rx = rte_ring_create(
    //     "app_ring_rx",
    //     app.ring_rx_size,
    //     rte_socket_id(),
    //     RING_F_SP_ENQ | RING_F_SC_DEQ);

    // if (app.rings_rx == NULL)
    //     rte_panic("Cannot create RX ring\n");

    for (int i = 0; i < app.n_flow; i++)
    {
        char name[32];

        snprintf(name, sizeof(name), "app_ring_flow_%u", i);

        app.rings_flow[i] = rte_ring_create(
            name,
            app.ring_rx_size,
            rte_socket_id(),
            RING_F_SP_ENQ | RING_F_SC_DEQ);

        if (app.rings_flow[i] == NULL)
            rte_panic("Cannot create flow ring %u\n", i);
    }

    app.ring_tx_size = (topower2(app.buff_size_bytes / MEAN_PKT_SIZE) << 2);

    app.rings_tx = rte_ring_create(
        "app_ring_tx",
        app.ring_tx_size,
        rte_socket_id(),
        RING_F_SP_ENQ | RING_F_SC_DEQ);

    if (app.rings_tx == NULL)
        rte_panic("Cannot create TX ring\n");

    app.rings_pull = rte_ring_create(
        "app_ring_pull",
        app.ring_tx_size,
        rte_socket_id(),
        RING_F_SP_ENQ | RING_F_SC_DEQ);

    if (app.rings_pull == NULL)
        rte_panic("Cannot create PULL ring\n");

    app.qlen_bytes_in = app.qlen_pkts_in = 0;
    app.qlen_bytes_out = app.qlen_pkts_out = 0;
}

static void
app_ports_check_link(void)
{
    struct rte_eth_link link;

    memset(&link, 0, sizeof(link));
    rte_eth_link_get_nowait(0, &link);
    RTE_LOG(INFO, SWITCH, "Port (%u Gbps) %s\n",
            link.link_speed / 1000,
            link.link_status ? "UP" : "DOWN");
}

static void
app_init_ports(void)
{
    // uint32_t i;

    /* Init NIC ports, then start the ports */

    int ret;

    RTE_LOG(INFO, SWITCH, "Initializing NIC port  ...\n");

    /* Init port */
    ret = rte_eth_dev_configure(
        0,
        1,
        1,
        &port_conf);
    if (ret < 0)
        rte_panic("Cannot init NIC port (%d)\n", ret);

    rte_eth_promiscuous_enable(0);

    /* Init RX queues */
    ret = rte_eth_rx_queue_setup(
        0,
        0,
        app.port_rx_ring_size,
        rte_eth_dev_socket_id(0),
        &rx_conf,
        app.pool);
    if (ret < 0)
        rte_panic("Cannot init RX for port0 (%d)\n",
                  ret);

    /* Init TX queues */
    ret = rte_eth_tx_queue_setup(
        0,
        0,
        app.port_tx_ring_size,
        rte_eth_dev_socket_id(0),
        &tx_conf);
    if (ret < 0)
        rte_panic("Cannot init TX for port (%d)\n",
                  ret);

    /* Start port */
    ret = rte_eth_dev_start(0);
    if (ret < 0)
        rte_panic("Cannot start port (%d)\n", ret);

    sleep(1);
    app_ports_check_link();
}

void app_init(void)
{
    app_init_mbuf_pools();
    app_init_rings();
    app_init_ports();
    app_init_locks();

    RTE_LOG(INFO, SWITCH, "Initialization completed\n");
}
