#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

int
main(int argc, char **argv)
{
    int signo = atoi(argv[1]);
    pid_t pid = atoi(argv[2]);
    printf("Executing `kill -%d %d`\n", signo, pid);
    
    int kill_return = kill(pid, signo);
    printf("kill returned = %d\n", kill_return);
    
    if(kill_return == -1)
    {
        switch(errno)
        {   
            case EINVAL:
                printf("errno - EINTVAL\n");
                break;
            case EPERM:
                printf("errno - EPERM\n");
                break;
            case ESRCH:
                printf("errno - ESRCH\n");
                break;
        }   

    }
}
