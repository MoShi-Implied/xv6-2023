这个lab其实我一开始是有点没法下手的，就是原理都能懂，但是实现的时候就不晓得，经群友指点，决定先尽可能搞懂进程的页表是如何进行分配的。

## 源码阅读
### vm.c
该文件中包含了大部分虚拟内存部分的代码，一个个来看。

#### kvmmake()
``kvmmake``用于生成**内核的直接映射页表**，这个函数纠正了我的误区，我一直以为内核是直接看到物理内存的，看来这是个错误的观点，但是内核中的虚拟内存的排布和进程中的虚拟内存不一样：==内核虚拟地址和物理地址之间是**简单映射**，而进程虚拟地址和物理地址之间是复杂映射==：
1. 内核的虚拟地址和物理地址之间存在一种连续且简单的映射关系
2. 进程的虚拟地址和物理地址之间的映射则更为复杂且不保证连续性

``kvmmake``中对内核页表进行设置，进行了大量的映射：设备映射、内核代码段映射，内核数据段映射、中断处理程序映射、为每个进程分配并映射内核栈。

#### kvminit()
该函数调用``kvmmake``完成对内核页表的创建。

####  kvminithart()
<font color="red">没太理解，这里就先不说吧</font>

#### walk()
==该函数用于对进程三级页表的创建，对最底层页表的PTE不会进行任何操作。==

xv6中，用户空间的虚拟地址是64位的，尽管该操作系统的内核架构是32位。
先看看关于该函数的注释：
```c
// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
```
1. 返回值：是``va``对应的``PTE``地址
2. 若是传入的参数``alloc``不为0，创建一个需要的页表页
3. RISC-V Sv39 页表结构具有三级页表页。每个页表页包含 512 个 64 位的 PTE，一个 64 位的虚拟地址被分成五个字段：
	1. 39…63 位：必须为零
	2. 30…38 位：9 位的二级索引
	3. 21…29 位：9 位的一级索引
	4. 12…20 位：9 位的零级索引
	5. 0…11 位：12 位的页内偏移量

这下，就对xv6的多级页表系统有了个初步的了解。

对``walk``进行分析：
```c
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}
```

首先，检查``va``的有效性：
```c
if(va >= MAXVA)
	panic("walk");
```
若是传入的虚拟地址大于所允许的最大虚拟地址，就抛出一个``panic``。
```c
  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
```
``walk``从顶级页表开始进行查找（原理见OSTEP多级页表部分），对于每一级，获取其页表条目``pte``，该操作通过索引获得，索引通过位移获得：
```c
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

PX(level, va)
```
这一步其实就是根据之前提示的RISC-V中拥有三级页表的虚拟地址的结构来完成的，通过这一步，==得到了``va``的``n``级页表的``pte``，在多级页表结构中，非0级页表的``pte``就是对应的下一级页表的地址==，因此我们还需要对该``pte``的**有效位**进行检查：
```c
if(*pte & PTE_V)
```
这个``PTE_V``就是valid，若是该页表条目有效，就更新目前的页表地址：
```c
#define PTE2PA(pte) (((pte) >> 10) << 12)

pagetable = (pagetable_t)PTE2PA(*pte);
```
为什么要先左移10位，再右移12位呢？这和RISC-V架构有关，我翻看了RISC-V手册：
![[../../Sv39对va和pa的转换.png]]
从其中能够知道：==地址偏移量offset是12位，``PTE``的低10位都是标志位==。因此这步位移操作实际上是在：==清除标志位，空出物理地址的12位offset==。

若是该``PTE``是无效的，就需要对页表进行一个创建，在创建后插入``PTE``中：
```c
if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
	return 0;
memset(pagetable, 0, PGSIZE);
*pte = PA2PTE(pagetable) | PTE_V;
```
- alloc为0表示不需要分配页表
- ``pagetable = (pde_t*)kalloc()) == 0``则表示页表分配失败

可以注意到，这里又使用了一个宏：
```c
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

*pte = PA2PTE(pagetable) | PTE_V;
```
这个宏用于创建一个PTE，它的原理和从PTE中提取``PA``的原理正好相反，理解了之前的流程这个还是很好理解的。

<font color="red">新的疑惑：不同级的页表之间岂不是需要有一个很特殊的地址？才能实现这个效果？我可能需要去看看页表分配相关的代码。</font>
<font color="red">所有存放页表的物理地址之间是比较紧密的还是相对稀疏的？还是说没有什么关系？</font>

#### walkaddr()
刚刚所说的``walk``是找到``va``所对应的最后一级的PTE，而``walkaddr``是返回``va``所对应的``pa``：
```c
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0) // 确定该PTE是有效的
    return 0;
  if((*pte & PTE_U) == 0) // 确定该PTE是用户能访问的
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}
```
借用之前编写的``walk``获取对应的PTE，然后对该PTE进行解引用，使用位移操作就能很简单的获取其物理地址。
重要的是：<font color="red"><b>不要忘了对有效位的检验！</b></font>（从此也能看出来该函数主要是提供给用户的，因为有用户可访问位的检查，对于内核来说，所有的虚拟空间应该都是可访问的）。

#### kvmmap()
这个函数用于在内核中创建映射，它直接调用了其它的映射函数：
```c
// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}
```
从注释部分就能看出，==该函数用于将映射添加至内核页表中，并且该函数只会在操作系统启动的时候被使用，该函数仅仅是添加映射，不会刷新TLB和启用分页==。

通过搜索该函数的调用，能发现它只有在``kvmmake``中使用了：
```c
// uart registers
kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

// virtio mmio disk interface
kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

// PLIC
kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

// map kernel text executable and read-only.
kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

// map kernel data and the physical RAM we'll make use of.
kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

// map the trampoline for trap entry/exit to
// the highest virtual address in the kernel.
kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
```
可以看出来这段代码是在对内核中的各个内存区域进行一个映射。

#### mappages()
该函数用于创建虚拟内存的内存映射，先来看看函数原型：
```c
// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
```
注释部分就已经将函数的作用和注意事项说的很清楚了：
- 该函数用于将``va``映射到``pa``，映射之后将其放入PTE中
- ``va``和``size``需要满足**页对齐**
- 返回0成功，-1不成功

```c
if((va % PGSIZE) != 0)
panic("mappages: va not aligned");

if((size % PGSIZE) != 0)
panic("mappages: size not aligned");

if(size == 0)
panic("mappages: size");
```
此处满足对其要求和特判，不多说。

以下是是该函数的核心部分：
```c
a = va;
last = va + size - PGSIZE;
for(;;){
if((pte = walk(pagetable, a, 1)) == 0) // kalloc分配失败的时候
  return -1;
if(*pte & PTE_V)
  panic("mappages: remap");
*pte = PA2PTE(pa) | perm | PTE_V;
if(a == last)
  break;
a += PGSIZE;
pa += PGSIZE;
}
```
其中的``last``是最后一页的边界地址。
可以看出来，只有当``kalloc``分配失败的时候，该函数才会返回-1。
``walk``函数的作用别记错了：==它用于确保三级页表的完整性，对第0级页表的PTE是不会进行任何操作==。在根据``va``找到了对应的PTE之后，现需要判断该va是否已经被映射了：
```c
if(*pte & PTE_V)
	painc("...");	
```
若是重复映射就抛出一个painc，若是没有就拼接一个PTE，放入页表中：
```c
*pte = PA2PTE(pa) | perm | PTE_V;
```
其中的``perm``是PTE的诸多**权限位**。

#### uvmunmap()
该函数==用于取消映射==：
```c
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);
```
将从``va``开始的n页全都释放，==``va``需要满足页对齐==。

对齐部分的代码就跳过了，之前说过，看看函数核心：
```c
for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
if((pte = walk(pagetable, a, 0)) == 0)
  panic("uvmunmap: walk");
if((*pte & PTE_V) == 0)
  panic("uvmunmap: not mapped");
if(PTE_FLAGS(*pte) == PTE_V)
  panic("uvmunmap: not a leaf");
if(do_free){
  uint64 pa = PTE2PA(*pte);
  kfree((void*)pa);
}
*pte = 0;
}
```
好像这几个函数都先使用``walk``去保证三级页表结构的完整性，然后再进行操作。

比较有意思的是这一句：
```c
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

if(PTE_FLAGS(*pte) == PTE_V)
  panic("uvmunmap: not a leaf");
```
首先有个知识点就是叫法，再次之前我没法很好的去表述指向物理地址的那一个页表的PTE叫什么，现在可以有个说明了。页表项有两种：
1. **叶子页表项**：指向一个实际的物理地址，包含可读、可写等权限位
2. **非叶子页表项**：指向下一级页表

<font color="red"><b>在大多数情况下，叶子页表项除了有效位还有其它的标志位，如可读可写等，但是非叶子页表项通常只包含下一级页表的地址，而不包含这些标志位</b></font>。其实这一点在``walk``中也能看出来，在填写非叶子页表项时，只进行了位移，而没有做任何其它操作。
```c
/*
	walk()
*/
*pte = PA2PTE(pagetable) | PTE_V; // 向该页面的页表中插入一个PTE
```
能看到仅有一个有效位。

因为只有叶子页表项存储的是``va``所对应的``pa``，因此需要进行此筛选操作。

当传入的``do_free``为非0值的时候，才会真正释放物理内存：
```c
if(do_free){
  uint64 pa = PTE2PA(*pte);
  kfree((void*)pa);
}
```

#### uvmcreate()
==为用户进程创建一个空页表==：
```c
// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}
```

#### uvmfirst()
该函数是==为了操作系统的第一个进程：init进程而编写的==，用于启动init：
```c
// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}
```
该函数对``va``0进行映射，并将其一段源代码``src``放入该页中，在``userinit``对该函数进行了调用：
```c
uvmfirst(p->pagetable, initcode, sizeof(initcode));
```
其中的``initcode``是一串字节码：
```c
// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};
```
很显然，它的长度是小于一页的。

#### uvmalloc()
该函数用于拓展进程内存：
```c
// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.

// 分配PTEs和物理内存，将进程的内存从oldsz拓展到newsz
// newsz不要求页对齐
// 返回new size，若是分配失败则返回0
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
```
其中提到``newsz``是不要求页对齐的，其实该函数实际上在进行内存分配的时候还是以也为单位：
```c
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}
```
这段代码中，首先对``oldsz``进行了页对齐，它用的方法还是很有意思的：
```c
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))

oldsz = PGROUNDUP(oldsz);
```
很巧妙的用位操作进行了向上的页对齐，虽然写不出来，但是稍微想想还是能理解的。（==这种方法是利用了二进制，若是需要对齐的数字不是2的幂，就没法使用这种方法==）。

函数的主体部分是对拓展的部分进行映射：
```c
for(a = oldsz; a < newsz; a += PGSIZE){
	mem = kalloc();
	if(mem == 0){
	  uvmdealloc(pagetable, a, oldsz);
	  return 0;
	}
	memset(mem, 0, PGSIZE);
	if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
	  kfree(mem);
	  uvmdealloc(pagetable, a, oldsz);
	  return 0;
	}
}
```
若是有一个页面分配失败，就会调用``uvmdealloc``将其返回原始大小；若是出现映射失败的情况，同样使其回到原始大小，要注意的是需要释放本次分配的内存。

#### freewalk()
该函数用于释放进程的各级页表，==前提是叶子页表项已经全部删除==：
```c
// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}
```
这个512是由虚拟地址中，使用9位来作为``VPN``得到的。

```c
if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0)
	...
```
这个判断条件其实就是之前说过的，叶子页表项和非叶子页表项的区别就在于非叶子表项没有这些标志位（全为0），所以这一步判断了这个是哪种页表。

为什么说叶子页表项被全部移除是前提？
```c
 else if(pte & PTE_V){
      panic("freewalk: leaf");
}
```
能到这里说明：
1. PTE有效
2. 是叶子页表项

这就说明叶子页表项并没被解除映射，故抛出``panic``。

#### uvmfree()
用于进程内存的销毁：
```c
// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}
```
该函数做了两件事：
1. 将进程的内存映射统统撤销
2. 将各级页表进行释放

#### uvmcopy()
该函数==用于创建一个和父进程完全一样的子进程页表==，我想很典型的一个就是在``fork``中使用：
```c
// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);
```

该函数从``va``0开始，以页为单位逐个进行映射：
```c
for(i = 0; i < sz; i += PGSIZE)
	...
```
使用``walk``找到需要进行复制的PTE，然后检查PTE和有效性和其标志位，记录下来，使用``kalloc``为新页表分配新内存，其内存中的内容也尽数复制后，拼接新的PTE放入新页表中：
```c
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
```

这里有一个小知识点，我在看这段代码的时候生出一个疑惑：
- 为什么``mem``是``char*``类型的？在我看来它应该是``uint64``
我能提出这个问题，其实还是说明了我对指针的理解还是有点问题的，首先要记住一点：<font color="red"><b>指针就是指针，而不是其它的数据结构，它实际上就只是一个虚拟地址，而它的类型只是说明，这个指针以一个怎样的形式去读取这个地址上的数据</b></font>，因此从本质上来说，此处的``char*``和``uint64``并无区别，但是为什么不能都传入``uint64*``呢？这点看``memmove``的原理就能知道了，因为<font color="red"><b>不同类型的指针在自增/自减的时候地址改变的大小不一样</b></font>，如``char*``只会+1，而``uint64``则会+8，这就是很大的不同了。

``uvmcopy``单从逻辑上来说没有什么难点，也就不多说了。

#### uvmclear()
该函数==设置一条无效PTE来阻止用户访问，通常用于在用户栈的防护页（guard page）上设置一个不可访问的页面，以防止用户栈溢出==。
```c
// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}
```

#### copyout()、copyin()和copyinstr()
这里之说``copyout``，因为这三个函数没什么难点，并且逻辑都差不多，容易看得懂（自己写的话想写对是很困难的）。
这个函数在之前lab2的时候已经使用过了，用于==将处于内核的数据拷贝至用户空间==：
```c
// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva); // 向下页对齐
    if(va0 >= MAXVA)
      return -1;
    pte = walk(pagetable, va0, 0);
    if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 ||
       (*pte & PTE_W) == 0)
      return -1;
    pa0 = PTE2PA(*pte);
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```

### kalloc.c
该文件中包含物理内存管理的代码，==该文件中使用的都是物理地址！==管理物理内存使用以下两个结构体：
```c
// 链表节点
struct run {
  struct run *next;
};

// 链表头指针和一个自旋锁
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
```
还有一个不能够忽略的数据：
```c
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
```
它表示的是除了内核占用之后的第一个物理地址。

#### kinit()
初始化**空闲内存链表**和一个锁，将``end``之后的所有内存都放到链表中（为什么``end``之后的是空闲内存？这个要看``kernel.ld``这个链接器脚本，我看不太明白这个）：
```c
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}
```

#### freerange()
用于释放范围内的内存，主要是将其==以页为单位进行管理==：
```c
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}
```

#### kree()
<font color="red">我不是很明白为什么需要将释放的内存填满垃圾数据</font>。

这里链表的维护使用的是头插法：
```c
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
```

#### kalloc()
原理一样简单，只是<font color="red">还是不明白为什么需要将释放的内存填满垃圾数据</font>。

```c
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

```

## Lab部分
至此，源码阅读就完成了，我觉得我是把这两个文件搞得很清楚了。可以开始写lab了。

### speed up syscall
难度不大，但是我觉得想写对还是有点困难的，因为要改的地方有点多，一不小心就会遗漏。

首先是第一点，更改``kernel/proc.c``中的进程结构，增加一个新的变量，用于记录``USYSCALL``的物理地址（模仿``trapframe``）：
```c
// proc.c

struct proc {
...
  struct usyscall *usyscall;
...
};
```

查看用于创建进程的``allocproc``，可以看到在给进程分配了``pid``和更改状态之后，就对``trapframe``分配了物理内存：
```c
// Allocate a trapframe page.
if((p->trapframe = (struct trapframe *)kalloc()) == 0){
	freeproc(p);
	release(&p->lock);
	return 0;
}
```
因此，模仿该代码，给``usyscall``分配物理地址：
```c
// 为usyscall分配内存
if((p->usyscall = (struct usyscall*)kalloc()) == 0) {
	freeproc(p);
	release(&p->lock);
	return 0;
}
```
在分配完物理地址后，就能对该地址进行处理了，此时就写入``pid``：
```c
// 为usyscall分配内存
...

p->usyscall->pid = p->pid;
```

在``proc_pagetable``中，能看到该函数为进程创建了一个空页表和诸多内存映射：
```c
if(mappages(pagetable, TRAMPOLINE, PGSIZE,
		  (uint64)trampoline, PTE_R | PTE_X) < 0){
	uvmfree(pagetable, 0);
	return 0;
}

// map the trapframe page just below the trampoline page, for
// trampoline.S.
if(mappages(pagetable, TRAPFRAME, PGSIZE,
		  (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
	uvmunmap(pagetable, TRAMPOLINE, 1, 0);
	uvmfree(pagetable, 0);
	return 0;
}
```
能看到这两个函数有个特点：
- 若是某一个虚拟地址映射失败，就需要对以映射的部分解除映射（实际试了下，测试点不够严谨，不解除映射也是可以通过所有测试的，因为测试用例只存在地址分配和映射都成功的情况。我又看了下``freewalk``，该函数强调了：所有的叶子页表项必须要先解除映射）
- 使用``uvmfree``释放页表

因此添加对``USYSCALL``的映射，使用在进程结构中已经记录了的``usyscall``：
```c
if(mappages(pagetable, USYSCALL, PGSIZE, (uint64)(p->usyscall), PTE_R | PTE_U) < 0){
	// 映射完成后，我们访问 USYSCALL 开始的页，就会访问到 p->usyscall
	uvmunmap(pagetable, TRAMPOLINE, 1, 0);
	uvmunmap(pagetable, TRAPFRAME, 1, 0);
	uvmfree(pagetable, 0);
	return 0;
}
```

现在关于映射就做完了，但是任务还没结束，还要对映射进行解除和释放物理内存，否则会因为内存泄漏无法通过内存检测（我就在这里被卡了很久）。

首先在``proc_freepagetable``销毁页表的时候取消映射：
```c
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, USYSCALL, 1, 0); // add 
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}
```

在销毁进程的时候释放内存，并将指针置空：
```c 
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  if(p->usyscall) // add
    kfree((void*)p->usyscall);
  
  p->usyscall = 0; // add
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}
```

### Print a page table
直接在``kernel/vm.c``添加函数：
```c
void helper(pagetable_t pt, int level) {
  if(level > 3) 
    return;
  // printf("debug\n");

  // printf("level %d\n", level);
  if(level == 1) {
    printf("page table %p\n", pt);
  }
  
  for(int i = 0; i < 512; i++) {
    // printf("access %d\n", i);
    pte_t pte = (pte_t)pt[i];
    if(pte & PTE_V){
      for(int j = 0; j < level; j++) {
        printf(" ..");
      }
      printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));
    }
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
      helper((pagetable_t)PTE2PA(pte), level + 1);
    }
  }
}

void vmprint(pagetable_t pt) {
  // 
  helper(pt, 1);
}
```

然后在``kernel/exec.c``中添加对该函数的调用：
```c
if(p->pid == 1) {
	vmprint(p->pagetable);
}
```
<font color="red"><b>调用函数这部分在return之前调用就行了，太早调用会少页，我不知道为什么……</b></font>

### Detect which pages have been accessed
还好，这部分不需要再手动添加syscall原型了，提前帮你写好了。这个题目说是hard，但是做起来感觉还是十分轻松的（我觉得题目有点难看懂），直接添加函数就好了：
```c
#define PTE_A (1L << 6) // 1 -> accessed

int pgaccess(pagetable_t pagetable,uint64 start_va, int page_num, uint64 result_va)
{
  int max_pg = 64;

  if(page_num > max_pg)
    page_num = max_pg;

  uint64 result_mask = 0;
  uint64 va = start_va;
  for(int i = 0; i < max_pg; i++) {
    pte_t *pte = walk(pagetable, va, 0);
    if(*pte & PTE_A) {
      result_mask |= (1 << i);
      *pte &= ~PTE_A;
    }
    
    va += PGSIZE;
  }
  copyout(pagetable, result_va, (char*)&result_mask, sizeof(result_mask));
  return 0;
}
int
sys_pgaccess(void)
{
  uint64 st_va;
  int npg;
  uint64 abits;
  argaddr(0, &st_va);
  argint(1, &npg);
  argaddr(2, &abits);

  return pgaccess(myproc()->pagetable, st_va, npg, abits);
}
```
唯一要注意的地方就是记得将``PTE_A``清零，但是这一点也是在hints中说了的。

关于PTE_A的位置，在之前[[#walk]]中的图片中有。