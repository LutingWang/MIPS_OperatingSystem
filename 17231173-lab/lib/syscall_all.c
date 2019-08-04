#include "../drivers/gxconsole/dev_cons.h"
#include <mmu.h>
#include <env.h>
#include <printf.h>
#include <pmap.h>
#include <sched.h>
#include <semaphore.h>

// #define DPOSIX

extern char *KERNEL_SP;
extern struct Env *curenv;

/* Overview:
 *      This function is used to print a character on screen.
 *
 * Pre-Condition:
 *      `c` is the character you want to print.
 */
void sys_putchar(int sysno, int c, int a2, int a3, int a4, int a5) {
        printcharc((char) c);
        return ;
}

/* Overview:
 *      This function enables you to copy content of `srcaddr` to `destaddr`.
 *
 * Pre-Condition:
 *      `destaddr` and `srcaddr` can't be NULL. Also, the `srcaddr` area
 *      shouldn't overlap the `destaddr`, otherwise the behavior of this
 *      function is undefined.
 *
 * Post-Condition:
 *      the content of `destaddr` area(from `destaddr` to `destaddr`+`len`) will
 * be same as that of `srcaddr` area.
 */
void *memcpy(void *destaddr, void const *srcaddr, u_int len) {
        char *dest = destaddr;
        char const *src = srcaddr;

        while (len-- > 0) {
                *dest++ = *src++;
        }

        return destaddr;
}

/* Overview:
 *      This function provides the environment id of current process.
 *
 * Post-Condition:
 *      return the current environment id
 */
u_int sys_getenvid(int sysno, int thread) {
        if (curenv->tcb_super == NULL) {
                if (!thread) return curenv->env_id;
                else return (curenv->env_id) * TCB2ENV;
        }
        struct Env *super = curenv->tcb_super;
        if (!thread) {
                return super->env_id;
        }
        u_int i;
        for (i = 0; i < super->tcb_cnum; i++) {
                if (super->tcb_children[i]->env_id == curenv->env_id) {
                        break;
                }
        }
        u_int envid = (super->env_id) * TCB2ENV;
        return envid + i + 1;
}

/* Overview:
 *      This function enables the current process to give up CPU.
 *
 * Post-Condition:
 *      Deschedule current environment. This function will never return.
 */
void sys_yield(void) {
#ifdef DEBUG
        printf("sys_yield@syscall_all.c called\n");
#endif
        bcopy((void *)(KERNEL_SP - TF_SIZE), (void *)(TIMESTACK - TF_SIZE), TF_SIZE);
        sched_yield();
}

/* Overview:
 *      This function is used to destroy the current environment.
 *
 * Pre-Condition:
 *      The parameter `envid` must be the environment id of a
 * process, which is either a child of the caller of this function
 * or the caller itself.
 *
 * Post-Condition:
 *      Return 0 on success, < 0 when error occurs.
 */
int sys_env_destroy(int sysno, u_int envid) {
        /*
                printf("[%08x] exiting gracefully\n", curenv->env_id);
                env_destroy(curenv);
        */
        int r;
        struct Env *e;

        if ((r = envid2env(envid, &e, 1)) < 0) {
                return r;
        }

        printf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
        env_destroy(e);
        return 0;
}

/* Overview:
 *      Set envid's pagefault handler entry point and exception stack.
 *
 * Pre-Condition:
 *      xstacktop points one byte past exception stack.
 *
 * Post-Condition:
 *      The envid's pagefault handler will be set to `func` and its
 *      exception stack will be set to `xstacktop`.
 *      Returns 0 on success, < 0 on error.
 */
int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop) {
        // Your code here.
#ifdef DEBUG
        printf("sys_set_pgfault_handler@syscall_all.c called with (int sysno: %d, u_int envid: %x, u_int func, u_int xstacktop\n", sysno-9527, envid);
#endif
        struct Env *env;
        int ret;
        if ((ret = envid2env(envid, &env, 0))) {
                return ret;
        }
        env->env_pgfault_handler = func;
        env->env_xstacktop = xstacktop;

#ifdef DEBUG
        printf("sys_set_pgfault_handler@syscall_all.c: over\n");
#endif
        return 0;
        //      panic("sys_set_pgfault_handler not implemented");
}

/* Overview:
 *      Allocate a page of memory and map it at 'va' with permission
 * 'perm' in the address space of 'envid'.
 *
 *      If a page is already mapped at 'va', that page is unmapped as a
 * side-effect.
 *
 * Pre-Condition:
 * perm -- PTE_V is required,
 *         PTE_COW is not allowed(return -E_INVAL),
 *         other bits are optional.
 *
 * Post-Condition:
 * Return 0 on success, < 0 on error
 *      - va must be < UTOP
 *      - env may modify its own address space or the address space of its children
 */
int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm) {
#ifdef DEBUG
        printf("sys_mem_alloc@syscall_all.c called with (int sysno: %d, u_int envid: %x, u_int va: %x, u_int perm: %x)\n", sysno-9527, envid, va, perm);
#endif
        struct Env *env;
        struct Page *ppage;
        int ret;

        if (va >= UTOP) return -E_INVAL;
        if ((perm & PTE_V) == 0 || (perm & PTE_COW) != 0) return -E_INVAL;
        if ((ret = envid2env(envid, &env, 0))) {
#ifdef DEBUG
                printf("sys_mem_alloc@syscall_all.c: envid2env failed. Please check if argument checkperm if correctly passed in!\n");
                panic("^^^^^^^^^^^^^^^^^^^");
#endif
                return ret;
        }
        if ((ret = page_alloc(&ppage))) return ret;
        if ((ret = page_insert(env->env_pgdir, ppage, va, perm))) return ret;

#ifdef DEBUG
        printf("sys_mem_alloc@syscall_all.c: over\n");
#endif
        return ret;
}

/* Overview:
 *      Map the page of memory at 'srcva' in srcid's address space
 * at 'dstva' in dstid's address space with permission 'perm'.
 * Perm has the same restrictions as in sys_mem_alloc.
 * (Probably we should add a restriction that you can't go from
 * non-writable to writable?)
 *
 * Post-Condition:
 *      Return 0 on success, < 0 on error.
 *
 * Note:
 *      Cannot access pages above UTOP.
 */
int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva, u_int perm) {
#ifdef DEBUG
        printf("sys_mem_map@syscall_all.c called\n");
#endif
        int ret;
        struct Env *srcenv;
        struct Env *dstenv;
        struct Page *ppage;
        Pte *ppte = NULL; /* in case srcva doesn't map a page */
        u_int round_srcva = ROUNDDOWN(srcva, BY2PG);
        u_int round_dstva = ROUNDDOWN(dstva, BY2PG);

        ppage = NULL;
        ret = 0;

    //your code here
        if (srcva >= UTOP || dstva >= UTOP || (perm & PTE_V) == 0) {
#ifdef DEBUG
                printf("sys_mem_map@syscall_all.c: parameter check failed\n");
#endif
                return -E_INVAL;
        }

        if ((ret = envid2env(srcid, &srcenv, 0))) {
#ifdef DEBUG
                printf("sys_mem_map@syscall_all.c: envid2env failed for src. Please check if argument checkperm if correctly passed in!\n");
                panic("^^^^^^^^^^^^^^^^^^^");
#endif
                return ret;
        }
        if ((ret = envid2env(dstid, &dstenv, 0))) {
#ifdef DEBUG
                printf("sys_mem_map@syscall_all.c: envid2env failed for dst. Please check if argument checkperm if correctly passed in!\n");
                panic("^^^^^^^^^^^^^^^^^^^");
#endif
                return ret;
        }
        ppage = page_lookup(srcenv->env_pgdir, round_srcva, &ppte);
        if (ppage == NULL || ppte == NULL) return -E_INVAL;
        if ((ret = page_insert(dstenv->env_pgdir, ppage, round_dstva, perm))) return ret;

#ifdef DEBUG
        printf("sys_mem_map@syscall_all.c: over\n");
#endif
        return ret;
}

/* Overview:
 *      Unmap the page of memory at 'va' in the address space of 'envid'
 * (if no page is mapped, the function silently succeeds)
 *
 * Post-Condition:
 *      Return 0 on success, < 0 on error.
 *
 * Cannot unmap pages above UTOP.
 */
int sys_mem_unmap(int sysno, u_int envid, u_int va) {
#ifdef DEBUG
        printf("sys_mem_unmap@syscall_call.c called with (int sysno: %d, u_int envid: %x, u_int va: %x)\n", sysno, envid, va);
#endif
        int ret;
        struct Env *env;

        if (va >= UTOP) return -E_INVAL;
        if ((ret = envid2env(envid, &env, 0))) {
#ifdef DEBUG
                printf("sys_mem_unmap@syscall_all.c: envid2env failed for dst. Please check if argument checkperm if correctly passed in!\n");
                panic("^^^^^^^^^^^^^^^^^^^");
#endif
                return ret;
        }
        page_remove(env->env_pgdir, va);

        return ret;
        //      panic("sys_mem_unmap not implemented");
}

/* Overview:
 *      Allocate a new environment.
 *
 * Pre-Condition:
 * The new child is left as env_alloc created it, except that
 * status is set to ENV_NOT_RUNNABLE and the register set is copied
 * from the current environment.
 *
 * Post-Condition:
 *      In the child, the register set is tweaked so sys_env_alloc returns 0.
 *      Returns envid of new environment, or < 0 on error.
 */
int sys_env_alloc(void) {
#ifdef DEBUG
        printf("sys_env_alloc@syscall_all.c called\n");
#endif
        int r;
        struct Env *e;

        bcopy((void *)KERNEL_SP - TF_SIZE, &(curenv->env_tf), TF_SIZE);
        if ((r = env_alloc(&e, curenv->env_id))) return r;
        bcopy(&(curenv->env_tf), &(e->env_tf), TF_SIZE);
        e->env_status = ENV_NOT_RUNNABLE;
        e->env_pri = curenv->env_pri;
#ifdef DEBUG
        printf("sys_env_alloc@syscall_all.c: setting pri to %d\n", e->env_pri);
#endif
        e->env_tf.pc = e->env_tf.cp0_epc;
        e->env_tf.regs[2] = 0;

#ifdef DEBUG
        printf("sys_env_alloc@syscall_all.c: over\n");
#endif
        return e->env_id;
        //      panic("sys_env_alloc not implemented");
}

/* Overview:
 *      Set envid's env_status to status.
 *
 * Pre-Condition:
 *      status should be one of `ENV_RUNNABLE`, `ENV_NOT_RUNNABLE` and
 * `ENV_FREE`. Otherwise return -E_INVAL.
 *
 * Post-Condition:
 *      Returns 0 on success, < 0 on error.
 *      Return -E_INVAL if status is not a valid status for an environment.
 *      The status of environment will be set to `status` on success.
 */
int sys_set_env_status(int sysno, u_int envid, u_int status) {
        struct Env *env;
        int ret;

        if ((ret = envid2env(envid, &env, 0))) {
#ifdef DEBUG
                panic("SYS_set_env_status@syscall_all.c: envid2env failed. Please check if checkperm is correctly passed in\n");
#endif
                return ret;
        }
        if (status == ENV_RUNNABLE
                        || status == ENV_NOT_RUNNABLE
                        || status == ENV_FREE) {
                if (env->env_status == status) {
#ifdef DEBUG
                        printf("sys_set_env_status@syscall_all.c: resetting env status ");
                        switch (status) {
                                case ENV_RUNNABLE:
                                        printf("ENV_RUNNABLE\n");
                                        break;
                                case ENV_NOT_RUNNABLE:
                                        printf("ENV_NOT_RUNNABLE\n");
                                        break;
                                case ENV_FREE:
                                        printf("ENV_FREE\n");
                                        break;
                        }
#endif
                }
                env->env_status = status;
                if (status == ENV_RUNNABLE) {
                        LIST_INSERT_HEAD(env_sched_list, env, env_sched_link);
                } else if (status == ENV_FREE) {
                        panic("sys_set_env_status@syscall_all.c: setting env status to free\n");
                }
        } else {
                return -E_INVAL;
        }

        return 0;
        //      panic("sys_env_set_status not implemented");
}

/* Overview:
 *      Set envid's trap frame to tf.
 *
 * Pre-Condition:
 *      `tf` should be valid.
 *
 * Post-Condition:
 *      Returns 0 on success, < 0 on error.
 *      Return -E_INVAL if the environment cannot be manipulated.
 *
 * Note: This hasn't be used now?
 */
int sys_set_trapframe(int sysno, u_int envid, struct Trapframe *tf) {
        return 0;
}

/* Overview:
 *      Kernel panic with message `msg`.
 *
 * Pre-Condition:
 *      msg can't be NULL
 *
 * Post-Condition:
 *      This function will make the whole system stop.
 */
void sys_panic(int sysno, char *msg) {
        // no page_fault_mode -- we are trying to panic!
        panic("%s", TRUP(msg));
}

/* Overview:
 *      This function enables caller to receive message from
 * other process. To be more specific, it will flag
 * the current process so that other process could send
 * message to it.
 *
 * Pre-Condition:
 *      `dstva` is valid (Note: NULL is also a valid value for `dstva`).
 *
 * Post-Condition:
 *      This syscall will set the current process's status to
 * ENV_NOT_RUNNABLE, giving up cpu.
 */
void sys_ipc_recv(int sysno, u_int dstva) {
        if (dstva >= UTOP) return;
        curenv->env_ipc_recving = 1;
        curenv->env_ipc_dstva = dstva;
        curenv->env_status = ENV_NOT_RUNNABLE;
        sys_yield();
}

/* Overview:
 *      Try to send 'value' to the target env 'envid'.
 *
 *      The send fails with a return value of -E_IPC_NOT_RECV if the
 * target has not requested IPC with sys_ipc_recv.
 *      Otherwise, the send succeeds, and the target's ipc fields are
 * updated as follows:
 *    env_ipc_recving is set to 0 to block future sends
 *    env_ipc_from is set to the sending envid
 *    env_ipc_value is set to the 'value' parameter
 *      The target environment is marked runnable again.
 *
 * Post-Condition:
 *      Return 0 on success, < 0 on error.
 *
 * Hint: the only function you need to call is envid2env.
 */
int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva, u_int perm) {
        int r;
        struct Env *e;
        // struct Page *p;

        if (srcva >= UTOP) return -E_INVAL;
        if ((r = envid2env(envid, &e, 0))) {
#ifdef DEBUG
                panic("sys_ipc_can_send@syscall_all.c: envid2env failed for dst. Please check if argument checkperm if correctly passed in!\n");
#endif
                return r;
        }
        if (e->env_ipc_recving == 0) return -E_IPC_NOT_RECV;
        e->env_ipc_recving = 0;
        e->env_ipc_from = curenv->env_id;
        e->env_ipc_value = value;
        if (srcva) {
                if ((r = sys_mem_map(sysno, curenv->env_id, srcva,
                                                e->env_id, e->env_ipc_dstva, perm))) return r;
                e->env_ipc_perm = perm;
        }
        e->env_status = ENV_RUNNABLE;
        LIST_INSERT_HEAD(env_sched_list, e, env_sched_link);

        return 0;
}

/* Overview:
 *      This function is used to write data to device, which is
 *      represented by its mapped physical address.
 *      Remember to check the validity of device address (see Hint below);
 *
 * Pre-Condition:
 *      'va' is the startting address of source data, 'len' is the
 *      length of data (in bytes), 'dev' is the physical address of
 *      the device
 *
 * Post-Condition:
 *      copy data from 'va' to 'dev' with length 'len'
 *      Return 0 on success.
 *      Return -E_INVAL on address error.
 *
 * Hint: Use ummapped segment in kernel address space to perform MMIO.
 *       Physical device address:
 *      * ---------------------------------*
 *      |   device   | start addr | length |
 *      * -----------+------------+--------*
 *      |  console   | 0x10000000 | 0x20   |
 *      |    IDE     | 0x13000000 | 0x4200 |
 *      |    rtc     | 0x15000000 | 0x200  |
 *      * ---------------------------------*
 */
int sys_write_dev(int sysno, u_int va, u_int dev, u_int len) {
        if ((0x10000000 <= dev && dev + len <= 0x10000020)
                        || (0x13000000 <= dev && dev + len <= 0x13004200)
                        || (0x15000000 <= dev && dev + len <= 0x15000200)) {
                bcopy((void *)va, (void *)dev + 0xA0000000, len);
                return 0;
        } else {
                return -E_INVAL;
        }
}

/* Overview:
 *      This function is used to read data from device, which is
 *      represented by its mapped physical address.
 *      Remember to check the validity of device address (same as sys_read_dev)
 *
 * Pre-Condition:
 *      'va' is the startting address of data buffer, 'len' is the
 *      length of data (in bytes), 'dev' is the physical address of
 *      the device
 *
 * Post-Condition:
 *      copy data from 'dev' to 'va' with length 'len'
 *      Return 0 on success, < 0 on error
 *
 * Hint: Use ummapped segment in kernel address space to perform MMIO.
 */
int sys_read_dev(int sysno, u_int va, u_int dev, u_int len) {
        if ((0x10000000 <= dev && dev + len <= 0x10000020)
                        || (0x13000000 <= dev && dev + len <= 0x13004200)
                        || (0x15000000 <= dev && dev + len <= 0x15000200)) {
                bcopy((void *)dev + 0xA0000000, (void *)va, len);
                return 0;
        } else {
                return -E_INVAL;
        }
}

int sys_sem_init(int sysno, sem_t *s, u_int value, int shared) {
#ifdef DPOSIX
        printf("sys_sem_init@syscall_all.c called with (sem_t *s: %x, u_int value: %x, int shared: %d)\n", s, value, shared);
#endif
        sem_t *sem;
        sem_t *sems = (sem_t *) USEM;
        if (shared) {
                sem = sems->shared++;
                s->shared = sem;
        } else {
                sem = s;
                s->shared = NULL;
        }
        sem->count = value;
        LIST_INIT(&sem->queue);
        return 0;
}

int sys_sem_destroy(int sysno, sem_t *s) {
#ifdef DPOSIX
        printf("sys_sem_destroy@syscall_all.c called with (sem_t *s: %x)\n", s);
#endif
        sem_t *sem = s;
        if (s->shared != NULL) {
                sem = (sem_t *) s->shared;
        }
        sem->count = 0;
        struct Env *e;
        while (!LIST_EMPTY(&sem->queue)) {
                e = LIST_FIRST(&sem->queue);
                LIST_REMOVE(e, env_blocked_link);
                e->env_status = ENV_RUNNABLE;
                LIST_INSERT_HEAD(env_sched_list, e, env_sched_link);
        }
        return 0;
}

int sys_sem_wait(int sysno, sem_t *s) {
#ifdef DPOSIX
        printf("sys_sem_wait@syscall_all.c called with (sem_t *s: %x)\n", s);
#endif
        sem_t *sem = s;
        if (s->shared != NULL) {
                sem = (sem_t *) s->shared;
        }
#ifdef DPOSIX
        printf("sys_sem_wait@syscall_all.c: current value of sem is %x\n", sem->count);
#endif
        if (sem->count-- <= 0) {
                LIST_INSERT_HEAD(&sem->queue, curenv, env_blocked_link);
                curenv->env_status = ENV_NOT_RUNNABLE;
#ifdef DPOSIX
                printf("sys_sem_wait@syscall_all.c: turning into blocked status\n");
#endif
                return 1;
        }
        return 0;
}

int sys_sem_trywait(int sysno, sem_t *s) {
        sem_t *sem = s;
        if (s->shared != NULL) {
                sem = (sem_t *) s->shared;
        }
        if (sem->count <= 0) {
                return 1;
        } else {
                sem->count--;
                return 0;
        }
}

int sys_sem_post(int sysno, sem_t *s) {
#ifdef DPOSIX
        printf("sys_sem_post called with (sem_t *s: %x)\n", s);
#endif
        sem_t *sem = s;
        if (s->shared != NULL) {
                sem = (sem_t *) s->shared;
        }
#ifdef DPOSIX
        printf("sys_sem_post@syscall_all.c: current value of sem is %x\n", sem->count);
#endif
        struct Env *e;
        if (sem->count++ < 0) {
                e = LIST_FIRST(&sem->queue);
                LIST_REMOVE(e, env_blocked_link);
                e->env_status = ENV_RUNNABLE;
                LIST_INSERT_HEAD(env_sched_list, e, env_sched_link);
        }
        return 0;
}

int sys_sem_getvalue(int sysno, sem_t *s, int *sval) {
#ifdef DPOSIX
        printf("sys_sem_getvalue@syscall_all.c called with (sem_t *s: %x, int *sval: %x)\n", s, sval);
#endif
        sem_t *sem = s;
        if (s->shared != NULL) {
                sem = (sem_t *) s->shared;
        }
#ifdef DPOSIX
        printf("sys_sem_getvalue@syscall_all.c: current value of sem is %d\n", sem->count);
#endif
        *sval = sem->count;
        return 0;
}

