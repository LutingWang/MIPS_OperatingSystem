#include "lib.h"
#include <mmu.h>
#include <env.h>

struct Env *env;
struct Env **thread = (struct Env **) UTHREAD;

void exit(void) {
        //close_all();
        syscall_env_destroy(0);
}

void libmain(int argc, char **argv) {
        // set env to point at our env structure in envs[].
        //writef("xxxxxxxxx %x  %x  xxxxxxxxx\n",argc,(int)argv);
        int envid;
        envid = syscall_getenvid();
        envid = ENVX(envid);
        env = &envs[envid];

        syscall_mem_alloc(0, UTHREAD, PTE_V|PTE_R|PTE_COW);
        *thread = env;

        syscall_mem_alloc(0, USEM, PTE_V|PTE_R|PTE_LIBRARY);
        sem_t *sems = (sem_t *) USEM;
        sems->shared = sems + 1;

        // call user main routine
        umain(argc,argv);
        // exit gracefully
        exit();
        //syscall_env_destroy(0);
}

