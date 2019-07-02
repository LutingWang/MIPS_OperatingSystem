本项目为 2019 年北航操作系统实验代码，仅包含最后一次 commit 的信息。代码向前兼容历次实验，但不排除有 bug 的可能。以下为挑战性任务实验报告

# Lab4-challenge 实验报告

## 实验目标

按照 POSIX 标准实现 pthread 库和 semaphore 库的部分功能。

### 线程

要求实现全用户地址空间共享，线程之间可以互相访问栈内数据。可以保留少量仅限于本线程可以访问的空间用以保存线程相关数据。

#### pthread_create

```c
/* create a new thread
 * pre-condition:
 *     thread - out arg of new thread id
 *     attr - attributes of the new thread
 *     start_routine - function of the new thread
 *     arg - argument to be passed in start_routine
 * post-condition:
 *     On success, return 0; else, return error number
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
```

#### pthread_exit

```c
/* terminate calling thread
 * pre-condition:
 *     retval - return value of calling thread
 */
void pthread_exit(void *retval);
```

#### pthread_cancel

```c
/* send a cancellation request to a thread
 * pre-condition:
 *     thread - target thread to be cancelled
 * post-condition:
 *     On success, return 0; else, return error number
 */
int pthread_cancel(pthread_t thread);
```

#### pthread_join

```c
/* join with a terminated thread
 * pre-condition:
 *     thread - id of the target thread caller is waiting for
 *     retval - out arg for return value of target thread
 * post-condition:
 *     On success, return 0; else, return error number
 */
int pthread_join(pthread_t thread, void **retval);
```

### 信号量

信号量的相关要求在理论课上已经涉及，此处不再赘述。

## 总体思路

本实验中难点在于如何基于进程实现线程。理论课上提到，进程是资源分配的单位、线程是调度单位。但是在我们的实验中，已经将资源分配和调度在同一个数据结构下实现，也就是`struct Env`。因此，我的思路是将`struct Env`同时作为进程和线程的数据结构。用面向对象的语言解释就是`Env`结构体的成员变量在不同环境下具有不同的可见度。这样我就只需要在线程涉及的地方加入代码，而不需要大动以往代码，避免引发错误。

而对于另外一种思路，也就是拆分`struct Env`为进程控制块和线程控制块。我进行过尝试，但是由于以下原因放弃了

1. 代码改动量太大，而且很多情况下只是将数据类型更改，功效比低下。
2. 很大一部分程序是单线程的，从这些程序中可以看出进程与线程之间有一定联系，将其拆分未必有益。

因此，我做了如下解读

1. 进程与线程本质上是相同的，不妨均称其为线程。
2. 进程是一种特殊的线程，也可以称为主线程。
3. 主线程只能由`init`或`fork`产生，其他线程只能由唯一的主线程`sfork`产生。
4. 当涉及到资源分配时，线程族（同一主线程的子线程）之间进行共享；否则，线程独立运行，互不干扰。

这样，就可以比较明确的使用`struct Env`同时作为进程控制块和线程控制块了。

而对于进程模拟线程一说，我的想法是，线程本身就是在进程这一概念之上提出的，因此二者有许多关联。虽然将其拆分会带来很多优点，但是统一起来未必不能实现。

## 代码实现

### 地址空间

```c
#define UTHREAD (UTEXT-BY2PG*2)
#define USEM (UTEXT-BY2PG*3)
```

- **UTHREAD** 线程专属地址，在进程间不共享。
- **USEM** 共享信号量地址，在`fork`产生的进程族之间共享。

### 数据结构

#### 信号量

```c
typedef struct {
	int count;
	struct Env_list queue;
	void* shared;
} sem_t;
```

- **count** 当值为正时，表示剩余资源数；否则表示阻塞进程数。
- **queue** 阻塞进程队列。
- **shared** 共享信号量的实际地址。

当`shared`非`NULL`时，表示该信号量为共享信号量。此时`shared`指向的地址是`USEM`表示的页中某处，被指向的信号量可以被`fork`产生的进程族共享。

#### 线程控制块

```c
typedef u_int pthread_t;

struct Env {
    // ...
	struct Env* tcb_super;
	struct Env* tcb_children[TCB2ENV];
	u_int tcb_cnum;
	LIST_ENTRY(Env) env_blocked_link; // for sem queue
	void* retval;
	int dead;
};
```

- **TCB2ENV** 单个进程最大线程数，本次设置为 $16$。
- **tcb_super** 进程控制块索引。为`NULL`表示该线程为进程。
- **tcb_children** 线程控制块索引，当`tcb_super != NULL`时有效。
- **tcb_cnum** 子线程个数。
- **env_blocked_link** 阻塞队列索引，用于信号量。
- **retval** 线程返回值，当`dead == 1`时有效。
- **dead** 线程运行结束标志位。

### 系统调用

#### 信号量

为了保证操作的原子性，信号量的相关操作全部用系统调用实现。

```c
int sys_sem_init(int sysno, sem_t *s, u_int value, int shared);
int sys_sem_destroy(int sysno, sem_t *s);
int sys_sem_wait(int sysno, sem_t *s);
int sys_sem_trywait(int sysno, sem_t *s);
int sys_sem_post(int sysno, sem_t *s);
int sys_sem_getvalue(int sysno, sem_t *s, int *sval);
```

其中，`sem_destroy`的规格描述不够清晰，我的行为描述如下：

>如果执行`sem_destroy`时阻塞队列非空，则将其全部释放。否则直接初始化为 $0$。

#### 进程控制

进程控制的大部分系统调用是在原函数的基础上扩充实现的，因此从函数原型上看不出差别。这里选择几个有函数声明的用户态系统调用接口进行说明。

##### 进程/线程号

```c
u_int syscall_getenvid(void);
u_int syscall_getthreadid(void);
```

`Linux`开发人员文档中强调：在旧版的`POSIX`接口实现中，同族线程的进程号不一（这是利用进程模拟线程的常见问题之一），导致了许多错误。为了解决这一问题，我将进程/线程号增加了`getter`方法，确保用于在正确使用的情况下可以获得正确的进程/线程号。由于实验模拟微内核操作系统，许多用户态函数需要访问`env_id`，因此不能做到完全封装，需要用户进行判断。

由于没有资料明确给出线程号的生成规则，我以线程号在全局内不重复为目标，定义了以下规则

1. 线程号的高 $28$ 位与进程号的低 $28$ 位相同。
2. 线程号的低 $4$ 位在统一进程内不重。

##### 进程撤销

`POSIX`接口中提供了撤销线程的接口，但是没有给出撤销进程的接口。因此我对`syscall_env_destroy`这一系统调用进行更改，使其只接受进程`envid`参数。这样，在撤销进程时，会同时撤销进程的所有线程。这一接口作为系统调用存在，但是并不建议在多线程程序中使用。

```c
int syscall_env_destroy(u_int envid);
```

### 特性

以下结合代码对实验中实现的一些特性进行说明。

#### 线程机制

进程的来源有`init`和`fork`。前者为文件加载而来，后者为复制而来。但线程与他们都不一样，由函数而来。换句话说，线程相当于一个`test`段被缩减的子进程。这样看来`sfork`和`fork`的关系就显而易见了，我们只需要把`fork`分配出来的进程稍加修饰，就成为了一个线程。这里的修饰包括

```c
echild->tcb_super = env;
echild->env_tf.regs[29] = stack;
echild->env_tf.pc = wrapper;
echild->env_tf.regs[4] = routine;
echild->env_tf.regs[5] = arg;
echild->env_tf.regs[6] = newenvid;
```

##### 设置线程栈

由于所有线程共享同一片地址空间，他们的栈必须被合理安排。`struct Env`中，记录着子线程的寄存器现场，自然也就包括了栈帧。因此，上述代码将栈帧设置为传入参数`stack`就可以重定位子线程的栈顶了。`stack`的定义如下

```c
u_int stack = USTACKTOP - PDMAP * (env->tcb_cnum + 1);
```

可以看出，我为每个线程预留了`PDMAP`大小的栈空间，所有线程按初始化顺序从`USTACKTOP`向下排列。

##### 设置函数体

上述代码中`3-6`行用于设置线程内容。其中最为核心的是`wrapper`参数。实际上，`wrapper`是一个固定的函数指针，指向一个自定义函数

```c
static void thread_wrapper(void *(*start_routine)(void *), void *arg, u_int envid) {
    void *retval;
    *thread = envs + ENVX(envid);
    retval = (*start_routine)(arg);
    pthread_exit(retval);
}
```

该函数以回调函数`start_routine`为参数，可以包装任何用户态函数。当函数执行完后，将接着执行`pthread_exit`以退出线程。这与开发人员文档的解释是一致的：正常情况下线程退出相当于执行了`pthread_exit`。

值得注意的是，主线程并没有被`wrapper`包含，因此在很多涉及线程撤销的场景下需要特判。

##### 设置线程树

在`sfork`中，将子线程的`tcb_super`设置为当前进程的指针。尽管不这样做也可以通过`parent_id`获得主线程控制块，但是由于

1. 进程/线程`id`具有二义性，尽量不适用
2. 精简代码

此处选择了增加索引。另外，在`wrapper`中设置了`*thread`，这一变量的作用是指向线程的控制块。由于线程控制块在线程间不共享，必须单独开辟一块空间进行存储

```c
struct Env **thread = (struct Env **) UTHREAD;

void libmain(int argc, char **argv) {
    // ...
    syscall_mem_alloc(0, UTHREAD, PTE_V|PTE_R|PTE_COW);
    *thread = env;
	// ...
}
```

这段空间在`UTEXT`之下，不会被`libpage`映射。同时这一页在进程中以写时复制的权限被映射，每个写入该地址的线程都会获得一个属于自己的页，从而避免了错误。

#### 进程地址空间共享

共享的实现是进程模拟线程的一大难点。我的解决方案是将所有线程用到的页全部以`PTE_LIBRARY`映射到进程中。可以说这样保证了进程可以管理全部资源，实际上是所有线程使用同一份资源。为了实现这一目标，我在两处进行了更改

##### 进程创建

```c
// sfork
for (i = PDMAP; i < USTACKTOP; i += BY2PG) {
    if ((*vpd)[PDX(i)] == 0) {
        i -= BY2PG;
        i += PDMAP;
        continue;
    }
    libpage(newenvid, i / BY2PG);
}
```

有别于创建进程，这里将`duppage`换成了`libpage`。

##### 缺页中断

如果程序执行过程中进行了分配页的操作，这一页同样需要被映射到进程中。处理一般缺页中断的函数是

```c
void pageout(int va, int context) {
	// ...
	p->pp_ref++;
	printf("pageout:t@@@___0x%x___@@@  ins a page n", va);
	va = VA2PFN(va);
	page_insert((Pde*)context, p, va, PTE_R);
	if (va > UTEXT) {
		env_libpage(va);
	}
}
```

函数首先对页面进行检查，这部分不需要更改，因而没有出现在上面。

主要的更改处为`env_libpage`，是一个自定义函数

```c
void env_libpage(u_int va) {
    if (curenv->tcb_super == NULL && curenv->tcb_cnum == 0) {
        return;
    }
    struct Env *e;
    int i;
    if (curenv->tcb_super == NULL) {
        e = curenv;
    } else {
        e = curenv->tcb_super;
    }
    mem_map(e, va);
    for (i = 0; i < e->tcb_cnum; i++) {
        mem_map(e->tcb_children[i], va);
    }
}
```

函数中遍历了所有线程，将`va`对应的页进行映射。

#### 有名信号量

实验中要求实现无名信号量，由于同一进程下，线程共享地址空间，这部分是容易实现的。而`POSIX`接口中还提供了有名信号量

```c
int sys_sem_init(int sysno, sem_t * s, u_int value, int shared) {
	sem_t* sem;
	sem_t* sems = (sem_t*)USEM;
	if (shared) {
		sem = sems->shared++;
		s->shared = sem;
	}
	else {
		sem = s;
		s->shared = NULL;
	}
	sem->count = value;
	LIST_INIT(&sem->queue);
	return 0;
}
```

信号量在创建时就确定了其可见性，分别保存在不同区域。要做到进程间共享，可以参考上述线程控制块独享的方法。也就是在加载进程的时候分配一页作为共享页，存储信号量数组

```c
syscall_mem_alloc(0, USEM, PTE_V|PTE_R|PTE_LIBRARY);
```

页面上的第一个信号量作为索引，用于记录信号量数组的长度

```c
sem_t *sems = (sem_t *) USEM;
sems->shared = sems + 1;
```

由此，每次分配信号量时，可以首先查看`USEM`地址处的信号量，从而获得一个空的信号量指针。

在使用时，同样需要将该页映射到所有进程中。`fork`原生支持对于`PTE_LIBRARY`页面的映射，因此重点还是落在`sfork`。方法与前文类似，不再介绍。

#### 多进程与多线程协作

指导书中曾经写过，`fork`是不改变共享页的权限的，因此在对多线程程序使用`fork`时会产生错误。这就是说，我们不能在`pthread_init`之后使用`fork`，但是我们可以在`fork`得到的进程内创建线程。由于`PTE_COW`和`PTE_LIBRARY`不能共存，因此在创建线程时需要先消除写时复制。这就涉及到前述的`libpage`权限检查

```c
if ((perm & PTE_COW) != 0) {
    perm -= PTE_COW;
    syscall_mem_alloc(pid, tmp, PTE_V | PTE_R | perm);
    user_bcopy((void*)addr, (void*)tmp, BY2PG);
    syscall_mem_map(pid, tmp, pid, addr, PTE_V | PTE_R | perm);
    syscall_mem_unmap(pid, tmp);
}
if ((perm & PTE_LIBRARY) == 0) {
    perm += PTE_LIBRARY;
    syscall_mem_map(pid, addr, pid, addr, perm);
}
syscall_mem_map(pid, addr, envid, addr, perm);
```

检查`COW`的作用是兼容`fork`，即如果当前指令位于`fork`得到的子进程中，需要先对页面进行复制，从而消除写时复制。检查共享权限的作用是判断当前进程是不是第一次创建子线程，如果是，就需要将其页面全部标为共享。

### 测试样例

为了方便起见，我将多个测试程序放在一个函数中进行调用

```c
#define TEST(name) \
    do { \
        if (fork() == 0) { \
            writef("\n--------------------------------------------------------------------------------\ntest " #name " begin\n"); \
            test_##name(); \
            writef("\ntest " #name " passedn^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"); \
            sem_post(&s); \
            return; \
        } \
        sem_wait(&s); \
    } while (0)

void umain(void) {
	sem_t s;
	sem_init(&s, 1, 0);
	TEST(pthread_create);
	TEST(pthread_join);
	TEST(pthread_exit);
	TEST(pthread_cancel);
	TEST(sem_trywait);
	TEST(sem_destroy);
	TEST(shared_stack);
	TEST(id);
	writef("\n\n########################################################################\ntest_umain is over\n\n");
}
```

该函数为进程主函数，他不断申请进程进行测试。实际上，这个函数还测试了信号量跨进程使用的能力。

下面对其进行解释

#### pthread_create

```c
#define MAGIC ((void *) 123)
#define MAGIC2 ((void *) 246)

static void* routine_pthread_create1(void* arg) {
	user_assert(arg == MAGIC);
	return NULL;
}

static void* routine_pthread_create2(void* arg) {
	user_assert(arg == MAGIC2);
	return NULL;
}

static void test_pthread_create(void) {
	pthread_t th1, th2;
	pthread_create(&th1, NULL, routine_pthread_create1, MAGIC);
	pthread_create(&th2, NULL, routine_pthread_create2, MAGIC2);
	user_assert(th1 == 1);
	user_assert(th2 == 2);
	return;
}

#undef MAGIC
#undef MAGIC2
```

- **测试内容** 线程号与传参
- **现象** 检查通过

#### pthread_join

```c
#define MAGIC ((void *) 456)

static void* routine_pthread_join(void* arg) {
	return MAGIC;
}

static void test_pthread_join(void) {
	pthread_t th;
	void* retval;
	pthread_create(&th, NULL, routine_pthread_join, NULL);
	pthread_join(th, &retval);
	user_assert(retval == (void*)MAGIC);
	return;
}

#undef MAGIC
```

- **测试内容** 线程返回值
- **现象** 检查通过

#### pthread_exit

```c
#define MAGIC ((void *) 789)

static void* routine_pthread_exit(void* arg) {
	pthread_exit((void*)MAGIC);
	return NULL; // never executed
}

static void test_pthread_exit(void) {
	pthread_t th;
	void* retval;
	pthread_create(&th, NULL, routine_pthread_exit, NULL);
	pthread_join(th, &retval);
	user_assert(retval == (void*)MAGIC);
	return;
}

#undef MAGIC
```

- **测试内容** `exit`与`return`效果是否一致
- **现象** 检查通过

#### pthread_cancel

```c
static void* routine_pthread_cancel(void* arg) {
	int i;
	writef("nroutine: ");
	for (i = 0; i < 500; i++) {
		writef("%d ", i);
	}
	return NULL;
}

static void test_pthread_cancel(void) {
	pthread_t th;
	pthread_create(&th, NULL, routine_pthread_cancel, NULL);

	// to hold some time
	int i;
	writef("ntest_main: ");
	for (i = 1000; i < 1050; i++) {
		writef("%d ", i);
	}

	pthread_cancel(th);
	return;
}
```

- **测试内容** 线程执行中断
- **现象** 子线程打印数字，但并未打印到 $500$

#### sem_trywait

```c
static void* routine_sem_trywait1(void* arg) {
	sem_t* s = (sem_t*)arg;
	int c;
	sem_getvalue(s, &c);
	user_assert(c == 0);
	user_assert(sem_trywait(s) == -1);
	sem_getvalue(s, &c);
	user_assert(c == 0);
	sem_post(s + 1);
	return NULL;
}

static void* routine_sem_trywait2(void* arg) {
	sem_t* s = (sem_t*)arg;
	int c;
	sem_getvalue(s, &c);
	user_assert(c == 0);
	sem_post(s);
	sem_getvalue(s, &c);
	user_assert(c == 1);
	user_assert(sem_trywait(s) == 0);
	sem_getvalue(s, &c);
	user_assert(c == 0);
	return NULL;
}

void test_sem_trywait(void) {
	sem_t s[2];
	pthread_t th1, th2;
	sem_init(s, 0, 0);
	sem_init(s + 1, 0, 0);
	pthread_create(&th1, NULL, routine_sem_trywait1, s);
	pthread_create(&th2, NULL, routine_sem_trywait2, s);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
	return;
}
```

- **测试内容** 函数返回值正确
- **现象** 检查通过

#### sem_destroy

```c
static void* routine_sem_destroy1(void* arg) {
	sem_t* s = (sem_t*)arg;
	int c;
	sem_getvalue(s, &c);
	user_assert(c <= 0);
	sem_wait(s);
	writef("routine1: wakenn");
	return NULL;
}

static void* routine_sem_destroy2(void* arg) {
	sem_t* s = (sem_t*)arg;
	int c;
	sem_getvalue(s, &c);
	user_assert(c <= 0);
	sem_wait(s);
	writef("routine2: wakenn");
	return NULL;
}

void test_sem_destroy(void) {
	sem_t s[2];
	pthread_t th1, th2;
	int c;
	sem_init(s, 0, 0);
	sem_init(s + 1, 0, 0);
	pthread_create(&th1, NULL, routine_sem_destroy1, &s);
	pthread_create(&th2, NULL, routine_sem_destroy2, &s);
	for (c = 0; c < 100; c++) {
		writef("%d ", c);
	}
	sem_getvalue(s, &c);
	user_assert(c == -2);
	sem_destroy(s);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
	return;
}
```

- **测试内容** 阻塞线程在信号量销毁后恢复运行
- **现象** 检查通过且两个线程打印内容无误

#### 栈共享

```c
static void* routine_shared_stack1(void* arg) {
	int i = 200;
	int* a = (int*)0x7ebfdfd0;
	sem_t* s = (sem_t*)arg;
	sem_post(s);
	sem_wait(s + 1);
	writef("routine1: i=%d, &i=%x, a=%x, *a=%dn", i, &i, a, *a);
	sem_post(s);
	sem_wait(s + 1);
	sem_post(s + 2);
	return NULL;
}

static void* routine_shared_stack2(void* arg) {
	int i = 100;
	int* a = (int*)0x7effdfd0;
	sem_t* s = (sem_t*)arg;
	sem_post(s + 1);
	sem_wait(s);
	writef("routine2: i=%d, &i=%x, a=%x, *a=%dn", i, &i, a, *a);
	sem_post(s + 1);
	sem_wait(s);
	sem_post(s + 2);
	return NULL;
}

static void test_shared_stack(void) {
	sem_t s[3];
	sem_init(s, 0, 0);
	sem_init(s + 1, 0, 0);
	sem_init(s + 2, 0, 0);
	pthread_t th1, th2;
	pthread_create(&th1, NULL, routine_shared_stack1, s);
	pthread_create(&th2, NULL, routine_shared_stack2, s);
	sem_wait(s + 2);
	return;
}
```

- **测试内容** 线程间可以通过指针直接访问对应变量
- **现象** 两个线程打印的`i`与`*a`恰好互换

#### 进程/线程号

```c
static void* routine_id1(void* arg) {
	writef("routine1: env id is %xn", syscall_getenvid());
	writef("routine1: thread id is %xn", syscall_getthreadid());
	return NULL;
}

static void* routine_id2(void* arg) {
	writef("routine2: env id is %xn", syscall_getenvid());
	writef("routine2: thread id is %xn", syscall_getthreadid());
	return NULL;
}

static void test_id(void) {
	pthread_t th1, th2;
	pthread_create(&th1, NULL, routine_id1, NULL);
	pthread_create(&th2, NULL, routine_id2, NULL);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
	writef("test_main: env id is %xn", syscall_getenvid());
	writef("test_main: thread id is %xn", syscall_getthreadid());
	return;
}
```

- **测试内容** 进程/线程号被封装
- **现象** 打印结果符合要求

## 任务列表

```
/
|---include
	|---env.h
		|---struct Env
    |---mmn.h
    	|---UTHREAD
    	|---USEM
    |---pthread.h
    	|---*
    |---semaphore.h
    	|---*
    |---unistd.h
    	|---SYS_sem_*
|---lib
	|---env.c
		|---env_libpage
	|---syscall_all.c
		|---sys_sem_*
		|---sys_getenvid
		|---*
|---mm
	|---pmap.c
		|---pageout
|---user
	|---fork.c
		|---libpage
		|---sfork
	|---lib.h
	|---libos.c
	|---pthread.c
	|---semaphore.c
	|---syscall_lib.c
	|---testsem.c
```

