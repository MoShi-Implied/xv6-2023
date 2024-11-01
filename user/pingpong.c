#include "kernel/types.h"
#include "user/user.h"
// #include <unistd.h>

int main() {
    int f_pipefd[2], c_pipefd[2];
    
    // 创建管道
    if(pipe(f_pipefd) == -1) {
        fprintf(2, "Error pipe!");
        exit(1);
    }
    if(pipe(c_pipefd) == -1) {
        fprintf(2, "Error pipe!");
        exit(1);
    }

    int child_pid = fork();
    if(child_pid == -1) {
        fprintf(2, "Error fork!");
        exit(1);
    }

    // 在子进程中
    if(child_pid == 0) {
        // 关闭c写端
        close(c_pipefd[1]);

        char msg[1];
        read(c_pipefd[0], msg, sizeof(msg));
        fprintf(1, "%d: received ping\n", getpid());

        close(c_pipefd[0]); // c写段用完了不需要了
        close(f_pipefd[0]); // 关闭ff读端
        
        write(f_pipefd[1], msg, sizeof(msg));
        close(f_pipefd[0]);
    }
    // 父进程中
    else {
        close(c_pipefd[0]);
        char msg[1] = "a";
        write(c_pipefd[1], msg, sizeof(msg));

        close(c_pipefd[1]);
        close(f_pipefd[1]);
        read(f_pipefd[0], msg, sizeof(msg));
        fprintf(1, "%d: received pong\n", getpid());

        close(f_pipefd[0]);
    }
    exit(0);
}