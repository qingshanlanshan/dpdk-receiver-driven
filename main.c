#include <signal.h>
#include <stdio.h>
#include "main.h"

volatile bool force_quit;

static void
signal_handler(int signum)
{
    switch (signum)
    {
    case SIGTERM:
    case SIGINT:
        RTE_LOG(
            INFO, SWITCH,
            "%s: Receive %d signal, prepare to exit...\n",
            __func__, signum);
        force_quit = true;
        break;
    }
}

static void
app_quit(void)
{
    // uint8_t i;
    /* close ports */

    uint8_t port = (uint8_t)app.ports;
    RTE_LOG(
        INFO, SWITCH,
        "%s: Closing NIC port %u ...\n",
        __func__, port);

    rte_eth_dev_stop(port);
    rte_eth_dev_close(port);

    /* free resources */
    /*if (app_cfg.cfg != NULL) {
        cfg_free(app_cfg.cfg);
    }
    if (app_cfg.bm_policy != NULL) {
        free(app_cfg.bm_policy);
    }
    if (app_cfg.qlen_fname != NULL) {
        free(app_cfg.qlen_fname);
    }*/
    /* close files */
    if (app.log_qlen)
    {
        fclose(app.qlen_file);
    }
    printf("App quit. Bye...\n");
}

int main(int a, char **b)
{

    uint32_t lcore;
    int ret;
    int argc = a - 6;
    char **argv = (char **)malloc(sizeof(char *) * argc);
    for (int i = 0; i < argc; ++i)
    {
        argv[i] = b[i];
        printf("%s ", argv[i]);
    }
    printf("\n");
    /* Init EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        return -1;

    argc -= ret;
    argv += ret;

    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Parse application arguments (after the EAL ones) */
    ret = app_parse_args(argc, argv);
    if (ret < 0)
    {
        app_print_usage();
        return -1;
    }



    char *optstr = "s:r:f:";
    struct option opts[] = {
        {"sender", 1, NULL, 's'},
        {"default_speed", 1, NULL, 'r'},
        {"n_flow", 1, NULL, 'f'},
        {0, 0, 0, 0},
    };
    int opt;
    char **argv1 = (char **)malloc(sizeof(char *) * 7);
    for (int i = 0; i < 7; ++i)
    {
        argv1[i] = b[a-7+i];
        printf("%s ", argv1[i]);
    }
    printf("\n");

    while ((opt = getopt_long(7, argv1, optstr, opts, NULL)) != -1)
    {
        switch (opt)
        {
        case 's':
            app.sender = atoi(optarg);
            break;
        case 'r':
            app.default_speed = atoi(optarg);
            break;
        case 'f':
            app.n_flow = atoi(optarg);
            break;
        default:
            continue;
        }
    }
    
    /* Init */
    app_init();
    RTE_LOG(INFO, SWITCH, "%s: flow number = %d, default speed=%dMbps\n", app.sender ? "sender" : "receiver", app.n_flow,app.default_speed);
    app.start_cycle = rte_get_tsc_cycles();
    /* Launch per-lcore init on every lcore */

    rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
    RTE_LCORE_FOREACH_SLAVE(lcore)
    {
        if (rte_eal_wait_lcore(lcore) < 0)
        {
            return -1;
        }
    }

    app_quit();
    fflush(stdout);

    return 0;
}

int app_lcore_main_loop(__attribute__((unused)) void *arg)
{
    unsigned lcore;

    lcore = rte_lcore_id();

    if (lcore == app.core_rx)
    {
        app_main_loop_rx();
        return 0;
    }

    // if (lcore == app.core_worker)
    // {
    //     app_main_loop_pkt_gen();
    //     return 0;
    // }

    if (lcore == app.core_tx)
    {
        app_main_loop_tx();
        return 0;
    }
    if (lcore == app.core_log)
    {
        app_main_loop_test();
        return 0;
    }

    if (app.n_lcores >= 3 + app.n_flow)
    {
        for (int i = 0; i < app.n_flow; i++)
        {
            if (lcore == app.core_worker[i])
            {
                app_main_loop_pkt_gen_each_flow(i);
                return 0;
            }
        }
    }
    else
    {
        if (lcore == app.core_worker[0])
        {
            app_main_loop_pkt_gen();
            return 0;
        }
    }

    return 0;
}
