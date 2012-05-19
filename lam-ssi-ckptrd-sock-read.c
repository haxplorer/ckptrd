#include "lam-ssi-ckptrd-includes.h"

#define MAX_CHARARRAY_SIZE 150

/* Local functions */
bool read_sock_dctrlmesg(int ,lam_cr_dctrlmesg *);
bool read_sock_dinfomesg(int ,lam_cr_dinfomesg *);
bool read_sock_dbootmesg(int ,lam_cr_dbootmesg *);
bool read_sock_debug_info(int ,struct debug_info *);

bool read_sock_int(int, int *);
bool read_sock_long(int, long *);
char* read_sock_charArray(int, bool *);
bool read_sock_struct_gpsArray(int, struct _gps *);

bool
lam_ssi_ckptrd_read_dsockdmesg(int connfd, lam_cr_dsockmesg *dsockmesg)
{
    bool success = true;

    success &= read_sock_int(connfd, &dsockmesg->dmesg_type);

    if(success)
    {
        if(dsockmesg->dmesg_type == DINFO)
            success &= read_sock_dinfomesg(connfd, &dsockmesg->dmesg.dinfomesg);
        else if(dsockmesg->dmesg_type == DCTRL)
            success &= read_sock_dctrlmesg(connfd, &dsockmesg->dmesg.dctrlmesg);
        else if(dsockmesg->dmesg_type == DBOOT)
            success &= read_sock_dbootmesg(connfd, &dsockmesg->dmesg.dbootmesg);
        else
        {
            char log_buf[100];
            sprintf(log_buf, "Unknown dmesg_type = %d", dsockmesg->dmesg_type);
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
            success = false;
        }
    }

    return success;
}

bool
read_sock_dinfomesg(int connfd, lam_cr_dinfomesg *dinfomesg)
{
    bool success = true;

    success &= read_sock_int(connfd, &dinfomesg->info_type);
    success &= read_sock_int(connfd, &dinfomesg->mpirun_pid);

    dinfomesg->name = read_sock_charArray(connfd, &success);
    dinfomesg->name = (char *) realloc(dinfomesg->name, strlen(dinfomesg->name) + 1);

    success &= read_sock_int(connfd, &dinfomesg->mpiworld_n);

    if(success && (dinfomesg->mpiworld_n > 0))
    {
        dinfomesg->mpi_gps = (struct _gps *) malloc(dinfomesg->mpiworld_n * sizeof(struct _gps));
        int i;
        for(i = 0 ; i < dinfomesg->mpiworld_n; i++)
            success |= read_sock_struct_gpsArray(connfd, &dinfomesg->mpi_gps[i]);
    }
    else
    {
        dinfomesg->mpi_gps = NULL;
    }

    success &= read_sock_debug_info(connfd, &dinfomesg->mpi_debug_info);

    return success;
}

bool
read_sock_dctrlmesg(int connfd, lam_cr_dctrlmesg *dctrlmesg)
{
    bool success = true;

    success &= read_sock_int(connfd, &dctrlmesg->mesg_type);
    success &= read_sock_int(connfd, &dctrlmesg->ctrl_type);
    success &= read_sock_int(connfd, &dctrlmesg->mpi_pid);
    dctrlmesg->filename = read_sock_charArray(connfd, &success);
    success &= read_sock_long(connfd, &dctrlmesg->size);

    return success;
}

bool
read_sock_dbootmesg(int connfd, lam_cr_dbootmesg *dbootmesg)
{
    bool success = true;

    success &= read_sock_int(connfd, &dbootmesg->node_count);
    dbootmesg->node_ips = (char **) malloc(dbootmesg->node_count * sizeof(char *));
    int i;
    for(i=0;i<dbootmesg->node_count;i++)
        dbootmesg->node_ips[i] = read_sock_charArray(connfd, &success);

    return success;
}


bool
read_sock_debug_info(int connfd, struct debug_info *var)
{
    void *buffer = malloc(sizeof(struct debug_info));
    ssize_t received_bytes;
   
    received_bytes = read(connfd, buffer, sizeof(struct debug_info));
    if(received_bytes != sizeof(struct debug_info))
        return false;

    memcpy(var, buffer, sizeof(struct debug_info));

    return true;
}


bool
read_sock_int(int connfd, int *var)
{
    void *buffer = malloc(sizeof(int));
    ssize_t received_bytes;
   
    received_bytes = read(connfd, buffer, sizeof(int));
    if(received_bytes != sizeof(int))
        return false;

    memcpy(var, buffer, sizeof(int));

    return true;
}

bool
read_sock_long(int connfd, long *var)
{
    void *buffer = malloc(sizeof(long));
    ssize_t received_bytes;
   
    received_bytes = read(connfd, buffer, sizeof(long));
    if(received_bytes != sizeof(long))
        return false;

    memcpy(var, buffer, sizeof(long));

    return true;
}


char*
read_sock_charArray(int connfd, bool *success)
{
    char buffer[MAX_CHARARRAY_SIZE];
    int buffer_size = sizeof(buffer);
    ssize_t received_bytes = 0, temp_receive_bytes;
    int i;

    char *buffer_ptr = buffer;

    for(i=0;i<buffer_size;i++)
    {
        temp_receive_bytes = read(connfd, buffer_ptr, 1);

        if(temp_receive_bytes != 1)
        {
            *success &= false;
            return NULL;
        }

        buffer_ptr++;        
        received_bytes += temp_receive_bytes;
        if(buffer[i] == '\0')
            break;
    }

    if(i != buffer_size)
        i = buffer_size - 1;
    if(buffer[i] != '\0')
        buffer[i] = '\0';

    *success &= true;
    return strdup(buffer);
}

bool
read_sock_struct_gpsArray(int connfd, struct _gps *var)
{
    void *buffer = malloc(sizeof(struct _gps));
    ssize_t received_bytes;
   
    received_bytes = read(connfd, buffer, sizeof(struct _gps));
    if(received_bytes != sizeof(struct _gps))
        return false;

    memcpy(var, buffer, sizeof(struct _gps));

    return true;
}

