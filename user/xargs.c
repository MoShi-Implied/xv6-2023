#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// void run(char* prog, char** argv) {
//     int fork_pid = fork();
//     if(fork_pid < 0) {
//         fprintf(2, "Error fork!\n");
//         return;
//     }

//     if(fork_pid == 0) {
//         exec(prog, argv);
//         exit(0);
//     }
//     return;
// }

int main(int argc, char* argv[]) {
    if(argc < 2) {
        fprintf(2, "Error arguments!\n");
        exit(1);
    }

    char buff[1024];
    char* args_buff[128]; // 参数列表
    memset(args_buff, 0, sizeof(args_buff));

    char** args = args_buff;

    for(int i = 1; i < argc; i++) {
        *args = argv[i];
        args++;
    }

    // 从标准输入中获取指令
    char* p = buff, *last_p = buff;
    // 用了管道，管道就是将标准输出变为标准输入
    while(read(0, p, 1) != 0) {
        if(*p == ' ' || *p == '\n') {
            *p = '\0';
            *args = last_p;
            args++;
            last_p = p + 1;
        }
        p++;
    }

    // 参数是正确的
    // printf("%s %s %s %s %s\n", argv[1], *args_buff, *(args_buff+1), *(args_buff+2), *(args_buff+3));

    exec(argv[1], args_buff);

    // 当没有子进程可用的时候，wait会返回-1
    // while(wait(0) != -1) {} // 等待所有子进程结束

    return 0;
}