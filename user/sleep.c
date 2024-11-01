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