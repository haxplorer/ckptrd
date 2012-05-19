#include "lam-ssi-ckptrd-includes.h"

int main(int argc, void **argv)
{
    if(argc != 2)
    {
        printf("Usage: detatch [all | <pid_t>]\n");
        return 0;
    }
    int shmid, i, *procs = lam_ssi_ckptrd_get_procs();
    int target = -1;

    if(strcmp(argv[1], "all") != 0)
        target = atoi(argv[1]);

    for(i=0;i<MAX_PID;i++)
    {
        if(procs[i] == 0 || (target != -1 && i != target))
            continue;

        if((shmid = shmget(i, MAX_PROC_SHM, 0664)) != -1)
            shmctl(shmid, IPC_RMID, NULL);
        procs[i] = 0;
    }

    return 0;
}
