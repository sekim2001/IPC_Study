#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>

#define SHM_NAME "/embedded_shm"
#define MQ_NAME "/embedded_mq"
#define SHM_SIZE 1024
#define MAX_MSG_SIZE 256

typedef struct
{
    sem_t sem_sync;
    char high_bw_data[SHM_SIZE - sizeof(sem_t)];
} shm_data_t;

int main()
{
    int pipe_fd[2];
    pid_t pid;

    // Pipe 초기화
    if (pipe(pipe_fd) == 1) { perror("pipe"); exit(EXIT_FAILURE); }

    // Message Queue 초기화
    struct mq_attr attr = { .mq_flags = 0, .mq_maxmsg = 10, .mq_msgsize = MAX_MSG_SIZE, .mq_curmsgs = 0 };
    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 066, &attr);
    if (mq == (mqd_t) -1) { perror("mq_open"); exit(EXIT_FAILURE);}
    
    // Shared Memory 초기화
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); exit(EXIT_FAILURE);}
    ftruncate(shm_fd, sizeof(shm_data_t));
    shm_data_t* shm_ptr = mmap(0, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    sem_init(&shm_ptr->sem_sync, 1, 0);

    // 프로세스 포크
    pid = fork();
    if (pid < 0) {perror("fork"); exit(EXIT_FAILURE);}

    if (pid > 0) // 부모 프로세서 (Writer)
    {
        close(pipe_fd[0]);      // 읽기 끝 닫기

        // Pipe 전송
        char pip_msg[] = "[PIPE] Sensor Init Ok";
        write(pipe_fd[1], pip_msg, strlen(pip_msg) + 1);

        // Message Queue 전송 
        char mq_msg[] = "[MQ] Critical Event Triggered";
        mq_send(mq, mq_msg, strlen(mq_msg) + 1, 0);

        // shared Memory 전송
        snprintf(shm_ptr->high_bw_data, sizeof(shm_ptr->high_bw_data), "[SHM] Large Image Payload Data");
        sem_post(&shm_ptr->sem_sync); // 자식 프로세스에 읽기 권한 부여

        wait(NULL); // 자식 종료 대기
        close(pipe_fd[1]);
        mq_close(mq);
        mq_unlink(MQ_NAME);
        sem_destroy(&shm_ptr->sem_sync);
        munmap(shm_ptr, sizeof(shm_data_t)); 
        shm_unlink(SHM_NAME);
    }
    else // 자식 프로세스 (Reader)
    {
        close(pipe_fd[1]);      //  쓰기 끝 닫기
        char buffer[MAX_MSG_SIZE];

        // Pipe 수신
        read(pipe_fd[0], buffer, sizeof(buffer));
        printf("child received %s \n", buffer);

        // Message Queue 수신
        mq_receive(mq, buffer, MAX_MSG_SIZE, NULL);
        printf("Child recived %s\n", buffer);

        // Shared Memory 수신
        sem_wait(&shm_ptr->sem_sync); //부모가 쓸 때까지 대기
        printf("Child recevied %s\n", shm_ptr->high_bw_data);

        close(pipe_fd[0]);
        mq_close(mq);
        munmap(shm_ptr, sizeof(shm_data_t));
        exit(EXIT_SUCCESS);
    }

    return 0;
}

