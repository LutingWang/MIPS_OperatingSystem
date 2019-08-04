// implement fork from user space

#include "lib.h"
#include <mmu.h>
#include <env.h>

/* ----------------- help functions ---------------- */

/* Overview:
 *      Copy `len` bytes from `src` to `dst`.
 *
 * Pre-Condition:
 *      `src` and `dst` can't be NULL. Also, the `src` area
 *       shouldn't overlap the `dest`, otherwise the behavior of this
 *       function is undefined.
 */
void user_bcopy(const void *src, void *dst, size_t len) {
        void *max;

        //      writef("~~~~~~~~~~~~~~~~ src:%x dst:%x len:%x\n",(int)src,(int)dst,len);
        max = dst + len;

        // copy machine words while possible
        if (((int)src % 4 == 0) && ((int)dst % 4 == 0)) {
                while (dst + 3 < max) {
                        *(int *)dst = *(int *)src;
                        dst += 4;
                        src += 4;
                }
        }

        // finish remaining 0-3 bytes
        while (dst < max) {
                *(char *)dst = *(char *)src;
                dst += 1;
                src += 1;
        }

        //for(;;);
}

/* Overview:
 *      Sets the first n bytes of the block of memory
 * pointed by `v` to zero.
 *
 * Pre-Condition:
 *      `v` must be valid.
 *
 * Post-Condition:
 *      the content of the space(from `v` to `v`+ n)
 * will be set to zero.
 */
void user_bzero(void *v, u_int n) {
        char *p; p = v;
        int m; m = n;
        while (--m >= 0) { *p++ = 0; }
}
/*--------------------------------------------------------------*/

/* Overview:
 *      Custom page fault handler - if faulting page is copy-on-write,
 * map in our own private writable copy.
 *
 * Pre-Condition:
 *      `va` is the address which leads to a TLBS exception.
 *
 * Post-Condition:
 *  Launch a user_panic if `va` is not a copy-on-write page.
 * Otherwise, this handler should map a private writable copy of
 * the faulting page at correct address.
 */
static void pgfault(u_int va) {
        u_int tmp = UTEXT - BY2PG; // arbitrary address
        va = ROUNDDOWN(va, BY2PG);
        //      writef("fork.c:pgfault():\t va:%x\n",va);
        if (((*vpt)[VPN(va)] & PTE_COW) == 0) {
                user_panic("pgfault@fork.c: page of va is not cow\n");
        }

        syscall_mem_alloc(0, tmp, PTE_V|PTE_R);
        user_bcopy((void *) va, (void *) tmp, BY2PG);
        syscall_mem_map(0, tmp, 0, va, PTE_V|PTE_R);
        syscall_mem_unmap(0, tmp);
}
//map the new page at a temporary place
//copy the content
//map the page on the appropriate place
//unmap the temporary place

/* Overview:
 *      Map our virtual page `pn` (address pn*BY2PG) into the target `envid`
 * at the same virtual address.
 *
 * Post-Condition:
 *  if the page is writable or copy-on-write, the new mapping must be
 * created copy on write and then our mapping must be marked
 * copy on write as well. In another word, both of the new mapping and
 * our mapping should be copy-on-write if the page is writable or
 * copy-on-write.
 *
 * Hint:
 *      PTE_LIBRARY indicates that the page is shared between processes.
 * A page with PTE_LIBRARY may have PTE_R at the same time. You
 * should process it correctly.
 */
static void duppage(u_int envid, u_int pn) {
        u_int addr = pn * BY2PG;
        u_int perm = (*vpt)[pn] & 0xfff;
        if ((perm & PTE_V) == 0) {
                return;
        }
#ifdef DEBUG
        writef("duppage@fork.c: perm for page %x in env %x is %x\n", pn, envid, perm);
#endif

        if ((perm & PTE_R) != 0 && (perm & PTE_LIBRARY) == 0) {
                perm = perm | PTE_COW;
                syscall_mem_map(0, addr, envid, addr, perm);
                syscall_mem_map(0, addr, 0, addr, perm);
        } else {
                syscall_mem_map(0, addr, envid, addr, perm);
        }

        //      user_panic("duppage not implemented");
}

static void libpage(u_int envid, u_int pn) {
        u_int addr = pn * BY2PG;
        u_int perm = (*vpt)[pn] & 0xfff;
        u_int pid = env->env_id;
        u_int tmp = UTEXT - BY2PG;
        if ((perm & PTE_V) == 0) {
                return;
        }
#ifdef DEBUG
        writef("libpage@fork.c: perm for page %x in env %x is %x\n", pn, pid, perm);
#endif
        if ((perm & PTE_COW) != 0) {
                perm -= PTE_COW;
                syscall_mem_alloc(pid, tmp, PTE_V|PTE_R|perm);
                user_bcopy((void *) addr, (void *) tmp, BY2PG);
                syscall_mem_map(pid, tmp, pid, addr, PTE_V|PTE_R|perm);
                syscall_mem_unmap(pid, tmp);
        }
        if ((perm & PTE_LIBRARY) == 0) {
                perm += PTE_LIBRARY;
                syscall_mem_map(pid, addr, pid, addr, perm);
        }
        syscall_mem_map(pid, addr, envid, addr, perm);
}

/* Overview:
 *      User-level fork. Create a child and then copy our address space
 * and page fault handler setup to the child.
 *
 * Hint: use vpd, vpt, and duppage.
 * Hint: remember to fix "env" in the child process!
 * Note: `set_pgfault_handler`(user/pgfault.c) is different from
 *       `syscall_set_pgfault_handler`.
 */
extern void __asm_pgfault_handler(void);
int fork(void) {
#ifdef DEBUG
        writef("fork@fork.c called\n");
#endif
        if (env->tcb_cnum != 0) {
                user_panic("fork@fork.c: trying to fork a multi-thread process\n");
        }
        u_int newenvid;
        extern struct Env *envs;
        extern struct Env *env;
        u_int i;

        //The parent installs pgfault using set_pgfault_handler
        set_pgfault_handler(pgfault);
#ifdef DEBUG
        writef("fork@fork.c: page fault handler set for father\n");
#endif

        //alloc a new env
        newenvid = syscall_env_alloc();
        if (newenvid == 0) {
#ifdef DEBUG
                writef("fork@fork.c: this is child\n");
#endif
                env = envs + ENVX(syscall_getenvid());
                *thread = env;
        } else {
#ifdef DEBUG
                writef("fork@fork.c: this is father\n");
#endif
                for (i = 0; i < USTACKTOP; i += BY2PG) {
                        if ((*vpd)[PDX(i)] == 0) {
                                i -= BY2PG;
                                i += PDMAP;
                                continue;
                        }
                        duppage(newenvid, i / BY2PG);
                }
#ifdef DEBUG
                writef("fork@fork.c: duppage succeeded\n");
#endif
                syscall_mem_alloc(newenvid, UXSTACKTOP - BY2PG, PTE_V|PTE_R);
#ifdef DEBUG
                writef("fork@fork.c: user exception stack alloc succeeded\n");
#endif
                syscall_set_pgfault_handler(newenvid, __asm_pgfault_handler, UXSTACKTOP);
#ifdef DEBUG
                writef("fork@fork.c: page fault handler set for son succeeded\n");
#endif
                syscall_set_env_status(newenvid, ENV_RUNNABLE);
#ifdef DEBUG
                writef("fork@fork.c: son status set succeeded\n");
#endif
        }

#ifdef DEBUG
        writef("fork@fork.c: over\n");
#endif
        return newenvid;
}

// Challenge!
u_int sfork(u_int wrapper, u_int routine, u_int arg, u_int stack) {
        u_int newenvid;
        struct Env *echild;
        int i;

        //The parent installs pgfault using set_pgfault_handler
        set_pgfault_handler(pgfault);
#ifdef DPOSIX
        writef("sfork@fork.c: page fault handler set for father\n");
#endif

        if (env->tcb_super != NULL) {
                user_panic("sfork@fork.c: env %x is not parent\n", env);
        }

        //alloc a new env
        newenvid = syscall_env_alloc();
        for (i = PDMAP; i < USTACKTOP; i += BY2PG) {
                if ((*vpd)[PDX(i)] == 0) {
                        i -= BY2PG;
                        i += PDMAP;
                        continue;
                }
                libpage(newenvid, i / BY2PG);
        }
#ifdef DPOSIX
        writef("sfork@fork.c: libpage succeeded\n");
#endif
        echild = envs + ENVX(newenvid);
        env->tcb_children[env->tcb_cnum++] = echild;
#ifdef DPOSIX
        writef("sfork@fork.c: env %x's %dth pthread set to %x\n", env, env->tcb_cnum, echild);
#endif
        syscall_mem_alloc(newenvid, UXSTACKTOP - BY2PG, PTE_V|PTE_R);
        syscall_set_pgfault_handler(newenvid, __asm_pgfault_handler, UXSTACKTOP);

        echild->tcb_super = env;
        echild->env_tf.regs[29] = stack;
        echild->env_tf.pc = wrapper;
        echild->env_tf.regs[4] = routine;
        echild->env_tf.regs[5] = arg;
        echild->env_tf.regs[6] = newenvid;
        syscall_set_env_status(newenvid, ENV_RUNNABLE);
#ifdef DPOSIX
        writef("sfork@fork.c: son status set succeeded with stack: %x, wrapper: %x\n", stack, wrapper);
#endif

        return newenvid;
        // user_panic("sfork not implemented");
}

