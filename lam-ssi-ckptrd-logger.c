# include "lam-ssi-ckptrd-includes.h"

void strchomp(char*);
void log_message(const char*, const char*);

void
lam_ssi_ckptrd_log_message(const char *message, int priority)
{
    char log_class[4][10] = {"FINE", "INFO" ,"WARNING" ,"FATAL"};

    if(priority < 0 || priority > 3)
        priority = 1;

    log_message(message, log_class[priority]);
    return;
}

void
log_message(const char *message, const char *log_class_str)
{
    FILE *logfile;
    logfile = fopen(LOG_FILE, "a");

    /* error cant open LOG_FILE for writing */
    if(!logfile)
        return;

    time_t time_now;
    if(!time(&time_now))
        return;

    char *message_temp = strdup(message);
    char *time_str = asctime(localtime(&time_now));
    strchomp(time_str);
    printf("\n-----------\n");
    strchomp(message_temp);
    printf("\n-----------\n");

    int max_size = strlen(message_temp) + 50;
    char *log_message_buffer = (char *) malloc(max_size);
    snprintf(log_message_buffer, max_size, "%s [%s]: %s", log_class_str, time_str, message_temp);

    fprintf(logfile, "%s\n", log_message_buffer);
	fclose(logfile);

    free(message_temp);
    free(log_message_buffer);

    return;
}

void
strchomp(char *str)
{
    if(str == NULL)
        return;

    int length;
    length = strlen(str);

    int i;
    for(i=length-1;i>=0;i--)
    {
        if(str[i] != '\n')
        {
            str[i+1] = '\0';
            break;
        }
    }

    return;
}

/*
int
main()
{
    char *str;

    time_t time_now;
    time(&time_now);
    str = asctime(localtime(&time_now));

    printf("BEGIN:%s:END\n", str);
    strchomp(str);
    printf("BEGIN:%s:END\n", str);
}
*/
