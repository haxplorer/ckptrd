#include "lam-ssi-ckptrd-includes.h"

void daemonize();
void erroe_signal_handler(int);
void sig_child_handler (int);

/* status stuff */
void showstatus();
void format_duration(double, char *);

void
sig_child_handler(int sig)
{
    pid_t child_pid_t;
    int status;
    
    /* to avoid zombies piling up */
    while(child_pid_t = waitpid(-1, &status, WNOHANG))
    {
        if(child_pid_t > 0)
        {
            char log_buf[100];
            sprintf(log_buf, "Child %d returned with status %d", child_pid_t, WEXITSTATUS(status));
            lam_ssi_ckptrd_log_message(log_buf, FINE);
            break;
        }

        sleep(5);
    }
}

void
error_signal_handler(int sig)
{
    char log_buf[100];

	switch(sig) 
    {
	case SIGSEGV:
        sprintf(log_buf, "Segmentation falult occured at %d (Exiting)", FATAL, getpid());
		lam_ssi_ckptrd_log_message(log_buf, FATAL);
        exit(0);
		break;
	case SIGTERM:
        sprintf(log_buf, "terminate signal sent to cktpr daemon at %d (Exiting)", INFO, getpid());
		lam_ssi_ckptrd_log_message(log_buf, INFO);
		exit(0);
		break;
	}
}

void
daemonize()
{
	if(getppid() == 1)
        return;

    int i, lock_fp;
	
	i=fork();
	if (i < 0)
        exit(1);
    if (i > 0)
        exit(0);

	setsid(); 


	for (i=getdtablesize();i>=0;--i) 
        close(i);

    i = open("/dev/null", O_RDWR);
    dup(i); dup(i);
    umask(027); 
	chdir("/tmp"); 

    char log_buf[50];

    lock_fp = open(LOCK_FILE, O_RDWR|O_CREAT, 0640);
	if (lock_fp < 0)
    {
        sprintf(log_buf, "Error opening lock file (writing) - %s by %d", LOCK_FILE, getpid()); 
        lam_ssi_ckptrd_log_message(log_buf, WARNING);
        exit(1);
    }

	if (lockf(lock_fp, F_TLOCK, 0) < 0)
    {
        char pid_buf[20] = "\0";
        read(lock_fp, pid_buf, sizeof(pid_buf));

        pid_t ckptr_prev_pid = atoi(pid_buf);

        printf("locked by %d\n", ckptr_prev_pid);
        /* probing for existance of the previous pid sending signal - 0 */
        int kill_return = kill(ckptr_prev_pid, SIGPING);

        if(kill_return == ESRCH)
        {
            if(lockf(lock_fp, F_ULOCK, 0) < 0)
            {
                sprintf("Unlocking %s failed though %d is dead by %d", LOG_FILE, ckptr_prev_pid, getpid());
                lam_ssi_ckptrd_log_message(log_buf, WARNING);
                exit(0);
            }

            if (lockf(lock_fp, F_TLOCK, 0) < 0)
            {
                sprintf(log_buf, "Unable to lock file (writing) - %s by %d", LOCK_FILE, getpid()); 
                lam_ssi_ckptrd_log_message(log_buf, WARNING);
                exit(0);
            }
        }
        else if(kill_return == 0)
        {
            sprintf(log_buf, "ckptrd running already running at %d", ckptr_prev_pid);
            lam_ssi_ckptrd_log_message(log_buf, WARNING);
            exit(0);
        }
    }


    char str[10];
    sprintf(str, "%d\n", getpid());
	int ret = write(lock_fp, str, strlen(str));
    
    char err[20];

    if(ret < 0)
    {
        switch(errno)
        {
        case EBADF:
            sprintf(err, "EBADF");
            break;
        case EINVAL:
            sprintf(err, "EINVAL");
            break;
        case EIO:
            sprintf(err, "EIO");
            break;
        default:
            sprintf(err, "UNKNOWN - %d", errno);                
        }
        sprintf(log_buf, "couldnt write to lock file %s by %d due to %s)", LOCK_FILE, getpid(), err);
        lam_ssi_ckptrd_log_message(log_buf, FATAL);
        exit(0);
    }

    sprintf(log_buf, "ckptrd started at %d", getpid());
    lam_ssi_ckptrd_log_message(log_buf, FINE);

	signal(SIGCHLD, SIG_IGN);
	signal(SIGTSTP, SIG_IGN); 
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, error_signal_handler);  
	signal(SIGSEGV, error_signal_handler);  
}

void
format_duration(double diff, char *duration)
{
    int seconds = (int) diff;
    int millis = (int) ((diff - (double)seconds) * 100.0);
    int minutes = seconds/60;
    int hours = minutes/60;
    seconds %= 60;
    minutes %= 60;

    snprintf(duration, 12, "%02d:%02d:%02d.%02d", hours, minutes, seconds, millis);
    return;
}

void
showstatus()
{
    char str[125];
    char format[50] = "%5d %2d %3d %4d %10s %5d %10s %12s %13s %10ld %s";
    char  title[100] = "\033[1;7m%5s %2s %3s %4s %10s %5s %11s %11s %10s %11s %s \033[22;27m";


    snprintf(str, 124, title, "MPID", "N", "RST", "CKPT", "STATUS ", "SPID", "START TIME", "STOP/RST TIME",
            "DURATION", "CKPT SIZE", "MPI PROCESS NAME");
    printf("%s\n", str);

    char status[4][10] = {"Unknown", "Running", "Finished", "Failed"};
    char start_time[10], stop_time[10], duration[12];

    int *procs = lam_ssi_ckptrd_get_procs();
    int i;
    for(i=0;i<MAX_PID;i++)
    {
        if(procs[i] > 0)
        {
            lam_cr_procinfo *procinfo = lam_ssi_ckptrd_get_procinfo(i);

            strftime(start_time, 10, "%X", localtime(&procinfo->start_time));
            if(procinfo->restarted == 0 && procinfo->proc_status == PROCRUN)
                strcpy(stop_time, "--   ");
            else
                strftime(stop_time, 10, "%X", localtime(&procinfo->stop_time));

            if(procinfo->proc_status == PROCFINISH)
                format_duration(procinfo->stop_time - procinfo->start_time, duration);
            else
                strcpy(duration, "--    ");

            snprintf(str, 124, format, procinfo->mpirun_pid, procinfo->mpiworld_n, procinfo->restarted,
                    procinfo->checkpointed, status[procinfo->proc_status], procs[i], start_time, stop_time,
                    duration, procinfo->size, procinfo->name);
            printf("%s\n", str);
        }
    }
}

int
main(int argc, char **argv)
{
    if(argc > 1)
    {
        if(strcmp(argv[1], "-status") == 0)
            showstatus();

        return 0;
    }

	daemonize();

    /* initailize a approx 2KB shared memory for nodes ip */
    lam_ssi_ckptrd_make_ip_mem();
    lam_ssi_ckptrd_make_proc_mem();

    int listenfd, connfd;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    memset (&servaddr, 0, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(CKPTRD_PORT);

    bind(listenfd, (struct sockaddr *) & servaddr, sizeof (servaddr));

    listen(listenfd, LISTENQ);

    pid_t childpid;

    signal(SIGCHLD, sig_child_handler);

    while(true)
    {
        lam_ssi_ckptrd_log_message("Listening for connections", FINE);
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *) & cliaddr, &clilen);

        lam_ssi_ckptrd_log_message("New socket connection accepted", FINE);

        if ((childpid = fork ()) == 0)
    	{
            char log_buf[50];
            sprintf(log_buf, "Forking process %d", getpid());
            lam_ssi_ckptrd_log_message(log_buf, INFO);

	        close(listenfd);
            lam_ssi_ckptrd_fork_child(connfd);
            close(connfd);

            sprintf(log_buf, "Forked process %d exiting", getpid());
            lam_ssi_ckptrd_log_message(log_buf, INFO);
            exit(0);
	    }

        close(connfd);
    }

    lam_ssi_ckptrd_log_message("CANT GET HERE" , FATAL);
    return 0;
}

