#include "lam-ssi-ckptrd-includes.h"

/* TODO: add a suitable structure into shared memory
 * with the corresponding mpirun's as id
 * -> a create operation
 * -> a read operation
 * -> a modify operation
 */

char log_buf[150];

void
lam_ssi_ckptrd_make_proc_mem()
{
    int shmid;    
    if ((shmid = shmget(PROC_KEY, MAX_PROC_SHM, 0664 | IPC_CREAT | IPC_EXCL)) == -1) 
    {
        if(errno != EEXIST)
            lam_ssi_ckptrd_log_message("Error creating a shared mem segment for proc", FATAL);
        return;
    }

    void *ptr = shmat(shmid, (void *)0, 0);
    if (ptr == (void *)(-1)) 
    {
        lam_ssi_ckptrd_log_message("Error creating a ptr to shared mem for ips", FATAL);
        return;
    }

    memset(ptr, 0, MAX_PROC_SHM);

    /* detatch finally as the parent doesnt need this data now */
    shmdt(ptr);

    return;
}

int *
lam_ssi_ckptrd_get_procs()
{
    int shmid;    
    if ((shmid = shmget(PROC_KEY, MAX_PROC_SHM, 0664)) == -1) 
    {
        char temp[25];
        switch(errno)
        {
            case EACCES:
                strcpy(temp, "EACCES");
                break;
            case EEXIST:
                strcpy(temp, "EEXIST");
                break;
            case EINVAL:
                strcpy(temp, "EINVAL");
                break;
            case ENFILE:
                strcpy(temp, "ENFILE");
                break;
            case ENOENT:
                strcpy(temp, "ENOENT");
                break;
            case ENOMEM:
                strcpy(temp, "ENOMEM");
                break;
            case ENOSPC:
                strcpy(temp, "ENOSPC");
                break;
            case EPERM:
                strcpy(temp, "EPERM");
                break;
        }

        sprintf(log_buf, "Error reading a shared mem segment for procs (%d - %s)", errno, temp);
        lam_ssi_ckptrd_log_message(log_buf, FATAL);
        return;
    }

    int *procs = (int *) shmat(shmid, (void *)0, 0);
    if (procs == (int *)(-1)) 
    {
        lam_ssi_ckptrd_log_message("Error reading a ptr to shared mem for procs", FATAL);
        return;
    }

    return procs; 
}


void
lam_ssi_ckptrd_make_ip_mem()
{
    int shmid;    
    if ((shmid = shmget(IP_KEY, MAX_IP_SHM, 0664 | IPC_CREAT)) == -1) 
    {
        sprintf(log_buf, "Error creating a shared mem segment for ips (%d)", errno);
        lam_ssi_ckptrd_log_message(log_buf, FATAL);
        return;
    }

    void *ptr = shmat(shmid, (void *)0, 0);
    if (ptr == (void *)(-1)) 
    {
        lam_ssi_ckptrd_log_message("Error creating a ptr to shared mem for ips", FATAL);
        return;
    }

    char **node_ips = (char **) ptr;
    char *start = (char *) ptr;
    start += 100 * sizeof(char *);

    int i;
    for(i=0;i<100;i++)
    {
        node_ips[i] = start;
        start += 20 * sizeof(char);
    }

    /* detatch finally as the parent doesnt need this data */
    shmdt(ptr);

    return;
}

void
lam_ssi_ckptrd_update_node_ips(const lam_cr_dbootmesg *dbootmesg)
{
    int shmid;    
    if ((shmid = shmget(IP_KEY, MAX_IP_SHM, 0664)) == -1) 
    {
        lam_ssi_ckptrd_log_message("Error getting a shared mem segment for ips", FATAL);
        return;
    }

    char **node_ips = (char **) shmat(shmid, (void *)0, 0);
    if (node_ips == (char **)(-1)) 
    {
        lam_ssi_ckptrd_log_message("Error getting a ptr to shared mem for ips", FATAL);
        return;
    }
    
    int i;
    for(i=0;i<dbootmesg->node_count;i++)
    {
        strcpy(node_ips[i], dbootmesg->node_ips[i]);
    }

    return;
}

char **
lam_ssi_ckptrd_get_node_ips()
{
    int shmid;    
    if ((shmid = shmget(IP_KEY, MAX_IP_SHM, 0664)) == -1) 
    {
        lam_ssi_ckptrd_log_message("Error reading a shared mem segment for ips", FATAL);
        return;
    }

    char **node_ips = (char **) shmat(shmid, (void *)0, 0);
    if (node_ips == (char **)(-1)) 
    {
        lam_ssi_ckptrd_log_message("Error reading a ptr to shared mem for ips", FATAL);
        return;
    }

    return node_ips; 
}

int
lam_ssi_ckptrd_get_procsstatus(pid_t mpirun_pid, pid_t current_child)
{
    int shmid;    
    if ((shmid = shmget(PROC_KEY, MAX_PROC_SHM, 0664)) == -1) 
    {
        char temp[25];
        switch(errno)
        {
            case EACCES:
                strcpy(temp, "EACCES");
                break;
            case EEXIST:
                strcpy(temp, "EEXIST");
                break;
            case EINVAL:
                strcpy(temp, "EINVAL");
                break;
            case ENFILE:
                strcpy(temp, "ENFILE");
                break;
            case ENOENT:
                strcpy(temp, "ENOENT");
                break;
            case ENOMEM:
                strcpy(temp, "ENOMEM");
                break;
            case ENOSPC:
                strcpy(temp, "ENOSPC");
                break;
            case EPERM:
                strcpy(temp, "EPERM");
                break;
        }

        sprintf(log_buf, "Error reading a shared mem segment for procs (%d - %s)", errno, temp);
        lam_ssi_ckptrd_log_message(log_buf, FATAL);
        return -1;
    }

    int *procs = (int *) shmat(shmid, (void *)0, 0);
    if (procs == (int *)(-1)) 
    {
        lam_ssi_ckptrd_log_message("Error reading a ptr to shared mem for procs", FATAL);
        return -1;
    }

    int status = procs[mpirun_pid];
    procs[mpirun_pid] = current_child;

    shmdt((void *) procs);
    return status; 
}

lam_cr_procinfo *
lam_ssi_ckptrd_get_procinfo(pid_t mpirun_pid)
{
    int shmid;    
    if ((shmid = shmget(mpirun_pid, sizeof(lam_cr_procinfo), 0664 | IPC_CREAT)) == -1) 
    {
        char temp[25];
        switch(errno)
        {
            case EACCES:
                strcpy(temp, "EACCES");
                break;
            case EEXIST:
                strcpy(temp, "EEXIST");
                break;
            case EINVAL:
                strcpy(temp, "EINVAL");
                break;
            case ENFILE:
                strcpy(temp, "ENFILE");
                break;
            case ENOENT:
                strcpy(temp, "ENOENT");
                break;
            case ENOMEM:
                strcpy(temp, "ENOMEM");
                break;
            case ENOSPC:
                strcpy(temp, "ENOSPC");
                break;
            case EPERM:
                strcpy(temp, "EPERM");
                break;
        }

        sprintf(log_buf, "Error reading a shared mem segment for procinfo[%d] (%d - %s)", mpirun_pid, errno, temp);
        lam_ssi_ckptrd_log_message(log_buf, FATAL);
        return NULL;
    }

    lam_cr_procinfo *procinfo = (lam_cr_procinfo *) shmat(shmid, (void *)0, 0);
    if (procinfo == (lam_cr_procinfo *)(-1)) 
    {
        sprintf(log_buf, "Error reading a ptr to shared mem for procinfo[%d]", mpirun_pid);
        lam_ssi_ckptrd_log_message(log_buf, FATAL);
        return NULL;
    }

    return procinfo; 
}
