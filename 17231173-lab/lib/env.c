/* Notes written by Qian Liu <qianlxc@outlook.com>
 * If you find any bug, please contact with me.*/

#include <mmu.h>
#include <error.h>
#include <env.h>
#include <kerelf.h>
#include <sched.h>
#include <pmap.h>
#include <printf.h>

struct Env *envs = NULL;                // All environments
struct Env *curenv = NULL;              // the current env

static struct Env_list env_free_list;   // Free list
struct Env_list env_sched_list[2];      // Runnable list

extern Pde *boot_pgdir;
extern char *KERNEL_SP;


/* Overview:
 *  This function is for making an unique ID for every env.
 *
 * Pre-Condition:
 *  Env e is exist.
 *
 * Post-Condition:
 *  return e's envid on success.
 */
u_int mkenvid(struct Env *e) {
        static u_long next_env_id = 0;
        u_int idx = e - envs;
#ifdef DEBUG
        u_int result = (++next_env_id << (1 + LOG2NENV)) | idx;
        printf("mkenvid@env.c: generated env id %x for env %x\n", result, e);
        return result;
#endif
        return (++next_env_id << (1 + LOG2NENV)) | idx;
}
/*Hint: lower bits of envid hold e's position in the envs array. */
/*Hint:  high bits of envid hold an increasing number. */

/* Overview:
 *  Converts an envid to an env pointer.
 *  If envid is 0 , set *penv = curenv;otherwise set *penv = envs[ENVX(envid)];
 *
 * Pre-Condition:
 *  Env penv is exist,checkperm is 0 or 1.
 *
 * Post-Condition:
 *  return 0 on success,and sets *penv to the environment.
 *  return -E_BAD_ENV on error,and sets *penv to NULL.
 */
int envid2env(u_int envid, struct Env **penv, int checkperm) {
        if (!envid) {
                *penv = curenv;
                return 0;
        }

        struct Env *e;
        e = envs + ENVX(envid);

        if (e->env_status == ENV_FREE || e->env_id != envid) {
                *penv = 0;
                return -E_BAD_ENV;
        }
        if (checkperm && e->env_id != curenv->env_id && e->env_parent_id != curenv->env_id) {
                *penv = 0;
                return -E_BAD_ENV;
        }

        *penv = e;
        return 0;
}
/* Hint:
 * If envid is zero, return the current environment.*/
/*Step 1: Assign value to e using envid. */
/* Hint:
 * Check that the calling environment has legitimate permissions
 * to manipulate the specified environment.
 * If checkperm is set, the specified environment
 * must be either the current environment.
 * or an immediate child of the current environment.If not, error! */
/*Step 2: Make a check according to checkperm. */

/* Overview:
 *  Mark all environments in 'envs' as free and insert them into the env_free_list.
 *  Insert in reverse order,so that the first call to env_alloc() return envs[0].
 *
 * Hints:
 *  You may use these defines to make it:
 *      LIST_INIT,LIST_INSERT_HEAD
 */
void env_init(void) {
        LIST_INIT(&env_free_list);
#ifdef DEBUG
        printf("env_init@env.c: list init env_free_list succeeded\n");
#endif

        int i;
        for (i = NENV-1; i >= 0; i--) {
                envs[i].env_status = ENV_FREE;
                LIST_INSERT_HEAD(&env_free_list, envs+i, env_link);
        }
#ifdef DEBUG
        printf("env_init@env.c: env init succeeded\n");
#endif
}
/*Step 1: Initial env_free_list. */
/*Step 2: Travel the elements in 'envs', init every element(mainly initial its status, mark it as free)
 * and inserts them into the env_free_list as reverse order. */

/* Overview:
 *  Initialize the kernel virtual memory layout for environment e.
 *  Allocate a page directory, set e->env_pgdir and e->env_cr3 accordingly,
 *  and initialize the kernel portion of the new environment's address space.
 *  Do NOT map anything into the user portion of the environment's virtual address space.
 */
/***Your Question Here***/
static int env_setup_vm(struct Env *e) {
        int i, r;
        struct Page *p = NULL;
        Pde *pgdir;

        if ((r = page_alloc(&p))) {/* page_alloc returns -E_NO_MEM*/
                panic("env_setup_vm - page alloc error\n");
                return r;
        }
        p->pp_ref++;
        pgdir = (Pde *)page2kva(p);

        for (i = 0; i < PDX(UTOP); i++) {
                pgdir[i] = 0;
        }

        for (; i < PDX(~0); i++) {
                pgdir[i] = boot_pgdir[i];
        }

        e->env_pgdir = pgdir;
        e->env_cr3 = PADDR(pgdir);

        e->env_pgdir[PDX(VPT)]   = e->env_cr3;
        e->env_pgdir[PDX(UVPT)]  = e->env_cr3 | PTE_V | PTE_R;
        return 0;
}
/*Step 1: Allocate a page for the page directory using a function you completed in the lab2.
 * and add its reference.
 * pgdir is the page directory of Env e, assign value for it. */
/*Step 2: Zero pgdir's field before UTOP. */
/*Step 3: Copy kernel's boot_pgdir to pgdir. */
/* Hint:
 *  The VA space of all envs is identical above UTOP
 *  (except at VPT and UVPT, which we've set below).
 *  See ./include/mmu.h for layout.
 *  Can you use boot_pgdir as a template?
 */
/*Step 4: Set e->env_pgdir and e->env_cr3 accordingly. */
/*VPT and UVPT map the env's own page table, with
 * different permissions. */

/* Overview:
 *  Allocates and Initializes a new environment.
 *  On success, the new environment is stored in *new.
 *
 * Pre-Condition:
 *  If the new Env doesn't have parent, parent_id should be zero.
 *  env_init has been called before this function.
 *
 * Post-Condition:
 *  return 0 on success, and set appropriate values for Env new.
 *  return -E_NO_FREE_ENV on error, if no free env.
 *
 * Hints:
 *  You may use these functions and defines:
 *      LIST_FIRST,LIST_REMOVE,mkenvid (Not All)
 *  You should set some states of Env:
 *      id , status , the sp register, CPU status , parent_id
 *      (the value of PC should NOT be set in env_alloc)
 */
int env_alloc(struct Env **new, u_int parent_id) {
#ifdef DEBUG
        printf("env_alloc@env.c called with (struct Env **new, u_int parent_id: %x)\n", parent_id);
#endif
        // int r;
        struct Env *e;

        *new = 0;
        if (LIST_EMPTY(&env_free_list)) {
                return -E_NO_FREE_ENV;
        }

        e = LIST_FIRST(&env_free_list);
        env_setup_vm(e);
        e->env_id = mkenvid(e);
        e->env_parent_id = parent_id;
        e->env_status = ENV_RUNNABLE;

        e->env_tf.regs[29] = USTACKTOP;
        e->env_tf.cp0_status = 0x10001004;

        e->tcb_super = NULL;
        e->tcb_cnum = 0;
        e->retval = NULL;
        e->dead = 0;

        LIST_REMOVE(e, env_link);

        *new = e;

#ifdef DEBUG
        printf("env_alloc@env.c: alloc env succeeded with *new=%x\n", *new);
#endif
        return 0;
}
/*Step 1: Get a new Env from env_free_list*/
/*Step 2: Call certain function(has been implemented) to init kernel memory layout for this new Env.
 *The function mainly maps the kernel address to this new Env address. */
/*Step 3: Initialize every field of new Env with appropriate values*/
/*Step 4: focus on initializing env_tf structure, located at this new Env.
 * especially the sp register,CPU status. */
/*Step 5: Remove the new Env from Env free list*/

/* Overview:
 *   This is a call back function for kernel's elf loader.
 * Elf loader extracts each segment of the given binary image.
 * Then the loader calls this function to map each segment
 * at correct virtual address.
 *
 *   `bin_size` is the size of `bin`. `sgsize` is the
 * segment size in memory.
 *
 * Pre-Condition:
 *   bin can't be NULL.
 *   Hint: va may NOT aligned 4KB.
 *
 * Post-Condition:
 *   return 0 on success, otherwise < 0.
 */
static int load_icode_mapper(u_long va, u_int32_t sgsize, u_char *bin, u_int32_t bin_size, void *user_data) {
#ifdef DEBUG
        printf("load_icode_mapper@env.c called\n");
#endif
        struct Env *env = (struct Env *)user_data;
        struct Page *p = NULL;
        u_long i = 0;
        int r;
        u_long offset = va - ROUNDDOWN(va, BY2PG);

        Pde *pgdir = env->env_pgdir;

        for (i = 0; i < bin_size; i += BY2PG) {
                if ((r = page_alloc(&p))) return r;
                if (i == 0) {
                        bcopy((void *)bin,
                                        (void *)page2kva(p)+offset,
                                        (BY2PG - offset < bin_size) ? (BY2PG - offset) : bin_size);
                        i = -offset;
                } else {
                        bcopy((void *)bin + i,
                                        (void *)page2kva(p),
                                        (BY2PG < bin_size - i) ? BY2PG : (bin_size - i));
                }
                if ((r = page_insert(pgdir, p, va + i, PTE_R))) return r;
        }

        for (; i < sgsize; i += BY2PG) {
                if ((r = page_alloc(&p))) return r;
                if ((r = page_insert(pgdir, p, va+i, PTE_R))) return r;
                bzero((void *)page2kva(p), BY2PG);
        }
        return 0;
}
/* Step 1: load all content of bin into memory. */
/* Hint: You should alloc a page and increase the reference count of it. */
/* Step 2: alloc pages to reach `sgsize` when `bin_size` < `sgsize`.
 * i has the value of `bin_size` now. */

/* Overview:
 *  Sets up the the initial stack and program binary for a user process.
 *  This function loads the complete binary image by using elf loader,
 *  into the environment's user memory. The entry point of the binary image
 *  is given by the elf loader. And this function maps one page for the
 *  program's initial stack at virtual address USTACKTOP - BY2PG.
 *
 * Hints:
 *  All mappings are read/write including those of the text segment.
 *  You may use these :
 *      page_alloc, page_insert, page2kva , e->env_pgdir and load_elf.
 */
static void load_icode(struct Env *e, u_char *binary, u_int size) {
        struct Page *p = NULL;
        u_long entry_point;
        u_long r;
        u_long perm = PTE_V | PTE_R;

        r = page_alloc(&p);
        if (r) return;
        r = page_insert(e->env_pgdir, p, USTACKTOP-BY2PG, perm);
        if (r) return;

        load_elf(binary, size, &entry_point, e, load_icode_mapper);

        e->env_tf.pc = entry_point;
}
/* Hint:
 *  You must figure out which permissions you'll need
 *  for the different mappings you create.
 *  Remember that the binary image is an a.out format image,
 *  which contains both text and data.
 */
/*Step 2: Use appropriate perm to set initial stack for new Env. */
/*Hint: The user-stack should be writable? */
/*Step 3:load the binary by using elf loader. */
/***Your Question Here***/
/*Step 4:Set CPU's PC register as appropriate value. */

/* Overview:
 *  Allocates a new env with env_alloc, loads the named elf binary into
 *  it with load_icode and then set its priority value. This function is
 *  ONLY called during kernel initialization, before running the first
 *  user_mode environment.
 *
 * Hints:
 *  this function wrap the env_alloc and load_icode function.
 */
void env_create_priority(u_char *binary, int size, int priority) {
        struct Env *e;
        if (env_alloc(&e, 0)) return;
        e->env_pri = priority;
        load_icode(e, binary, size);
        LIST_INSERT_HEAD(env_sched_list, e, env_sched_link);
}
/*Step 1: Use env_alloc to alloc a new env. */
/*Step 2: assign priority to the new env. */
/*Step 3: Use load_icode() to load the named elf binary. */

/* Overview:
 * Allocates a new env with default priority value.
 *
 * Hints:
 *  this function warp the env_create_priority function/
 */
void env_create(u_char *binary, int size) {
        env_create_priority(binary, size, 1);
}
/*Step 1: Use env_create_priority to alloc a new env with priority 1 */

static void mem_map(struct Env *dstenv, u_int va) {

        Pte *ppte = NULL;
        struct Page *ppage = page_lookup(curenv->env_pgdir, va, &ppte);
        u_int perm = PTE_V|PTE_R|PTE_LIBRARY;
        if (page_insert(dstenv->env_pgdir, ppage, va, perm)) {
                panic("mem_map@env.c: page_insert failed\n");
        }
}

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

/* Overview:
 *  Frees env e and all memory it uses.
 */
void env_free(struct Env *e) {
        Pte *pt;
        u_int pdeno, pteno, pa;

        /* Hint: Note the environment's demise.*/
        printf("[%08x] free env %08x\n", curenv ? curenv->env_id : 0, e->env_id);

        /* Hint: Flush all mapped pages in the user portion of the address space */
        for (pdeno = 0; pdeno < PDX(UTOP); pdeno++) {
                /* Hint: only look at mapped page tables. */
                if (!(e->env_pgdir[pdeno] & PTE_V)) {
                        continue;
                }
                /* Hint: find the pa and va of the page table. */
                pa = PTE_ADDR(e->env_pgdir[pdeno]);
                pt = (Pte *)KADDR(pa);
                /* Hint: Unmap all PTEs in this page table. */
                for (pteno = 0; pteno <= PTX(~0); pteno++)
                        if (pt[pteno] & PTE_V) {
                                page_remove(e->env_pgdir, (pdeno << PDSHIFT) | (pteno << PGSHIFT));
                        }
                /* Hint: free the page table itself. */
                e->env_pgdir[pdeno] = 0;
                page_decref(pa2page(pa));
        }
        /* Hint: free the page directory. */
        pa = e->env_cr3;
        e->env_pgdir = 0;
        e->env_cr3 = 0;
        page_decref(pa2page(pa));
        /* Hint: return the environment to the free list. */
        e->env_status = ENV_FREE;
        LIST_INSERT_HEAD(&env_free_list, e, env_link);
        LIST_REMOVE(e, env_sched_link);
}

/* Overview:
 *  Frees env e, and schedules to run a new env
 *  if e is the current env.
 */
void env_destroy(struct Env *e) {
        int i;
        if (e->tcb_super != NULL) {
                panic("env_destroy@env.c: target is not process\n");
        }
        /* Hint: free e. */
        struct Env *child;
        for (i = 0; i < e->tcb_cnum; i++) {
                envid2env(e->tcb_children[i]->env_id, &child, 0);
                env_free(child);
        }
        env_free(e);

        /* Hint: schedule to run a new environment. */
        if (curenv == e) {
                curenv = NULL;
                /* Hint:Why this? */
                bcopy((void *)KERNEL_SP - TF_SIZE, (void *)TIMESTACK - TF_SIZE, TF_SIZE);
                printf("i am killed ... \n");
                sched_yield();
        }
}

extern void env_pop_tf(struct Trapframe *tf, int id);
extern void lcontext(u_int contxt);

/* Overview:
 *  Restores the register values in the Trapframe with the
 *  env_pop_tf, and context switch from curenv to env e.
 *
 * Post-Condition:
 *  Set 'e' as the curenv running environment.
 *
 * Hints:
 *  You may use these functions:
 *      env_pop_tf and lcontext.
 */
void env_run(struct Env *e) {
        if (curenv) {
                curenv->env_tf = *((struct Trapframe *)TIMESTACK-1);
                curenv->env_tf.pc = curenv->env_tf.cp0_epc;
        }
        curenv = e;
        curenv->env_runs++;
        lcontext((u_long)curenv->env_pgdir);
        env_pop_tf(&(curenv->env_tf), GET_ENV_ASID(curenv->env_id));
}
/*Step 1: save register state of curenv. */
/* Hint: if there is a environment running,you should do
 *  context switch.You can imitate env_destroy() 's behaviors.*/
/*Step 2: Set 'curenv' to the new environment. */
/*Step 3: Use lcontext() to switch to its address space. */
/*Step 4: Use env_pop_tf() to restore the environment's
 * environment   registers and drop into user mode in the
 * the   environment.
 */
/* Hint: You should use GET_ENV_ASID there.Think why? */

void env_check()
{
        struct Env *temp, *pe, *pe0, *pe1, *pe2;
        struct Env_list fl;
        int re = 0;
        // should be able to allocate three envs
        pe0 = 0;
        pe1 = 0;
        pe2 = 0;
        assert(env_alloc(&pe0, 0) == 0);
        assert(env_alloc(&pe1, 0) == 0);
        assert(env_alloc(&pe2, 0) == 0);

        assert(pe0);
        assert(pe1 && pe1 != pe0);
        assert(pe2 && pe2 != pe1 && pe2 != pe0);

        // temporarily steal the rest of the free envs
        fl = env_free_list;
        // now this env_free list must be empty!!!!
        LIST_INIT(&env_free_list);

        // should be no free memory
        assert(env_alloc(&pe, 0) == -E_NO_FREE_ENV);

        // recover env_free_list
        env_free_list = fl;

        printf("pe0->env_id %d\n",pe0->env_id);
        printf("pe1->env_id %d\n",pe1->env_id);
        printf("pe2->env_id %d\n",pe2->env_id);

        assert(pe0->env_id == 2048);
        assert(pe1->env_id == 4097);
        assert(pe2->env_id == 6146);
        printf("env_init() work well!\n");

        /* check envid2env work well */
        pe2->env_status = ENV_FREE;
        re = envid2env(pe2->env_id, &pe, 0);

        assert(pe == 0 && re == -E_BAD_ENV);

        pe2->env_status = ENV_RUNNABLE;
        re = envid2env(pe2->env_id, &pe, 0);

        assert(pe->env_id == pe2->env_id &&re == 0);

        temp = curenv;
        curenv = pe0;
        re = envid2env(pe2->env_id, &pe, 1);
        assert(pe == 0 && re == -E_BAD_ENV);
        curenv = temp;
        printf("envid2env() work well!\n");

        /* check env_setup_vm() work well */
        printf("pe1->env_pgdir %x\n",pe1->env_pgdir);
        printf("pe1->env_cr3 %x\n",pe1->env_cr3);

        assert(pe2->env_pgdir[PDX(UTOP)] == boot_pgdir[PDX(UTOP)]);
        assert(pe2->env_pgdir[PDX(UTOP)-1] == 0);
        printf("env_setup_vm passed!\n");

        assert(pe2->env_tf.cp0_status == 0x10001004);
        printf("pe2`s sp register %x\n",pe2->env_tf.regs[29]);
        printf("env_check() succeeded!\n");
}

