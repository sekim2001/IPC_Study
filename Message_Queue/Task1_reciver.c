#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MQ_NAMW "/my_queue"
#define MAX_MSG 10
#define MAX_SIZE 1024

int main()
{
    // 이전 잔여 큐 정리
    mq_unlink(MQ_NAMW);

    struct mq_attr attr = 
    {
        .mq_flags = 0,
        .mq_maxmsg = MAX_MSG,
        .mq_msgsize = MAX_SIZE,
        .mq_curmsgs = 0
    };

    mqd_t mqd = mq_open(MQ_NAMW, O_CREAT | O_RDONLY, 0666, &attr);
    if (mqd == (mqd_t)-1) 
    {
        perror("mq_open"); 
        exit(EXIT_FAILURE);
    } 

    printf("start listening ...\n");

    while (1) 
    {
        // 널 문자를 고려하여 +1 바이트 정의
        char buffer[MAX_SIZE + 1];
        unsigned int priority = 0;

        ssize_t read_bytes = mq_receive(mqd, buffer, MAX_SIZE, &priority);
        if (read_bytes >= 0) 
        {
            buffer[read_bytes] = '\0';  // 문자열 안정성 보장
            printf ("[Receive] Msg : %s (우선순위 : %u)\n", buffer, priority);

            if (strcmp(buffer, "exit") == 0) 
            {
                printf("exit 수신\n");
                break;
            }
        }
        else
        {
            perror("mq_receive 에러");
            break;
        }
    }

    // 서버가 리소스 닫고 시스템에서 제거
    mq_close(mqd);
    mq_unlink(MQ_NAMW);

    return 0;
}