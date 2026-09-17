#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MQ_NAME "/my_queue"
#define MAX_COUNT 10
#define NUM_MSGS 4

typedef struct {
    int sensor_id;
    float value;
    char alert_msg[32];
} Packet;


int main(void)
{
    pid_t pid;

    struct mq_attr attr = 
    {
        .mq_flags = 0,
        .mq_maxmsg = MAX_COUNT,
        .mq_msgsize = sizeof(Packet),
        .mq_curmsgs = 0
    };

    // 이전에 비정상 종료되어 남아있을 수 있는 큐 정리
    mq_unlink(MQ_NAME);

    mqd_t mqd = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mqd == (mqd_t)-1)
    {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1)
    {
        perror("fork error");
        mq_close(mqd);
        mq_unlink(MQ_NAME);
        exit(EXIT_FAILURE);
    }

    // parent process -> receiver 
    if (pid > 0)
    {
        Packet rev_packet;
        unsigned int priority = 0;
        
        wait(NULL);  // wait for child process to finish its work
        
        printf("=== 수신 프로세스: 큐에서 메시지 꺼내기 시작 ===\n");
        
        /*
        
        */
    # if 0 // 내가 작성한 코드
        while (1) {
                mqd_t read_bytes = mq_receive(mqd, &rev_packet, sizeof(Packet), &priority);
                if (read_bytes > 0) 
                { 
                    printf("[receive] ID: %d / value: %.3f / msg : %s (우선순위 : %u)\n", 
                        rev_packet.sensor_id, rev_packet.value, rev_packet.alert_msg, priority);
                }
                else break;
                }       
    #else
        for (int i = 0; i < NUM_MSGS; i++)
        {
            ssize_t read_bytes = mq_receive(mqd, (char *)&rev_packet, sizeof(Packet), &priority);
            if (read_bytes >= 0)
            {
                /*
                    버퍼 포인터 타입 캐스팅: mq_send와 mq_receive는 const char * 및 char *를
                    매개변수로 받으므로, 구조체 주소 전달 시 (const char *)&msg[i], (char
                    *)&rev_packet으로 명시적 형변환을 해주어야 컴파일러 경고(-Wincompatible-pointer-
                    types)가 발생하지 않음.
                */
                printf("[receive] ID: %d / value: %.3f / msg : %s (우선순위 : %u)\n", 
                        rev_packet.sensor_id, rev_packet.value, rev_packet.alert_msg, priority);
            }
            else
            {
                perror("mq_receive");
                break;
            }
        }
    #endif

        mq_close(mqd);
        mq_unlink(MQ_NAME);
    }
    // child process -> sender
    else
    {
        Packet msg[NUM_MSGS] = 
        {
            { .sensor_id = 1, .value = 2.3159, .alert_msg = "first" },
            { .sensor_id = 2, .value = 75.445, .alert_msg = "second" },
            { .sensor_id = 3, .value = 25.156, .alert_msg = "third" },
            { .sensor_id = 4, .value = 2.1536, .alert_msg = "FIRE_EMERGENCY" }
        };

        unsigned int priority[NUM_MSGS] = { 1, 1, 1, 10 };
        
        printf("=== 송신 프로세스: 메시지 전송 시작 ===\n");
        for (int i = 0; i < NUM_MSGS; i++)
        {
            if (mq_send(mqd, (const char *)&msg[i], sizeof(Packet), priority[i]) == -1)
            {
                perror("mq_send");
                break;
            }
            printf("[send] ID: %d / msg : %s (우선순위 : %u) 전송 완료\n", 
                    msg[i].sensor_id, msg[i].alert_msg, priority[i]);
        }

        mq_close(mqd);
        exit(EXIT_SUCCESS);
    }

    return 0;
}