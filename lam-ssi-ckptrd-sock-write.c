#include "lam-ssi-ckptrd-includes.h"

#define MAX_CHARARRAY_SIZE 150

/* Local functions */
bool write_sock_dctrlmesg(int , const lam_cr_dctrlmesg *);
bool write_sock_dinfomesg(int , const lam_cr_dinfomesg *);
bool write_sock_dbootmesg(int , const lam_cr_dbootmesg *);
bool write_sock_debug_info(int , const struct debug_info *);

bool write_sock_int(int, const int *);
bool write_sock_long(int, const long *);
bool write_sock_charArray(int, const char *);
bool write_sock_struct_gpsArray(int, const struct _gps *);

bool
lam_ssi_ckptrd_write_dsockdmesg(int connfd, const lam_cr_dsockmesg *dsockmesg)
{
    bool success = true;

    success &= write_sock_int(connfd, &dsockmesg->dmesg_type);

    if(success)
    {
        if(dsockmesg->dmesg_type == DINFO)
            success &= write_sock_dinfomesg(connfd, &dsockmesg->dmesg.dinfomesg);
        else if(dsockmesg->dmesg_type == DCTRL)
            success &= write_sock_dctrlmesg(connfd, &dsockmesg->dmesg.dctrlmesg);
        else if(dsockmesg->dmesg_type == DBOOT)
            success &= write_sock_dbootmesg(connfd, &dsockmesg->dmesg.dbootmesg);
        else
        {
            printf("Unknows code - %d\n",dsockmesg->dmesg_type);
            success = false;
        }
    }

    return success;
}

bool
write_sock_dinfomesg(int connfd, const lam_cr_dinfomesg *dinfomesg)
{
    bool success = true;

    success &= write_sock_int(connfd, &dinfomesg->info_type);
    success &= write_sock_int(connfd, &dinfomesg->mpirun_pid);
    success &= write_sock_charArray(connfd, dinfomesg->name);
    success &= write_sock_int(connfd, &dinfomesg->mpiworld_n);

    if(success && (dinfomesg->mpiworld_n > 0))
    {
        int i;
        for(i = 0 ; i < dinfomesg->mpiworld_n; i++)
            success |= write_sock_struct_gpsArray(connfd, &dinfomesg->mpi_gps[i]);
    }

    success &= write_sock_debug_info(connfd, &dinfomesg->mpi_debug_info);

    return success;
}

bool
write_sock_dctrlmesg(int connfd, const lam_cr_dctrlmesg *dctrlmesg)
{
    bool success = true;

    success &= write_sock_int(connfd, &dctrlmesg->mesg_type);
    success &= write_sock_int(connfd, &dctrlmesg->ctrl_type);
    success &= write_sock_int(connfd, &dctrlmesg->mpi_pid);
    success &= write_sock_charArray(connfd, dctrlmesg->filename);
    success &= write_sock_long(connfd, &dctrlmesg->size);

    return success;
}

bool
write_sock_dbootmesg(int connfd, const lam_cr_dbootmesg *dbootmesg)
{
    bool success = true;

    success &= write_sock_int(connfd, &dbootmesg->node_count);
    int i;
    for(i=0;i<dbootmesg->node_count;i++)
    {
        success &= write_sock_charArray(connfd, dbootmesg->node_ips[i]);
    }

    return success;
}


bool
write_sock_debug_info(int connfd, const struct debug_info *var)
{
    ssize_t sent_bytes;
   
    sent_bytes = write(connfd, var, sizeof(struct debug_info));
    if(sent_bytes != sizeof(struct debug_info))
        return false;

    return true;
}

bool
write_sock_int(int connfd, const int *var)
{
    ssize_t sent_bytes;
   
    sent_bytes = write(connfd, var, sizeof(int));
    if(sent_bytes != sizeof(int))
        return false;

    return true;
}

bool
write_sock_long(int connfd, const long *var)
{
    ssize_t sent_bytes;
   
    sent_bytes = write(connfd, var, sizeof(long));
    if(sent_bytes != sizeof(long))
        return false;

    return true;
}

bool
write_sock_charArray(int connfd, const char *var)
{
    char *temp_var;
    if(var == NULL)
        temp_var = strdup("");
    else
        temp_var = strdup(var);

    ssize_t sent_bytes;
   
    sent_bytes = write(connfd, temp_var, strlen(temp_var) + 1);
    if(sent_bytes != (strlen(temp_var) + 1))
    {
        free(temp_var);
        return false;
    }

    free(temp_var);
    return true;
}

bool
write_sock_struct_gpsArray(int connfd, const struct _gps *var)
{
    ssize_t sent_bytes;
   
    sent_bytes = write(connfd, var, sizeof(struct _gps));
    if(sent_bytes != sizeof(struct _gps))
        return false;

    return true;
}

