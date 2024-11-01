## 实验内容
提高对于**syscall**的熟悉程度。
[lab1 web](https://pdos.csail.mit.edu/6.828/2023/labs/util.html)
## sleep
创建文件sleep.c，内容很简单：
```c
// #include "kernel/stat.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "Error! There are no two arguments!");
    exit(1);
  }

  // if((argv[1]))
  int num = atoi(argv[1]);
  if(sleep(num) < 0) {
    fprintf(2, "Sleep Error!");
    exit(1);
  }
  exit(0);
}
```
唯一要注意的就是需要在**Makefile**中加上一段话，添加对sleep.c文件的编译。

## pingpong
我在写这个的时候还是遇到了问题，问题在于<font color="red">我对pipe系统调用不熟悉</font>，这里再说说``pipe``系统调用的[[#pipe系统调用|原理]]是什么。
<font color="red"><b>注意：关闭的文件描述符不能重新打开！！！open系统调用是打开文件而不是打开文件描述符！</b></font>

### pipe系统调用
``pipe(int*)``系统调用会创建一个管道，放入pipe传入的参数（一个长度为2的数组）中：
```c
#include <unistd.h>

int pipefd[2];
if(pipe(pipefd)) < 0) {
	...
}
```
在使用了pipe系统调用之后，``pipefd``中放置了两个**文件描述符**，其中``pipefd[0]``用于“读”，而``pipe[1]``用于“写”。

在本题中，我们是父子进程之间的交流，在创建子进程的时候使用的是``fork``系统调用。我们都很清楚：==fork系统调用的本质其实是对父进程的一个完美复制，包括变量、PC等==，所以在子进程中，==我们使用pipe所创建的``pipefd``和父进程中的``pipefd``的是一样的==，因此我们使用这个``pipefd``搭配``read``和``wirte``就能完成交流。

```c
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
```

## primes
写不明白，先pass了。

### 代码
还是做出来了，其实也不是很难，主要是一个思路问题。

在我的思路中，我只用父进程对需要过滤的数组进行一个准备工作，==过滤工作实际上是在子进程中进行的==，子进程对当前非**合数**的数字进行筛选，然后将符合要求的数字通过管道发送给父进程，父进程在收到后，将其放入数组中，然后进行递归调用。

代码如下：
```c
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
```

## find
做了很久，遇到了挺多问题的：
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define INFO(msg) printf("%s\n", msg)

// 找到的结果是“/filename”
char *basename(const char *pathname) {
  char *prev = 0;
  char *curr = strchr(pathname, '/');
  while (curr != 0) {
    prev = curr;
    curr = strchr(curr + 1, '/');
  }
  return prev;
}

void find(const char* path, const char* target) {
  char buff[512], *p;
  int fd;
  if((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "Open %s false!\n", path);
    return;
  }

  struct stat st;
  struct dirent de;

  if(fstat(fd, &st) < 0) {
    fprintf(2, "Error fstat!\n");
    close(fd);
    return;
  }

  switch(st.type) {
    case T_FILE: {
      char* filename = basename(path);
      if(filename == 0 || strcmp(filename + 1, target) != 0) {
        close(fd);
        return;
      }
      printf("%s\n", path);
      close(fd);

      break;
    }

    case T_DIR:
      // INFO(path);

      memset(buff, 0, sizeof(buff));
      strcpy(buff, path);
      p = buff + strlen(buff);
      *p = '/';
      p++;

    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0 || 
        strcmp(de.name, ".") == 0 ||
        strcmp(de.name, "..") == 0) {
          continue;
        }

        memcpy(p, de.name, sizeof(de.name));
        p[sizeof(de.name)] = 0;
        find(buff, target);
    }
    close(fd);
    break;
  }
}

int main(int argc, char* argv[]) {
    if(argc != 3) {
        fprintf(2, "Error arguments!\n");
        exit(1);
    }

    find(argv[1], argv[2]);
    exit(0);
}
```

## xargs
<font color="red">exec函数传入的参数和我们在终端中直接调用该程序输出的参数应该是一样的，也就是说，argv[0]仍然是exec需要调用的程序名！！！</font>
在提示中，提示需要使用``fork``、``wait``和``exec``，但是实际在写的时候，我发现好像我只使用exec就能完成任务：我只需要将参数准备好，然后使用exec调用指定程序就能完成任务，而我若是使用fork，理论上来说，甚至还会让程序运行变慢许多。

所以我还不是很明白为什么需要使用``fork``，以下代码是无``fork``版本，``fork``使用``run``进行了一个简单的包装，在``main``中将``exec``更改为``run``，并且将注释掉的``wait``部分取消，就是对应的使用``fork``的版本：
```c
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
```