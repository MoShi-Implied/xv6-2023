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