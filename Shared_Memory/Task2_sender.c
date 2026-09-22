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
    char input[SHM_SIZE]; 

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) exit(EXIT_FAILURE);

    shm_data_t* shm_data = (shm_data_t*) mmap(NULL, sizeof(shm_data_t), PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) exit(EXIT_FAILURE);

    printf("문자열 입력 대기... \n");

    while(1)
    {
        printf("> ");

        if (fgets(input, SHM_SIZE, stdin) != NULL)
        {
            input[strcspn(input, "\n")] = '\0';     // 개행 문자(\n) 제거

            strncpy(shm_data->buffer, input, SHM_SIZE - 1);
            
            sem_post(&shm_data->sem);

            if (strcmp(input, "exit") == 0)
            {
                break;
            }
        }
    }

    munmap(shm_data, sizeof(shm_data_t));
    close(shm_fd);

    return 0;
}
