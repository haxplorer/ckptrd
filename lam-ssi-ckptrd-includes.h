#include    <stdbool.h>
#include    <stdlib.h>
#include    <stdio.h>
#include    <sys/types.h>
#include    <sys/socket.h>
#include    <sys/time.h>
#include    <time.h>
#include    <netinet/in.h>
#include    <arpa/inet.h>
#include    <errno.h>
#include    <fcntl.h>
#include    <netdb.h>
#include    <signal.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <sys/stat.h> 
#include    <sys/uio.h> 
#include    <unistd.h>
#include    <sys/wait.h>
#include    <sys/un.h> 
#include    <sys/ipc.h>
#include    <sys/shm.h> 

#ifndef _LAM_SSI_CKPTRD_INCLUDES_H_
#define _LAM_SSI_CKPTRD_INCLUDES_H_

#define CKPTRD_PORT        9001
#define SA  struct sockaddr
#define MAXLINE     4096    
#define LISTENQ     1024    
#define LOG_FILE "/tmp/ckptrd.log"
#define LOCK_FILE "/tmp/ckptrd.lock"

/* log classes */
#define FINE        0
#define INFO        1
#define WARNING     2
#define FATAL       3

/* end of transmition through socket */
#define ENDOFSTRUCT 31

/* additional signals */
#define SIGPING     0
#define SIGNOTIFY   (SIGRTMIN+1)


/* Sock Message dmesg_type's */
#define DINFO       1
#define DCTRL       2
#define DBOOT       3

/* MPI info_type's */
#define MPISTART    1
#define MPISTOP     2
#define MPIERROR    3

/* Daemon ctrl_type's */
#define ISALIVE     1
#define KILLPID     2
#define GETSIZE     3
#define CSUCC       4
#define CFAIL       5

/* Ctrl mesg_type's */
#define CTRLREQ     1
#define CTRLRESP    2

/* dmesg_type's */
#define DINFOMESG   1
#define DCTRLMESG   2
#define DBOOTMESG   3

#define MAX_BUFFER  100

/* shared mem */
#define MAX_IP_SHM  ((sizeof(char *) * 200) + (sizeof(char) * 20))

#define IP_KEY      1000000
#define PROC_KEY    1000001

#define MAX_PID      32768
#define MAX_PROC_SHM (MAX_PID * sizeof(int))

/* proc_info */
#define PROCRUN     1
#define PROCFINISH  2    
#define PROCFAIL    3    


/*
 * Global Positioning System for running processes (LAM's struct)
 */

#ifndef _APP_MGMT_H
struct _gps 
{
    int            gps_node;               /* node ID */
    int            gps_pid;                /* process ID */
    int            gps_idx;                /* process index */
    int            gps_grank;              /* glob. rank in loc. world */
};
#endif

struct debug_info
{
    int nodeid;
    pid_t mpi_pid;
    int killed_by_signal;
    int status;
};

typedef struct
{
    int info_type;
    pid_t mpirun_pid;
    char *name;
    int mpiworld_n;
    struct _gps *mpi_gps;
    struct debug_info mpi_debug_info;
} lam_cr_dinfomesg;

typedef struct
{
    int mesg_type;
    int ctrl_type;
    pid_t mpi_pid;
    long size;
    char *filename;
} lam_cr_dctrlmesg;

typedef struct
{
    int node_count;
    char** node_ips;
} lam_cr_dbootmesg;

typedef union
{
    lam_cr_dinfomesg dinfomesg;
    lam_cr_dctrlmesg dctrlmesg;
    lam_cr_dbootmesg dbootmesg;
} lam_cr_dmesg;

typedef struct
{
    int dmesg_type;
    lam_cr_dmesg dmesg;
} lam_cr_dsockmesg;

typedef struct
{
    pid_t mpirun_pid;
    int mpiworld_n;
    int checkpointed, restarted;
    int proc_status;
    time_t start_time;
    time_t stop_time;
    long size;
    char name[100];
} lam_cr_procinfo;


/* lam_ssi_cptrd internal API fns */

bool lam_ssi_ckptrd_write_dsockdmesg(int, const lam_cr_dsockmesg *);
bool lam_ssi_ckptrd_read_dsockdmesg(int, lam_cr_dsockmesg *);
void lam_ssi_ckptrd_log_message(const char*, int);
void lam_ssi_ckptrd_fork_child(int);

void lam_ssi_ckptrd_make_ip_mem();
void lam_ssi_ckptrd_make_procs_mem();
int * lam_ssi_ckptrd_get_procs();
void lam_ssi_ckptrd_update_node_ips(const lam_cr_dbootmesg*);
char ** lam_ssi_ckptrd_get_node_ips();
int lam_ssi_ckptrd_get_procsstatus(pid_t, pid_t);
lam_cr_procinfo * lam_ssi_ckptrd_get_procinfo(pid_t); 


#endif
