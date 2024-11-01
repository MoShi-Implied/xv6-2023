#include "kernel/types.h"
#include "user/user.h"

void func(int* input, int num) {
    if(num == 0) {
        return;
    }

    int pipefd[2];
    if(pipe(pipefd) < 0) {
        fprintf(2, "Error pipe!\n");
        return;
    }    

    int pid = fork();
    if(pid < 0) {
        fprintf(2, "Error fork!\n");
        return;
    }

    if(pid == 0) { // 子进程对当前的数字进行处理
        int prime = input[0];
        printf("prime %d\n", prime);

        close(pipefd[0]); // 子进程关闭读口
        for(int i = 0; i < num; i++) {
            // 不是能被该质数整除的
            if(input[i] % prime != 0) {
                write(pipefd[1], &input[i], sizeof(input[i]));
            }
        }
        exit(0);
    }

    close(pipefd[1]);
    int counter = 0; // 计算在本回合非合数的数字的个数
    
    // read能保证数据会读完，若是read在write之前开启，read会一直阻塞
    while(read(pipefd[0], input + counter, sizeof(int)) > 0) {
        counter++;
    }
    close(pipefd[0]);

    func(input, counter);
}

int main() {
    int arr[33];
    for(int i = 0; i < 33; i++) {
        arr[i] = i + 2;
    }

    // num是要数组的长度
    func(arr,33);
}