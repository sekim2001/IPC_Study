#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    int sensor_id;      // 센서 번호 (예: 101, 102, 103)
    float temperature;  // 측정 온도 (예: 26.5, 38.2, 41.0)
    char status[16];    // 상태 문자열 (예: "OK", "WARNING", "DANGER")
} SensorData;

int main()
{
    int pipe_fd[2];

    if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // 자식 프로세스
    if (pid == 0)
    {
        close(pipe_fd[0]);

        SensorData list[3] = {
            {101, 26.5f, "OK"},
            {102, 38.2f, "WARNING"},
            {103, 42.0f, "DANGER"}
        };

        int length = sizeof(list) / sizeof(SensorData);

        for (int i = 0; i < length; i++)
        {
            write(pipe_fd[1], &list[i], sizeof(SensorData));
            sleep(1);
        }

        close(pipe_fd[1]);
        exit(EXIT_SUCCESS);
    }
    else
    {
        close(pipe_fd[1]);

        SensorData rev_msg;

        while (read(pipe_fd[0], &rev_msg, sizeof(SensorData)) > 0)
        {
            printf("[Monitor] 센서 ID: %d | 온도: %.1f°C | 상태: %s\n", rev_msg.sensor_id, rev_msg.temperature, rev_msg.status);
        }
        close(pipe_fd[0]);

        wait(NULL);
    }

    return 0;
}