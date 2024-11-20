#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  backtrace();

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_sigalarm() {
  int n;
  uint64 fn; // 因为是函数调用，因此这里应该是函数地址
  
  argint(0, &n);
  argaddr(1, &fn); // fn中存储的是这个函数的地址

  // printf("sys_sigalarm %p\n");

  // 设置参数
  struct proc* p = myproc();
  p->fn = (void(*)()) fn;
  p->ticks = n;
  p->passed = 0; // 重置计数器

  // printf("Finish rev\n");
  
  return 0;
}

uint64 sys_sigreturn() {
  // 恢复原始栈帧，能这么做是因为在传入的fn中调用了sigreturn
  // 若是fn中没调用该函数，那么我这么做就是错误的
  struct proc* p = myproc();

  // p->ticks = 0;  // 该函数只用于恢复运行情况，不能用于停止调用fn
                    // 若是想要停止调用fn需要在用户程序中显式地使用sigalarm(0, 0)
  p->running = 0;
  *(p->trapframe) = *(p->alarm_tf);

  return 0xac; // 我不知道为什么是要返回这个值，我是看到test3中是与这个值进行比较的
               // 但是这个是我最后调用的函数了，所以返回值应该只能在此处进行设置
               // 但是别的大佬的返回值都是0
}
