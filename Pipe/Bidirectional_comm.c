#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    // [0]: 읽기, [1]: 쓰기
    int parent_to_child[2], child_to_parent[2];
    pid_t pid;

    // 파이프 생성
    if (pipe(parent_to_child) == -1)
    {
        perror("Pipe");
        exit(EXIT_FAILURE);        
    }

    if (pipe(child_to_parent) == -1)
    {
        perror("Pipe");
        exit(EXIT_FAILURE);        
    }
        
    // 프로세스 복제
    pid = fork();
    if (pid < 0)
    {
        perror("Fork");
        exit(EXIT_FAILURE);   
    }

    // 부모 프로세스
    if (pid > 0)
    {
        close(parent_to_child[0]); // parent_to_child 읽기 FD 닫기
        close(child_to_parent[1]); // child_to_parent 쓰기 FD 닫기

        char msg[] = "Ping";
        char buffer[1024]; 

        // ping 쓰기
        write(parent_to_child[1], msg, sizeof(msg));
        close(parent_to_child[1]);

    // pong 읽기
    #if 0
        // pong 읽기
        read(child_to_parent[0], buffer, sizeof(buffer)); 
        printf("[Parent] Received : %s\n", buffer);
    #elif 1
        // 문자열 끝 "/0"를 고려하여 읽기 함수의 버퍼 크기 -1 한다.
        // read() 함수는 성공 시 읽은 바이트 수를 반환하므로, 반환값을 받아 끝에 "\0"을 넣으면 코드 안전성이 높아진다.
        ssize_t bytes = read(child_to_parent[0], buffer, sizeof(buffer)-1);
        if (bytes > 0)
        {
            buffer[bytes] = "\0";
            printf("[Parent] Received : %s\n", buffer);
        }
    #endif
        close(child_to_parent[0]);

        /*
            자식 프러세스가 종료하기 전 부모가 먼저 종료되면 자식 프로세스는 Orphan(고아)나
            Zombie(좀비) 프로세스로 남을 수 있다.

            따라서 부모 프로세스는 자식 프로세스가 종료 될때까지 대기하는 것이 정석이다.
        */
        wait(NULL);
    }
    // 자식 프로세스
    else
    {
        close(parent_to_child[1]); // parent_to_child 쓰기 FD 닫기
        close(child_to_parent[0]); // child_to_parent 읽기 FD 닫기

        char msg[] = "Pong";
        char buffer[1024];

        ssize_t bytes = read(parent_to_child[0], buffer, sizeof(buffer)-1);
        if (bytes > 0)
        {
            buffer[bytes] = "\0";
            printf("[Child] Recevied : %s\n", buffer);
        }close(parent_to_child[0]); 

        write(child_to_parent[1], msg, sizeof(msg));
        close(child_to_parent[1]);

        // 자식 프로세스가 종료됨을 부모에게 알리기 위한 
        exit(EXIT_SUCCESS);
    }

    return 0;
}