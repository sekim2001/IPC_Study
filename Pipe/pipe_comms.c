#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUFFER_SIZE 256

int main()
{
    int pipe_fd[2]; // pipe_fd[0]: 읽기(Read), pipe_fd[1]: 쓰기(Write)
    pid_t pid;

    // 1. 파이프 생성
    // pipe() 함수는 성공 시 0, 실패 시 -1을 반환합니다.
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // 2. 프로세스 생성 (부모-자식 분기)
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) // 부모 프로세스 (Writer)
    {
        // 사용하지 않는 읽기 파이프 닫기
        close(pipe_fd[0]);

        char msg[] = "[PIPE] Sensor Init Ok";
        printf("[Parent] 파이프로 데이터 전송: %s\n", msg);

        // 파이프 쓰기 버퍼에 데이터 전송 (문자열 끝 '\0' 포함 전송)
        if (write(pipe_fd[1], msg, strlen(msg) + 1) == -1) {
            perror("write");
        }

        // 쓰기 완료 후 쓰기 파이프 닫기 (자식 프로세스에 EOF 전달 가능)
        close(pipe_fd[1]);

        // 자식 프로세스가 종료될 때까지 대기
        wait(NULL);
        printf("[Parent] 자식 프로세스 종료 확인, 부모 프로세스 종료\n");
    }
    else // 자식 프로세스 (Reader)
    {
    #if 1
        // 사용하지 않는 쓰기 파이프 닫기
        close(pipe_fd[1]);

        char buffer[BUFFER_SIZE];

        // 파이프로부터 데이터 읽기 (부모가 write할 때까지 블로킹됨)
        ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer));
        if (bytes_read > 0) {
            printf("[Child]  파이프 수신 완료: %s (읽은 바이트: %zd)\n", buffer, bytes_read);
        } else if (bytes_read == 0) {
            printf("[Child]  파이프가 닫혔습니다 (EOF)\n");
        } else {
            perror("read");
        }
    #elif 0 // 비정상 동작

    char buffer[BUFFER_SIZE];

    while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("[Child] 수신: %s\n", buffer);
    }
    printf("[Child] 모든 데이터 수신 완료!\n");

    #endif

        // 읽기 파이프 닫기
        close(pipe_fd[0]);
        exit(EXIT_SUCCESS);
    }

    return 0;
}
