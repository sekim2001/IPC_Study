#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <mqueue.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#define MQ_NAME "/my_mq"
#define MAX_SIZE 1024

mqd_t mqd;
struct sigevent sev; // 재등록을 위해 전역으로 선언

void sigusr1_handler(int signo)
{
    (void)signo;
    char buffer[MAX_SIZE + 1];
    ssize_t read_bytes;

    // 1. 연산자 우선순위 괄호 수정
    while ((read_bytes = mq_receive(mqd, buffer, MAX_SIZE, NULL)) >= 0)
    {
        buffer[read_bytes] = '\0'; // '\0' 역슬래시 수정
        printf("[handler] receive : %s\n", buffer); // \n 역슬래시 수정
    }

    // 2. 큐를 모두 비웠다면 One-shot인 mq_notify 재등록
    if (mq_notify(mqd, &sev) == -1)
    {
        perror("mq_notify re-arm error");
    }
}

int main() 
{
    mq_unlink(MQ_NAME);

   struct mq_attr attr = 
   {
        .mq_curmsgs = 0,
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = MAX_SIZE
   };
    
    // O_NONBLOCK 필수
   mqd = mq_open(MQ_NAME, O_CREAT | O_RDWR | O_NONBLOCK, 0666, &attr);
   if (mqd == (mqd_t) -1)
   {
        perror("open mq");
        exit(EXIT_FAILURE);
   }

   pid_t pid = fork();
   if (pid == (pid_t)-1) 
   {
        perror("fork error");
        exit(EXIT_FAILURE);
   }

   // parent prossess -> Listener 
   if (pid > 0)
   {
        signal(SIGUSR1, sigusr1_handler);

        // sigevent 설정 (memset 후 값 설정)
        memset(&sev, 0, sizeof(sev));
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGUSR1;
        
        if (mq_notify(mqd, &sev) == -1)
        {
            perror("mq_notify error");
            exit(EXIT_FAILURE);
        }

        int count = 0;
        // 테스트 편의상 15초 동안 백그라운드 작업 수행 후 종료
        while (count < 15)
        {
            count++;
            printf("[listener] count : %d\n", count);
            sleep(1);
        }
        
        wait(NULL);
        mq_close(mqd);
        mq_unlink(MQ_NAME);
   }
   // child prossess -> sender
    else 
    {
        // 5초 후 메시지 전송 테스트
        sleep(5);

        char msg[] = "Message from sender";
        printf("[sender] send : %s\n", msg);
        mq_send(mqd, msg, strlen(msg), 1);

        mq_close(mqd);
        exit(EXIT_SUCCESS);
    }

    return 0;
}