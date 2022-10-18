#include "main.h"
#include <sys/wait.h>
#include <sys/prctl.h>

void app_main_loop_pkt_gen(void)
{
    int flowid;
    pid_t pid;
    for (flowid = 0; flowid < app.n_flow - 1; flowid++)
    {
        if ((pid = fork()) < 0)
        {
            RTE_LOG(DEBUG, SWITCH, "flowid=%d, fork failed\n", flowid);
        }
        else if (pid == 0)
        {
            prctl(PR_SET_PDEATHSIG, SIGKILL);
        }
        else
        {
            break;
        }
    }
    RTE_LOG(DEBUG, SWITCH, "pid=%d, flowid=%d\n", pid, flowid);

    struct rdp_params *rdp = (struct rdp_params *)malloc(sizeof(struct rdp_params));
    init(rdp);
    rdp->flowid = flowid;
    rdp->hdr.flowid = flowid;
    if (app.sender) // sender
    {
        S_preloop(rdp);

        while (!force_quit)
        {
            S_loop(rdp);
        }
    }
    else // receiver
    {
        R_preloop(rdp);
        while (!force_quit)
        {
            R_loop(rdp);
        }
    }

    if (flowid = app.n_flow - 1)
    {
        exit(0);
    }
    else
    {
        wait(0);
    }
}