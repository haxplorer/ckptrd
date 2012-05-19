#include	"lam-ssi-ckptrd-includes.h"

void str_cli (int);

void signal_handler(int signo)
{
    printf("Caught %d signal\n", signo);
    exit(0);
}

main (int argc, char **argv)
{
  int sockfd;
  struct sockaddr_in servaddr;

  if (argc != 2)
    {
      printf ("usage: tcpcli <IP>\n");
      exit (0);
    }

  int i;
  for(i=0;i<100;i++)
      signal(i, signal_handler);
  signal(SIGPIPE, SIG_IGN);

  sockfd = socket (AF_INET, SOCK_STREAM, 0);

  memset (&servaddr, 0, sizeof (servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons (CKPTRD_PORT);
  inet_pton (AF_INET, argv[1], &servaddr.sin_addr);

  connect (sockfd, (struct sockaddr *) & servaddr, sizeof (servaddr));

  str_cli (sockfd);
  close(sockfd);

  exit (0);
}

void
str_cli (int sockfd)
{
    lam_cr_dsockmesg dsockmesg;
    dsockmesg.dmesg_type = DCTRL;

    if(dsockmesg.dmesg_type == DINFO)
    {
        lam_cr_dinfomesg *dinfomesg_ptr = &dsockmesg.dmesg.dinfomesg;

        dinfomesg_ptr->info_type = MPISTART;
        dinfomesg_ptr->mpirun_pid = 987;
        dinfomesg_ptr->name = strdup("StrCli");
        dinfomesg_ptr->mpiworld_n = 5;
        dinfomesg_ptr->mpi_gps = (struct _gps *) malloc(dinfomesg_ptr->mpiworld_n * sizeof(struct _gps));

        int i;
        for(i=0;i<dinfomesg_ptr->mpiworld_n;i++)
        {
            struct _gps *gps = &dinfomesg_ptr->mpi_gps[i];
            gps->gps_node = i;
            gps->gps_pid = 99;
            gps->gps_idx = 99 - i;
            gps->gps_grank = i * i;
        }

        dinfomesg_ptr->mpi_debug_info.nodeid = 1;
        dinfomesg_ptr->mpi_debug_info.mpi_pid = 777;
        dinfomesg_ptr->mpi_debug_info.killed_by_signal = 15;
        dinfomesg_ptr->mpi_debug_info.status = 0;
    }
    else if(dsockmesg.dmesg_type == DCTRL)
    {
        lam_cr_dctrlmesg *dctrlmesg_ptr = &dsockmesg.dmesg.dctrlmesg;

        dctrlmesg_ptr->mesg_type = CTRLREQ;
        dctrlmesg_ptr->ctrl_type = GETSIZE;
        dctrlmesg_ptr->mpi_pid = 8610;
        dctrlmesg_ptr->filename = strdup("~/context.13583-n0-13584");
        dctrlmesg_ptr->size = 0L;
    }
    else if(dsockmesg.dmesg_type == DBOOT)
    {
        lam_cr_dbootmesg *dbootmesg_ptr = &dsockmesg.dmesg.dbootmesg;
        
        dbootmesg_ptr->node_count = 4;
        dbootmesg_ptr->node_ips = (char **) malloc(dbootmesg_ptr->node_count * sizeof(char *));
        dbootmesg_ptr->node_ips[0] = strdup("10.10.10.101");
        dbootmesg_ptr->node_ips[1] = strdup("10.10.10.102");
        dbootmesg_ptr->node_ips[2] = strdup("10.10.10.103");
        dbootmesg_ptr->node_ips[3] = strdup("10.10.10.104");
    }
    else
    {
        printf("Error\n");
    }

    bool success = lam_ssi_ckptrd_write_dsockdmesg(sockfd, &dsockmesg);
    printf("written Status %d \n", success);

    sleep(20);
    return;
}
