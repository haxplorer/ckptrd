#include "lam-ssi-ckptrd-includes.h"

#define MAX_CHARARRAY_SIZE 150
#define RAWSTRUCT          0

/* TODO: Use as base and chage adaptively */
#define CKPT_INTERVAL      10

/* TODO: hardcoded IPs to be dynamically loaded from lamboot */
char **node_ips = NULL;

/* Local functions */
void log_dsockmesg(lam_cr_dsockmesg *);
void service_dbootmesg(int, lam_cr_dbootmesg *);
void service_dinfomesg(int, lam_cr_dinfomesg *);
void service_dctrlmesg(int, lam_cr_dctrlmesg *);
void initialize_procinfo(lam_cr_dinfomesg *);
void ckpt_mpi(int, lam_cr_dinfomesg *);
void rst_mpi(lam_cr_dinfomesg *);
bool checkifalive(pid_t pid);
bool propagate_mpiworld(int, struct _gps *);
bool clean_mpi(int, pid_t);
int send_dsockmesg(const lam_cr_dsockmesg *, char *);
bool get_ckpt_size(pid_t, int, pid_t, long *);

#if(RAWSTRUCT)
static ssize_t read_sock_char(int, char *);
ssize_t read_sock_struct(int, void *);
#endif

/* Signal handlers */
void signal_handler(int);
void periodic_signal_handler(int);
void sig_action(int signo, siginfo_t *siginfo, void *);

/* local variable */
lam_cr_dsockmesg dsockmesg;
lam_cr_dinfomesg *dinfomesg_t;
lam_cr_procinfo *procinfo = NULL;
int ckpt_interval = CKPT_INTERVAL;

void
signal_handler(int signo)
{
    signal(SIGALRM, SIG_IGN);
    char log_buf[100];
    sprintf(log_buf, "Encountered %d signal in child", signo);
    lam_ssi_ckptrd_log_message(log_buf, FINE);

    if(procinfo != NULL)
    {
        procinfo->proc_status = PROCFAIL;
        time(&procinfo->stop_time);
    }
    exit(0);
}

void
sig_action(int signo, siginfo_t *siginfo, void *ptr)
{    
    char log_buf[100];
    sprintf(log_buf, "Encountered %d (SIGNOTIFY) in child", signo);
    lam_ssi_ckptrd_log_message(log_buf, FINE);

    procinfo->proc_status = PROCFAIL;
    time(&procinfo->stop_time);

    exit(0);
}    

void
lam_ssi_ckptrd_fork_child(int connfd)
{
    char log_buf[300];
    bool sock_return;

    int i, j;
    for(i=0;i<32;i++)
        signal(i, signal_handler);

    struct sigaction new_handler;
    new_handler.sa_sigaction = sig_action;
    new_handler.sa_flags = SA_SIGINFO;
    sigemptyset(&new_handler.sa_mask);
    sigaddset(&new_handler.sa_mask, SIGALRM);
    sigaction(SIGNOTIFY, &new_handler, NULL);

    sock_return = lam_ssi_ckptrd_read_dsockdmesg(connfd, &dsockmesg);

    if(!sock_return)
    {
        sprintf(log_buf, "Socket struct reading error from %d", getpid());
        lam_ssi_ckptrd_log_message(log_buf, INFO);
        return;
    }
    else
    {
        log_dsockmesg(&dsockmesg);
    }

    int dmesg_type = dsockmesg.dmesg_type;    

    switch(dmesg_type)
    {
        case DINFO:
            service_dinfomesg(connfd, &dsockmesg.dmesg.dinfomesg);
            break;
        case DCTRL:
            service_dctrlmesg(connfd, &dsockmesg.dmesg.dctrlmesg);
            break;
        case DBOOT:
            service_dbootmesg(connfd, &dsockmesg.dmesg.dbootmesg);
            break;
        default:
            /* un recogonised */
            break;
    }
    /** Do stuff **/
}

void
service_dbootmesg(int connfd, lam_cr_dbootmesg *dbootmesg)
{
    int i, *procs = lam_ssi_ckptrd_get_procs();
    for(i=0;i<MAX_PID;i++)
    {
        if(procs[i] > 0)
            kill(procs[i], SIGNOTIFY);
    }
    /* update the shared memory with the newly arrived set of boothosts */
    lam_ssi_ckptrd_update_node_ips(dbootmesg);
}

void
service_dinfomesg(int connfd, lam_cr_dinfomesg *dinfomesg)
{
    /* attach the shared memory having ip to the process to get ip of nodes */
    node_ips = lam_ssi_ckptrd_get_node_ips();

    char log_buf[150];
    switch(dinfomesg->info_type)
    {
        case MPISTART:
            initialize_procinfo(dinfomesg);
            ckpt_mpi(connfd, dinfomesg);
            break;

        case MPISTOP:
            sprintf(log_buf, "Process %s(%d) sent MPISTOP before MPISTART.", dinfomesg->name, dinfomesg->mpirun_pid);
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
            break;

        case MPIERROR:
            sprintf(log_buf, "Process %s(%d) sent MPIERROR before MPISTART.", dinfomesg->name, dinfomesg->mpirun_pid);
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
            break;

        default:
            /* cannot get here */
            break;
    }
    return;
}


void
initialize_procinfo(lam_cr_dinfomesg *dinfomesg)
{
    procinfo = lam_ssi_ckptrd_get_procinfo(dinfomesg->mpirun_pid);
    int status = lam_ssi_ckptrd_get_procsstatus(dinfomesg->mpirun_pid, getpid());

    if(status == 0)
    {
        procinfo->mpirun_pid = dinfomesg->mpirun_pid;
        procinfo->mpiworld_n = dinfomesg->mpiworld_n;
        procinfo->restarted = procinfo->checkpointed = 0;
        procinfo->proc_status = PROCRUN;
        time(&procinfo->start_time);
        time(&procinfo->stop_time);
        procinfo->size = 0L;
        strncpy(procinfo->name, dinfomesg->name, 99);
    }
    else if(status > 0)
    {
        (procinfo->restarted)++;
        procinfo->proc_status = PROCRUN;
        time(&procinfo->stop_time);
    }

}

void
service_dctrlmesg(int connfd, lam_cr_dctrlmesg *dctrlmesg)
{
    char log_buf[150];
    if(dctrlmesg->mesg_type == CTRLRESP)
    {
        /* drop those messages */
        return;
    }

    FILE *fp;
    char filename[150];
    long size_of_file = 0L;
    bool success;
    switch(dctrlmesg->ctrl_type)
    {
        case GETSIZE:
            if(dctrlmesg->filename[0] == '~')
                sprintf(filename, "%s/%s", getenv("HOME"), (dctrlmesg->filename + 1));
            else
                strcpy(filename, dctrlmesg->filename);
            fp = fopen(filename, "r");
            if(fp != NULL)
            {
                fseek(fp, 0, SEEK_END);
                size_of_file = ftell(fp);
                fclose(fp);
                success = true;
            }
            else
            {
                success = false;
            }
            break;

        case ISALIVE:
            success = checkifalive(dctrlmesg->mpi_pid);
            break;

        case KILLPID:
            success = !(checkifalive(dctrlmesg->mpi_pid));
            if(!success)
            {
                kill(dctrlmesg->mpi_pid, SIGKILL);
                sleep(1);
                success = !(checkifalive(dctrlmesg->mpi_pid));
            }
            break;
    }

    lam_cr_dsockmesg resp_dsockmesg;
    resp_dsockmesg.dmesg_type = DCTRL;

    lam_cr_dctrlmesg *temp_ctrl = &resp_dsockmesg.dmesg.dctrlmesg;
    temp_ctrl->mesg_type = CTRLRESP;
    temp_ctrl->ctrl_type = success ? CSUCC : CFAIL;
    temp_ctrl->mpi_pid = dctrlmesg->mpi_pid;
    temp_ctrl->filename = strdup(dctrlmesg->filename);
    temp_ctrl->size = size_of_file;

    log_dsockmesg(&resp_dsockmesg);
    signal(SIGPIPE, SIG_IGN);
    lam_ssi_ckptrd_write_dsockdmesg(connfd, &resp_dsockmesg);
    signal(SIGPIPE, SIG_DFL);

    return;
}

void
ckpt_mpi(int connfd, lam_cr_dinfomesg *dinfomesg)
{
    /* setting dinfomesg to a global variable so that the singal handler can access it */
    dinfomesg_t = dinfomesg;
    signal(SIGALRM, periodic_signal_handler);
    alarm(ckpt_interval);

    char log_buf[200];

    lam_cr_dsockmesg term_dsockmesg;
    bool success = lam_ssi_ckptrd_read_dsockdmesg(connfd, &term_dsockmesg);
    if(success)
    {
        log_dsockmesg(&term_dsockmesg);
        /* Stop further SIGALARM  and process the message */
        ckpt_interval = 0;
        alarm(0);

        if(term_dsockmesg.dmesg_type != DINFO)
        {
            sprintf(log_buf, "Illegal value - %d in second DINFO mesg", term_dsockmesg.dmesg_type);
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
        }
        else if(term_dsockmesg.dmesg.dinfomesg.info_type == MPISTOP)
        {
            sprintf(log_buf, "Process %s(%d) finished successfully.", term_dsockmesg.dmesg.dinfomesg.name, 
                    term_dsockmesg.dmesg.dinfomesg.mpirun_pid);
            lam_ssi_ckptrd_log_message(log_buf, INFO);            

            procinfo->proc_status = PROCFINISH;
            time(&procinfo->stop_time);
        }
        else if(term_dsockmesg.dmesg.dinfomesg.info_type == MPIERROR)
        {
            sprintf(log_buf, "Restarting mpirun %s" , term_dsockmesg.dmesg.dinfomesg.name);
            lam_ssi_ckptrd_log_message(log_buf, INFO);

            procinfo->proc_status = PROCFAIL;
            time(&procinfo->stop_time);

            rst_mpi(dinfomesg);
        }
    }
    else
    {
        ckpt_interval = 0;
        alarm(0);

        sprintf(log_buf, "mpirun (%d) was found dead %s." ,
                dinfomesg->mpirun_pid,  dinfomesg->name);
        lam_ssi_ckptrd_log_message(log_buf, INFO);
            
        procinfo->proc_status = PROCFAIL;
        time(&procinfo->stop_time);

        if(!checkifalive(dinfomesg->mpirun_pid))
        {
            sprintf(log_buf, "mpirun (%d) was found dead %s. Trying restart." ,
                    dinfomesg->mpirun_pid,  dinfomesg->name);
            lam_ssi_ckptrd_log_message(log_buf, INFO);
            rst_mpi(dinfomesg);
        }
    }

    lam_ssi_ckptrd_log_message("*** Quiting", FINE);
    exit(0);            
}

void
rst_mpi(lam_cr_dinfomesg *dinfomesg)
{
    propagate_mpiworld(dinfomesg->mpiworld_n, dinfomesg->mpi_gps);

    sleep(3);
    char log_buf[200];
    sprintf(log_buf, "lamrestart -ssi cr blcr -ssi cr_blcr_context_file $HOME/context.mpirun.%d &",
            dinfomesg->mpirun_pid);
    //lam_ssi_ckptrd_log_message(log_buf, INFO);
    signal(SIGCHLD, SIG_IGN);
    system(log_buf);
    signal(SIGCHLD, SIG_IGN);
}

void
periodic_signal_handler(int signo)
{
    signal(SIGALRM, SIG_IGN);
    char log_buf[150];
    sprintf(log_buf, "Checkpointing mpirun %s ", dsockmesg.dmesg.dinfomesg.name);
    lam_ssi_ckptrd_log_message(log_buf, FINE);

    if(checkifalive(dsockmesg.dmesg.dinfomesg.mpirun_pid))
    {
        sprintf(log_buf, "lamcheckpoint -ssi cr blcr -pid %d", dsockmesg.dmesg.dinfomesg.mpirun_pid);
        signal(SIGCHLD, SIG_IGN);
        system(log_buf);
        signal(SIGCHLD, signal_handler);
        if(procinfo->checkpointed % 10 == 0)
        {
            long size;
            if(get_ckpt_size(dinfomesg_t->mpirun_pid, dinfomesg_t->mpi_gps[0].gps_node,
                        dinfomesg_t->mpi_gps[0].gps_pid, &size))
            {
                procinfo->size += (size - procinfo->size) / (procinfo->checkpointed / 10 + 1);
            }
        }
        (procinfo->checkpointed)++;
    }
    else
    {
        sprintf(log_buf, "pid %d seems to be dead", dsockmesg.dmesg.dinfomesg.mpirun_pid);
        lam_ssi_ckptrd_log_message(log_buf, WARNING);
    }

    alarm(ckpt_interval);    
    signal(SIGALRM, periodic_signal_handler);
}

bool
checkifalive(pid_t pid)
{
    char log_buf[100];
    int success;

    success = kill(pid, SIGPING);
    if(success == 0)
        return true;

    switch(errno)
    {
        case EINVAL:
            sprintf(log_buf, "Invalid signal - %d", SIGPING);
            break;
        case EPERM:
            sprintf(log_buf, "No suffieicent privileges to ping %d", pid);
            break;
        case ESRCH:
            return false;
    }

    lam_ssi_ckptrd_log_message(log_buf, WARNING);         
    return false;
}

bool
propagate_mpiworld(int mpiworld_n, struct _gps *mpi_gps)
{
    char log_buf[150];

    sprintf(log_buf, "Propagating across %d nodes", mpiworld_n);
    lam_ssi_ckptrd_log_message(log_buf, INFO);

    bool success;
    int i;
    for(i=0;i<mpiworld_n;i++)
    {
        success = clean_mpi(mpi_gps[i].gps_node, mpi_gps[i].gps_pid);
        if(success)
        {
            sprintf(log_buf, "node%d successfully cleaned", i);
        }
        else
        {
            sprintf(log_buf, "node%d failed", i);
        }        
        lam_ssi_ckptrd_log_message(log_buf, INFO);
    }

}

bool
clean_mpi(int nodeid, pid_t mpi_pid)
{
    char log_buf[150];
    bool success = false;
    lam_cr_dsockmesg req_dsockmesg;
    req_dsockmesg.dmesg_type = DCTRL;

    lam_cr_dctrlmesg *temp_ctrl = &req_dsockmesg.dmesg.dctrlmesg;
    temp_ctrl->mesg_type = CTRLREQ;
    temp_ctrl->ctrl_type = KILLPID;
    temp_ctrl->mpi_pid = mpi_pid;
    temp_ctrl->filename = strdup("");

    int sockfd = send_dsockmesg(&req_dsockmesg, node_ips[nodeid]);
    if(sockfd > 0)
    {
        lam_cr_dsockmesg resp_dsockmesg;
        success = lam_ssi_ckptrd_read_dsockdmesg(sockfd, &resp_dsockmesg);

        log_dsockmesg(&resp_dsockmesg);

        if((resp_dsockmesg.dmesg_type != DCTRL) || (resp_dsockmesg.dmesg.dctrlmesg.mesg_type != CTRLRESP))
        {
            sprintf(log_buf, "node%d sending demsg_type %d / mesg_type %d in reply to DCTRL",
                    nodeid, resp_dsockmesg.dmesg_type, resp_dsockmesg.dmesg.dctrlmesg.mesg_type);
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
        }
        else if(resp_dsockmesg.dmesg.dctrlmesg.ctrl_type == CSUCC)
        {
            success = true;            
        }
        else
        {
            success = false;
        }
        close(sockfd);
    }
    return success;
}
    
bool
get_ckpt_size(pid_t mpirun_pid, int nodeid, pid_t mpi_pid, long *size)
{
    char log_buf[150], filename[50];
    sprintf(filename, "~/context.%d-n%d-%d", mpirun_pid, nodeid, mpi_pid);
    lam_ssi_ckptrd_log_message(filename, INFO);
    
    bool success = false;
    lam_cr_dsockmesg req_dsockmesg;
    req_dsockmesg.dmesg_type = DCTRL;

    lam_cr_dctrlmesg *temp_ctrl = &req_dsockmesg.dmesg.dctrlmesg;
    temp_ctrl->mesg_type = CTRLREQ;
    temp_ctrl->ctrl_type = GETSIZE;
    temp_ctrl->mpi_pid = mpi_pid;
    temp_ctrl->filename = strdup(filename);
    temp_ctrl->size = 0L;

    int sockfd = send_dsockmesg(&req_dsockmesg, node_ips[nodeid]);
    if(sockfd > 0)
    {
        lam_cr_dsockmesg resp_dsockmesg;
        success = lam_ssi_ckptrd_read_dsockdmesg(sockfd, &resp_dsockmesg);

        log_dsockmesg(&resp_dsockmesg);

        if((resp_dsockmesg.dmesg_type != DCTRL) || (resp_dsockmesg.dmesg.dctrlmesg.mesg_type != CTRLRESP))
        {
            sprintf(log_buf, "node%d sending demsg_type %d / mesg_type %d in reply to DCTRL",
                    nodeid, resp_dsockmesg.dmesg_type, resp_dsockmesg.dmesg.dctrlmesg.mesg_type);
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
        }
        else if(resp_dsockmesg.dmesg.dctrlmesg.ctrl_type == CSUCC)
        {
            *size = resp_dsockmesg.dmesg.dctrlmesg.size;
            success = true; 
        }
        else
        {
            success = false;
        }
        close(sockfd);
    }
    return success;
}

int
send_dsockmesg(const lam_cr_dsockmesg *dsockmesg, char *ip)
{
    struct sockaddr_in servaddr;
    int sockfd_ckptrd = socket (AF_INET, SOCK_STREAM, 0);
    memset (&servaddr, 0, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons (CKPTRD_PORT);
    inet_pton (AF_INET, ip, &servaddr.sin_addr);
    connect (sockfd_ckptrd, (struct sockaddr *) & servaddr, sizeof (servaddr));

    signal(SIGPIPE, SIG_IGN);
    bool success = lam_ssi_ckptrd_write_dsockdmesg(sockfd_ckptrd, dsockmesg);
    signal(SIGPIPE, SIG_DFL);
    if(success)
        return sockfd_ckptrd;
    else
        return -1 * sockfd_ckptrd;
}

void
log_dsockmesg(lam_cr_dsockmesg *dsockmesg)
{
    char log_buf[200];
    if(dsockmesg->dmesg_type == DINFO)
    {
        lam_cr_dinfomesg *temp_info = &dsockmesg->dmesg.dinfomesg;

        sprintf(log_buf, " INFO: %d  %d  %s  %d", temp_info->info_type, temp_info->mpirun_pid,
                temp_info->name, temp_info->mpiworld_n);
        lam_ssi_ckptrd_log_message(log_buf, FINE);

        int j;
        for(j=0;j<temp_info->mpiworld_n;j++)
        {
            sprintf(log_buf, " -------- (%d)  %d  %d  %d  %d", j, temp_info->mpi_gps[j].gps_node,
                    temp_info->mpi_gps[j].gps_pid, temp_info->mpi_gps[j].gps_idx,
                    temp_info->mpi_gps[j].gps_grank);
            lam_ssi_ckptrd_log_message(log_buf, FINE);
        }

        struct debug_info *temp_debug = &dsockmesg->dmesg.dinfomesg.mpi_debug_info;
        sprintf(log_buf, " ========    %d %d %d %d", temp_debug->nodeid, temp_debug->mpi_pid, 
                temp_debug->killed_by_signal, temp_debug->status);
        lam_ssi_ckptrd_log_message(log_buf, FINE);
    }
    else if(dsockmesg->dmesg_type == DCTRL)
    {
        lam_cr_dctrlmesg *temp_ctrl = &dsockmesg->dmesg.dctrlmesg;

        if(temp_ctrl->ctrl_type == GETSIZE)
            sprintf(log_buf, "CTRL: %d %d %s %d", temp_ctrl->mesg_type, temp_ctrl->ctrl_type,
                    temp_ctrl->filename, temp_ctrl->size);
        else
            sprintf(log_buf, "CTRL: %d %d %d", temp_ctrl->mesg_type, temp_ctrl->ctrl_type, temp_ctrl->mpi_pid);
        lam_ssi_ckptrd_log_message(log_buf, FINE);

    }
    else if(dsockmesg->dmesg_type == DBOOT)
    {
        lam_cr_dbootmesg *temp_boot = &dsockmesg->dmesg.dbootmesg;

        sprintf(log_buf, "BOOT: %d nodes", temp_boot->node_count);
        lam_ssi_ckptrd_log_message(log_buf, FINE);

        int j;
        for(j=0;j<temp_boot->node_count;j++)
        {
            sprintf(log_buf, "----> n%d - %s", j, temp_boot->node_ips[j]);
            lam_ssi_ckptrd_log_message(log_buf, FINE);
        }
    }

    else
    {
        sprintf(log_buf, "Encountered dmesg_type = %d", dsockmesg->dmesg_type);
        lam_ssi_ckptrd_log_message(log_buf, FINE);
    }

}

# if(RAWSTRUCT)
static ssize_t
read_sock_char(int fd, char *ptr)
{
    static int	read_cnt = 0;
    static char	*read_ptr;
    static char	read_buf[MAX_BUFFER];

    if (read_cnt <= 0) 
    {
again:
        read_cnt = read(fd, read_buf, sizeof(read_buf));
        if (read_cnt < 0) 
        {
            if (errno == EINTR)
                goto again;
            return(-1);
        }
        else if (read_cnt == 0)
        {
            /* socket closed on read */
            return(0);
        }

        read_ptr = read_buf;
    }

    read_cnt--;
    *ptr = *read_ptr++;
    return(1);
}

ssize_t
read_sock_struct(int fd, void *vptr)
{
    lam_ssi_ckptrd_log_message("Reading struct", FINE);

    char log_buf[200];
    int		n, rc;
    char	c, *ptr;

    ptr = vptr;
    for (n=0;n<lam_cr_dmesg_size;n++) 
    {
        rc = read_sock_char(fd, &c);
        if(rc == 1) 
        {
            /* retrieve the struct till ENDOFSTRUCT reaches */
            if(c == ENDOFSTRUCT)
            {
                sprintf(log_buf, "ENDOFSTRUCT received before entire struct at %d" , getpid());
                lam_ssi_ckptrd_log_message(log_buf, WARNING);
                return -1;
            }

            *ptr++ = c;
        }
        else if(rc == 0) 
        {
            sprintf(log_buf, "Couldnt read data fully (received only %d bytes) at %d --- Socket closed", n, getpid());
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
            return 0;
            /*
               if(n == 1)
               return 0;	// ended with out any data being read 
               else
               break;		// ended with some data read read 
               */
        }
        else
        {
            sprintf(log_buf, "Error(0) reading at %d", getpid());
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
            return -1;
        }
    }

    if(n < lam_cr_dmesg_size)
    {
        sprintf(log_buf, "ENDOFSTRUCT received before entire struct at %d" , getpid());
        lam_ssi_ckptrd_log_message(log_buf, WARNING);
        return -1;          /* Insufficient data got */
    }
    else
    {
        rc = read_sock_char(fd, &c);
        if(rc != 1)
        {
            sprintf(log_buf, "Error(1) reading at %d with rc - %d", getpid(), rc);
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
            return rc;
        }
        if(c != ENDOFSTRUCT)
        {
            do
            {
                rc = read_sock_char(fd, &c);
                if(rc != 1)
                {
                    sprintf(log_buf, "Error(2) reading at %d with rc - %d", getpid(), rc);
                    lam_ssi_ckptrd_log_message(log_buf, WARNING);
                    return rc;
                }
            }while(rc != ENDOFSTRUCT);

            return -1;
        }
    }

    return(n);
}
#endif
