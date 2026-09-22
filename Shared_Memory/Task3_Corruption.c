#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/wait.h>

#define SHM_NAME "/embedded_shm"
#define SHM_SIZE 1024

#define PID_NUM 4

typedef struct {
    sem_t sem_sync;                              // 프로세스 간 동기화 무명 세마포어
    // volatile int ready;   // 자식 프로세스 동시 출발용 플래그
    volatile int counter; // 컴파일러 최적화 방지 (매번 메모리 R/W 강제)
} shm_data_t;

int main()
{
    pid_t pid[PID_NUM];

    shm_unlink(SHM_NAME);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);    
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(shm_data_t)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    shm_data_t* shm_ptr = (shm_data_t*)mmap(NULL, sizeof(shm_data_t),
                                            PROT_READ | PROT_WRITE,
                                            MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // 초기값을 1로 설정
    sem_init(&shm_ptr->sem_sync, 1, 1); 
    
    // 초기화
    // shm_ptr->ready = 0;
    shm_ptr->counter = 0;

    for (int i = 0; i < PID_NUM ; i ++)
    {
        pid[i] = fork();

        if (pid[i] < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid[i] == 0)
        {
            // 부모가 4개 프로세스를 모두 fork할 때까지 대기 (동시성 극대화)
            // while (!shm_ptr->ready);

            for (int j = 0; j < 100000; j++)
            {
                sem_wait(&shm_ptr->sem_sync); // 임계 구역 진입 (Lock)
                shm_ptr->counter++;
                sem_post(&shm_ptr->sem_sync); // 임계 구역 탈출 (Unlock)
            }

            munmap(shm_ptr, sizeof(shm_data_t));
            close(shm_fd);
            exit(EXIT_SUCCESS);
        }
    }

    // usleep(10000); // 10ms 대기 후 출발 -> 모든 자식 프로세스 동시 시작
    // shm_ptr->ready = 1;

    // 4개 자식 프로세스 모두 대기
    for (int i = 0; i < PID_NUM; i++) {
        wait(NULL);
    }

    printf("[parent] counter = %d\n", shm_ptr->counter);

    sem_destroy(&shm_ptr->sem_sync);
    munmap(shm_ptr, sizeof(shm_data_t));
    shm_unlink(SHM_NAME);
    close(shm_fd);
}
