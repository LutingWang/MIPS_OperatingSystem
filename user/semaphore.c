/*************************************************************************
    > File Name: semaphore.c
    > Author: Luting Wang
    > Mail: 2457348692@qq.com
    > Created Time: 2019年06月11日 星期二 22时57分40秒
 ************************************************************************/

#include "lib.h"

// #define DPOSIX

int sem_init(sem_t *sem, int pshared, u_int value) {
        return msyscall(SYS_sem_init, (u_int) sem, value, pshared, 0, 0);
}

int sem_destroy(sem_t *sem) {
        return msyscall(SYS_sem_destroy, (u_int) sem, 0, 0, 0, 0);
}

int sem_wait(sem_t *sem) {
        if (msyscall(SYS_sem_wait, (u_int) sem, 0, 0, 0, 0)) {
                syscall_yield();
        }
        return 0;
}

int sem_trywait(sem_t *sem) {
 #ifdef DPOSIX
        writef("sem_trywait@semaphore.c called with (sem_t *sem: %x)\n", sem);
 #endif
        if ((msyscall(SYS_sem_trywait, (u_int) sem, 0, 0, 0, 0))) {
                return -1;
        } else {
                return 0;
        }
}

int sem_post(sem_t *sem) {
        return msyscall(SYS_sem_post, (u_int) sem, 0, 0, 0, 0);
}

int sem_getvalue(sem_t *sem, int *sval) {
        return msyscall(SYS_sem_getvalue, (u_int) sem, (u_int) sval, 0, 0, 0);
}

