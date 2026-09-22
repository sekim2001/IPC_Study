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

#define QUEUE_CAPACITY 8
typedef struct {
    int buffer[QUEUE_CAPACITY];
    int head; // Reader index
    int tail; // Writer index
    sem_t sem_mutex; // 인덱스 및 버퍼 접근 보호 (초기값 1)
    sem_t sem_empty; // 남은 빈 슬롯 수 (초기값 QUEUE_CAPACITY)
    sem_t sem_full;  // 채워진 데이터 슬롯 수 (초기값 0)
} shared_ring_buffer_t;

int main()
{
    shm_unlink(SHM_NAME);

    pid_t pid;

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);    
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(shared_ring_buffer_t)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    shared_ring_buffer_t* shm_ptr = (shared_ring_buffer_t*)mmap(NULL, sizeof(shared_ring_buffer_t),
                                                                PROT_READ | PROT_WRITE,
                                                                MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // 세마포어 초기화
    sem_init(&shm_ptr->sem_mutex, 1, 1);                // 버퍼 및 인덱스 접근 보호 (Mutex)
    sem_init(&shm_ptr->sem_empty, 1, QUEUE_CAPACITY);   // 남은 빈 슬롯 수 (초기 8칸)
    sem_init(&shm_ptr->sem_full, 1, 0);                 // 채워진 데이터 슬롯 수 (초기 0개)

    // 변수 초기화
    shm_ptr->head = 0;
    shm_ptr->tail = 0;

    pid = fork();

    // parent => consumer
    if (pid > 0)
    {
        for (int i = 1; i <= 100; i++) {
            // 1. 읽을 데이터가 생길 때까지 대기 (버퍼 비면 여기서 자동 블로킹)
            sem_wait(&shm_ptr->sem_full);


            sem_wait(&shm_ptr->sem_mutex);

            int data = shm_ptr->buffer[shm_ptr->head];
            printf("    [Consumer] 읽기: %d (head: %d)\n", data, shm_ptr->head);
            shm_ptr->head = (shm_ptr->head + 1) % QUEUE_CAPACITY;

            sem_post(&shm_ptr->sem_mutex);

            // 대기 중인 생산자 깨움
                        
            sem_post(&shm_ptr->sem_empty);

            // 소비자는 일부러 느리게 읽어서 버퍼가 가득 차는 상황을 유도
            usleep(20000); // 20ms
        }

        wait(NULL);
        sem_destroy(&shm_ptr->sem_mutex);
        sem_destroy(&shm_ptr->sem_empty);
        sem_destroy(&shm_ptr->sem_full); 
        close(shm_fd);
        munmap(shm_ptr, sizeof(shared_ring_buffer_t));
        shm_unlink(SHM_NAME);
    }
    // child => producer
    else
    {
        for (int i = 1; i <= 100; i++) {
            // 8개 다 차면 여기서 자동 블로킹
            sem_wait(&shm_ptr->sem_empty);
        
            sem_wait(&shm_ptr->sem_mutex);
        
            shm_ptr->buffer[shm_ptr->tail] = i;
            printf("[Producer] 쓰기: %d (tail: %d)\n", i, shm_ptr->tail);
            shm_ptr->tail = (shm_ptr->tail + 1) % QUEUE_CAPACITY;

            sem_post(&shm_ptr->sem_mutex);
        
            // 대기 중인 소비자 깨움
            sem_post(&shm_ptr->sem_full);
        
            // 아주 짧은 딜레이 or 딜레이 없음
            usleep(1000); // 1ms
        }

        close(shm_fd);
        munmap(shm_ptr, sizeof(shared_ring_buffer_t));
        exit(EXIT_SUCCESS);
    }

    return 0;
}