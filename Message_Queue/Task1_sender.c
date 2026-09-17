#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MQ_NAMW "/my_queue"
#define MAX_MSG 10
#define MAX_SIZE 1024

int main()
{
    mqd_t mqd = mq_open(MQ_NAMW, O_WRONLY);
    if (mqd == (mqd_t)-1) 
    {
        perror("mq_open 에러"); 
        exit(EXIT_FAILURE);
    }

    char msg1[] = "Apple";
    printf("send %s\n", msg1);
    if (mq_send(mqd, msg1, sizeof(msg1), 0))
    {
        perror("mq_send 실패");
    } 

    sleep(3);

    char msg2[] = "exit";
    printf("send %s \n", msg2);
    if (mq_send(mqd, msg2, sizeof(msg2), 0)) 
    {
        perror("mq_send 실패");
    }

    // 클라이언트는 닫김잔 하고 unlink하지 않음
    mq_close(mqd);

    return 0;
}