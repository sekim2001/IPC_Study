#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>


int main()
{
    int pipe_fd[2];
    pid_t pid;

    // 파이프 생성
    if (pipe(pipe_fd) == -1)
    {
        perror("Pipe");
        exit(EXIT_FAILURE);
    }

    // 프로세스 복제 (우선순위 괄호 주의!)
    pid = fork();
    if (pid == -1)
    {   
        perror("Fork");
        exit(EXIT_FAILURE);
    }

    // 부모 프로세스 
    if (pid > 0)
    {
        close(pipe_fd[0]);  // 읽기 FD 닫기

        int msg[] = {10, 20, 30, 40, 50};
        int length = sizeof(msg) / sizeof(msg[0]);

        for (size_t i = 0; i < length; i++)
        {
            // msg[i]의 "주소(&)"를 전달하여 int 크기만큼 전송
            write(pipe_fd[1], &msg[i], sizeof(int));
            sleep(0.5);
        }
        close(pipe_fd[1]);

        wait(NULL);
    }
    // 자식 프로세서
    else 
    {
        close(pipe_fd[1]);  // 읽기 FD 닫기

        int sum = 0;
        int val = 0;

        while (read(pipe_fd[0], &val, sizeof(int)) > 0)
        {
            printf("[child] recevied : %d\n", val);
            sum += val;
        }
        close(pipe_fd[0]);

        printf("[child] summary : %d\n", sum);
        exit(EXIT_SUCCESS);
    }

    return 0;
}