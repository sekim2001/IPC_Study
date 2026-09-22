#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/wait.h>

#define SHM_NAME "/my_queue"
#define SHM_SIZE 1024

typedef struct
{
    sem_t sem;
    char buffer[SHM_SIZE];
}shm_data_t;

int main()
{
    shm_unlink(SHM_NAME);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); exit(EXIT_FAILURE); }

    if (ftruncate(shm_fd, sizeof(shm_data_t)) == -1) { perror("ftruncate"); exit(EXIT_FAILURE); }

    shm_data_t* shm_data = (shm_data_t*) mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) { perror("mmap"); exit(EXIT_FAILURE); }

    if (sem_init(&shm_data->sem, 1, 0) == -1) { perror("sem_init"); exit(EXIT_FAILURE); }

    printf("메시지 대기 중...\n");

    while (1)
    {
        sem_wait(&shm_data->sem);

        printf("[reader] : %s\n", shm_data->buffer);

        if (strcmp(shm_data->buffer, "exit") == 0)
        {
            break;
        }
    }

   sem_destroy(&shm_data->sem);
    munmap(shm_data, sizeof(shm_data_t));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    return 0;
}