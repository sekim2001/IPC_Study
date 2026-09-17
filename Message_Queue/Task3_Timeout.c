#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/wait.h>

#define MQ_NAME "/my_mq"
#define MAX_COUNT 10
#define MAX_SIZE 1024
#define TIMEOUT 2

int main()
{
    mq_unlink(MQ_NAME);

    struct mq_attr attr = 
    {
        .mq_curmsgs = 0,
        .mq_flags = 0,
        .mq_maxmsg = MAX_COUNT,
        .mq_msgsize = MAX_SIZE
    };

    mqd_t mqd = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mqd == (mqd_t)-1)
    {
        perror("mq_open error");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == (pid_t)-1)
    {
        perror("fork error");
        exit(EXIT_FAILURE);
    }

    // parent prossess -> receiver
    if (pid > 0)
    {
        struct timespec timeout;
        char buffer[MAX_SIZE + 1];

        while (1)
        {
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += TIMEOUT;

            ssize_t read_bytes = mq_timedreceive(mqd, buffer, sizeof(buffer), NULL, &timeout);
            if (read_bytes >= 0) 
            {
                printf("[REV] PONG\n");
            }
            else 
            {
                if (errno == ETIMEDOUT) printf("[Warning]센서 연결 끊김 (Timeout)\n");
                break;
            }
        }
        
        wait(NULL);

        mq_close(mqd);
        mq_unlink(MQ_NAME);
    }
    // child prossess -> sender
    else 
    {
        char msg[] = "ping";

        for (int i = 0; i < 5; i++)
        {
            printf("[Send] PING\n");
            mq_send(mqd, msg, sizeof(msg), 1);
            sleep(1);
        }

        mq_close(mqd);

        exit(EXIT_SUCCESS);
    }

    return 0;
}