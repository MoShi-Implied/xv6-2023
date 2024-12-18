#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/* Possible states of a thread: */
#define FREE        0x0
#define RUNNING     0x1
#define RUNNABLE    0x2

#define STACK_SIZE  8192 // 每个栈的大小
#define MAX_THREAD  4    // 最大线程个数

struct context {
  uint64 ra;  // 返回地址
  uint64 sp;  // 栈基地址

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  char            stack[STACK_SIZE]; /* the thread's stack */ // 为线程预留的栈空间
  int             state;             /* FREE, RUNNING, RUNNABLE */
  struct context  ctx;
};

struct thread all_thread[MAX_THREAD];
struct thread *current_thread; // 当前正在运行的线程
extern void thread_switch(uint64, uint64);
              
void 
thread_init(void)
{
  // main() is thread 0, which will make the first invocation to
  // thread_schedule(). It needs a stack so that the first thread_switch() can
  // save thread 0's state.
  current_thread = &all_thread[0]; // 初始化为最开始的线程
  current_thread->state = RUNNING; // 并标识为正在运行中
}

void 
thread_schedule(void)
{
  struct thread *t, *next_thread;

  /* Find another runnable thread. */
  next_thread = 0; // 这个表示要运行的线程
  t = current_thread + 1; // t是正在进行探测的线程
  for(int i = 0; i < MAX_THREAD; i++){
    if(t >= all_thread + MAX_THREAD) // 若是越界，就重新标记为main线程（0下标）
      t = all_thread;
    if(t->state == RUNNABLE) { // 标记为可用
      next_thread = t; // next_thread就是下一个要执行的线程？
      break; // 找到一个可用的线程就退出
    }
    t = t + 1;
  }

  if (next_thread == 0) { // 没有符合要求的线程
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
     thread_switch((uint64)&t->ctx, (uint64)&next_thread->ctx); // 这里只切换了已经保存的callee寄存器
                                    // 但是caller寄存器没有管
                                    // 同时这里可以认为只是关键寄存器的切换
                                    // 在完成成功的切换后，栈位置等关键信息就已经被保存了
                                    // 很自然的就完成了线程的切换
                                    // 此处的线程切换实际上是单CPU的
  } else
    next_thread = 0;
}

void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break; // 如果这个线程还未分配，就更改该线程的状态，使其为新分配的线程
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE

  t->ctx.ra = (uint64)func;
  // 先为每个线程分配一个新的栈
  t->ctx.sp = (uint64)(t->stack + (STACK_SIZE - 1)); // 更改栈顶指针的位置，因为栈底是高地址;
}

void 
thread_yield(void)
{
  current_thread->state = RUNNABLE;
  thread_schedule();
}

volatile int a_started, b_started, c_started;
volatile int a_n, b_n, c_n;

void 
thread_a(void)
{
  int i;
  printf("thread_a started\n");
  a_started = 1;
  while(b_started == 0 || c_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_a %d\n", i);
    a_n += 1;
    thread_yield();
  }
  printf("thread_a: exit after %d\n", a_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_b(void)
{
  int i;
  printf("thread_b started\n");
  b_started = 1;
  while(a_started == 0 || c_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_b %d\n", i);
    b_n += 1;
    thread_yield();
  }
  printf("thread_b: exit after %d\n", b_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_c(void)
{
  int i;
  printf("thread_c started\n");
  c_started = 1;
  while(a_started == 0 || b_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_c %d\n", i);
    c_n += 1;
    thread_yield();
  }
  printf("thread_c: exit after %d\n", c_n);

  current_thread->state = FREE;
  thread_schedule();
}

int 
main(int argc, char *argv[]) 
{
  a_started = b_started = c_started = 0;
  a_n = b_n = c_n = 0;
  thread_init();
  thread_create(thread_a);
  thread_create(thread_b);
  thread_create(thread_c);
  current_thread->state = FREE;
  thread_schedule();
  exit(0);
}
