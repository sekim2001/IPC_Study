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

typedef struct {
    sem_t sem_parent;
    sem_t sem_child;
    char sem_data [1024];
} shm_data_t;

int main ()
{
    pid_t pid;

    shm_unlink (SHM_NAME);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(shm_data_t)) == -1) 
    {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    shm_data_t *shm_ptr = (shm_data_t*)mmap(NULL, sizeof(shm_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) 
    {
        perror("mmap");
        exit(EXIT_FAILURE); 
    }

    sem_init(&shm_ptr->sem_parent, 1, 0);
    sem_init(&shm_ptr->sem_child, 1, 0);

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE); 
    }

    if (pid > 0)
    {
        unsigned int index = 0;
        char msg[] = "ping";

        while (index < 10)
        {
            snprintf(shm_ptr->sem_data, sizeof(shm_ptr->sem_data), "%s : [%d]", msg, ++index);

            sem_post(&shm_ptr->sem_parent);

            sem_wait(&shm_ptr->sem_child);
            
            printf("[Parent] receved: %s\n", shm_ptr->sem_data);
            fflush(stdout);
        }

        wait(NULL);

        sem_destroy(&shm_ptr->sem_parent);
        sem_destroy(&shm_ptr->sem_child);
        munmap(shm_ptr, sizeof(shm_data_t));
        close(shm_fd);
        shm_unlink(SHM_NAME);
        printf("parent prossess end\n");
    }
    else
    {
        unsigned int index = 0;
        char msg[] = "pong";

        while (1)
        {
            sem_wait(&shm_ptr->sem_parent);
            printf("[Child] receved: %s\n", shm_ptr->sem_data);
            fflush(stdout);

            snprintf(shm_ptr->sem_data, sizeof(shm_ptr->sem_data), "%s : [%d]", msg, ++index);
            sem_post(&shm_ptr->sem_child);
            if (index >= 10) break;
        }
        
        munmap(shm_ptr, sizeof(shm_data_t));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }
}
