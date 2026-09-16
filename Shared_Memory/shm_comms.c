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

// 공유 메모리에 배치할 구조체 정의
// 프로세스 간 동기화를 위해 구조체 내부에 unnamed POSIX semaphore를 포함
typedef struct {
    sem_t sem_sync;                              // 프로세스 간 동기화 세마포어
    char high_bw_data[SHM_SIZE - sizeof(sem_t)]; // 실제 데이터 저장 영역
} shm_data_t;

int main()
{
    pid_t pid;

    // 이전 실행에서 남아있을 수 있는 공유 메모리 정리
    shm_unlink(SHM_NAME);

    // 1. POSIX 공유 메모리 객체 생성
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // 2. 공유 메모리 크기 설정
    if (ftruncate(shm_fd, sizeof(shm_data_t)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    // 3. 프로세스 가상 메모리 공간에 공유 메모리 매핑
    shm_data_t* shm_ptr = (shm_data_t*)mmap(NULL, sizeof(shm_data_t),
                                            PROT_READ | PROT_WRITE,
                                            MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // 4. 프로세스 간 공유 세마포어 초기화 (pshared = 1, 초기값 = 0)
    if (sem_init(&shm_ptr->sem_sync, 1, 0) == -1) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }

    // 5. 프로세스 생성 (부모-자식 분기)
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) // 부모 프로세스 (Writer)
    {
        // 공유 메모리에 대용량/고속 데이터 직접 쓰기 (복사 오버헤드 최소화)
        const char *msg = "[SHM] Large Image Payload Data";
        snprintf(shm_ptr->high_bw_data, sizeof(shm_ptr->high_bw_data), "%s", msg);
        printf("[Parent] 공유 메모리에 데이터 작성 완료: \"%s\"\n", shm_ptr->high_bw_data);

        // 자식 프로세스가 읽을 수 있도록 세마포어 신호(post) 전달 (값: 0 -> 1)
        sem_post(&shm_ptr->sem_sync);

        // 자식 프로세스 종료 대기
        wait(NULL);

        // 리소스 정리
        sem_destroy(&shm_ptr->sem_sync);
        munmap(shm_ptr, sizeof(shm_data_t));
        close(shm_fd);
        shm_unlink(SHM_NAME); // 공유 메모리 객체 해제
        printf("[Parent] 공유 메모리 정리 및 부모 프로세스 종료\n");
    }
    else // 자식 프로세스 (Reader)
    {
        printf("[Child]  부모 프로세스의 데이터 작성 대기 중...\n");

        // 부모가 데이터를 쓰고 sem_post를 호출할 때까지 블로킹 대기 (값: 1 -> 0)
        sem_wait(&shm_ptr->sem_sync);

        // 공유 메모리에서 데이터 직접 읽기
        printf("[Child]  공유 메모리 읽기 완료: \"%s\"\n", shm_ptr->high_bw_data);

        // 자식 측 매핑 해제 및 파일 디스크립터 닫기
        munmap(shm_ptr, sizeof(shm_data_t));
        close(shm_fd);
        exit(EXIT_SUCCESS);
    }

    return 0;
}
