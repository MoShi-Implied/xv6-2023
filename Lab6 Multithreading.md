[Lab6页面](https://pdos.csail.mit.edu/6.828/2023/labs/thread.html)

## Task1 Uthread: switching between threads

### 为什么只需要保存callee-saved寄存器？
在这部分的**Hints**提到了很有意思的点：
> [!Hints]
> ``thread_switch`` needs to save/restore only the callee-save registers. Why?
> （为什么``thread_switch``只需要保存``callee-saved``寄存器？）

这个问题硬控我很久啊，因为``caller-saved``寄存器在使用的时候都是用于存储临时值，那么若是在寄存器中重写了``caller-saved``寄存器，然后此时发生了进程调度，而我只保存``callee-saved``寄存器，我``caller-saved``中计算的值不就丢失了？这很显然会导致程序执行出错，但是实际上并没有这个问题，因为这个Task实现的是一个用户级的线程，它不会跟一般操作系统中的线程一样，利用时间片或者其它机制来进行进程调度，在此处需要使用函数``thread_yield``进行手动操作：
```c
// user/uthread.c

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
```
因此这是一个流程调用，在执行这个函数之前，``caller-saved``就因为它的原理而被推入栈帧进行保存了……所以这里其实是利用了栈帧的特性，故这里保存``callee-saved``寄存器就行了。

### 具体实现
这个实验实现的是一个用户级的线程，也就是说，==它只是表面上的多线程，而不是实际意义上的多线程==。它并没有使用多个CPU，整个过程也几乎不需要内核的参与。

首先，需要知道，一个线程启动需要一些什么必要的东西。看了OSTEP的应该都晓得，多线程的内存结构和单线程的本质上没有什么很大的区别，它只是多划分了很多的栈（==每个线程都有一个单独的栈，但是所有的线程都使用的是一个堆==）；每个线程有自己的``IP``，因此执行的代码之间没有什么关联。但是``IP``的值我们不用手动进行保存，因为<font color="red">因为在函数调用中，<b>在函数返回后需要执行的下一个指令的地址会被推入栈帧中</b></font>（具体的细节可以看看[[../../计算机系统基础/x86汇编#callq和ret|x86汇编]]中，关于``call``和``ret``指令的部分）。

此处，我们实现多线程的原理**本质上还是流程处理**（可以查看各个线程的入口函数，如``thread_a``），并不是实际的多线程，因此，我们只要知道了每个线程的栈的栈顶指针（``rsp``），就能够在其栈顶找到该线程要执行的下一条指令的地址（栈顶的``rip``）。

由于需要对寄存器进行保存，首先可以模仿``kernel/proc.h``中的``context``：
```c
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
```
将其作为``struct thread``的一部分，其实就相当于实现了一个**TCB**（进程控制块）：
```c
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

// 相当于一个TCB
struct thread {
  char            stack[STACK_SIZE]; /* the thread's stack */ // 为线程预留的栈空间
  int             state;             /* FREE, RUNNING, RUNNABLE */
  struct context  ctx;
};
```
那么，``kernel/setch.S``中的代码（就是用于实现进程的上下文切换操作，也是保存和恢复同样的``context``）就能够完全抄到``user/uthread_switch.S``中了：
```asm
/*
*	user/uthread_switch.S
*/
	.text

	/*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */
	/*ret     return to ra */
		sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)

        ret
```

然后就能够根据提示，去对函数``thread_schedule``和``thread_create``进行修改。

``thread_schedule``是实现线程切换的主逻辑，里面的内容其实很简单，进行线程的上下文切换，也就是对``struct context``进行交换，这个其实就是使用之前抄下来的，在``uthread_switch.S``中定义的函数``thread_switch``：
```c

void 
thread_schedule(void)
{
	...
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
	...
}
```

``thread_creath``则是在创建线程的时候，为线程分配已有、但未使用的线程结构：
```c
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
```
主要就三步：
1. 将空闲进程结构的状态变为可运行
2. 为线程结构添加入口函数（也就是设置寄存器``ra``）
3. 给它一个线程栈

<font color="red"><b>这个任务到这里就结束了，虽然代码不是很多，但是我觉得做下来还是有点难度的，我就因为走入了一个误区被困了很久</b></font>。
> [!Thinking]
> 我一直在思考，到底为什么只需要保存callee-saved寄存器（一开始我没有认真去看thread_a这几个入口函数，以为我要实现的内容就是真正意义上的多线程）。
> 我一直在思考：caller-saved寄存器若是正在进行计算，并且没有写回，然后发生了线程切换，岂不是会导致caller-saved寄存器的数据丢失，因此多线程此时就丧失了它结果的确定性，出现错误。

## Task2 Using threads
很简单的任务，就是实现多线程的并发正确，我做起来很顺，几分钟就搞定了，这个任务本质上也是分为两部分。

### 1. 实现正确的hash并发
它默认提供的代码在并发时是会出现错误的，因为``put``函数会同时进入**竞争区**，所以最简单的方法就是==一把大锁保平安==，进入竟态区部分锁上：
```c
// notxv6/ph.c
...
pthread_mutex_t mtx;
...

static 
void put(int key, int value)
{
  ..
  // is the key already present?
  pthread_mutex_lock(&mtx); // here!!
  
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    insert(key, value, &table[i], table[i]);
  }
  
  pthread_mutex_unlock(&mtx); // here!!
}

int main() {
	... 
	pthread_mutex_init(&mtx, NULL);
	...
}
```
只有修改部分需要上锁，读取是不会有问题的，因此``get``不需要进行修改。这样就成功通过了safe检查，但是这样没法通过time检查，一把大锁实在是太慢了。

### 2. 加速
一把大锁锁住实在是太慢了，在我电脑上2个线程都需要很长时间：
```shell
xfy@LAPTOP-H2UCBAVD:~/os/xv6$ ./ph 2
100000 puts, 7.497 seconds, 13338 puts/second
0: 0 keys missing
1: 0 keys missing
200000 gets, 5.643 seconds, 35445 gets/second
```
不出意外，time超时了……花了整整24s……

在Hints中已经给出了提示：==给每个桶一把锁==。看了下代码中这个hash的实现：
- 它只是简单的分为5个桶
- 每个桶中是一个链表
- 在``insert``的时候使用头插法

所以每个桶一把锁是个很容易的想法，==不同的线程处理不同的桶是不会有并发问题的==，代码就只需要将mtx变为一个数组就好了，很简单。

## Task3 Barrier
这个任务也比较简单，当所有的线程都使用了``barrier``后，再对计数器进行增加：
```c
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  if(bstate.nthread != nthread)
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  else {
    bstate.nthread = 0;
    bstate.round++;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```