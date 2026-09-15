#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/wait.h>

#define MQ_NAME "/embedded_mq"
#define MAX_MSG_SIZE 256
#define MAX_MSG_COUNT 10

int main()
{
    pid_t pid;

    // 이전 실행에서 남아있을 수 있는 큐 정리
    mq_unlink(MQ_NAME);

    // 1. 메시지 큐 속성 설정
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = MAX_MSG_COUNT,
        .mq_msgsize = MAX_MSG_SIZE,
        .mq_curmsgs = 0
    };

    // 2. 메시지 큐 생성 및 열기
    // 권한은 0666으로 설정
    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    // 3. 프로세스 생성 (부모-자식 분기)
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) // 부모 프로세스 (Sender)
    {
        char mq_msg[] = "[MQ] Critical Event Triggered";
        unsigned int priority = 1; // 메시지 우선순위 (0 이상, 높을수록 먼저 처리됨)

        printf("[Parent] 메시지 큐로 전송: \"%s\" (우선순위: %u)\n", mq_msg, priority);

        // 메시지 큐로 전송
        if (mq_send(mq, mq_msg, strlen(mq_msg) + 1, priority) == -1) {
            perror("mq_send");
        }

        // 자식 프로세스 종료 대기
        wait(NULL);

        // 메시지 큐 리소스 해제
        mq_close(mq);
        mq_unlink(MQ_NAME); // 시스템에서 메시지 큐 삭제
        printf("[Parent] 메시지 큐 해제 및 부모 프로세스 종료\n");
    }
    else // 자식 프로세스 (Receiver)
    {
        // 주의: 수신 버퍼의 크기는 큐의 mq_msgsize 이상이어야 함 (그렇지 않으면 EMSGSIZE 에러 발생)
        char buffer[MAX_MSG_SIZE];
        unsigned int priority = 0;

        // 메시지 큐로부터 수신 대기 (블로킹)
        ssize_t bytes_read = mq_receive(mq, buffer, MAX_MSG_SIZE, &priority);
        if (bytes_read >= 0) {
            printf("[Child]  메시지 큐 수신 완료: \"%s\" (우선순위: %u, 바이트: %zd)\n",
                   buffer, priority, bytes_read);
        } else {
            perror("mq_receive");
        }

        // 자식 프로세스에서 큐 디스크립터 닫기
        mq_close(mq);
        exit(EXIT_SUCCESS);
    }

    return 0;
}
