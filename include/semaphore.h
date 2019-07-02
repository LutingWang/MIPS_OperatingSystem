/*************************************************************************
    > File Name: semaphore.h
    > Author: Luting Wang
    > Mail: 2457348692@qq.com
    > Created Time: 2019年06月11日 星期二 20时59分17秒
 ************************************************************************/

#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H    1

#include <env.h>

typedef struct {
        int count;
        struct Env_list queue;
        void *shared;
} sem_t;

int sem_init(sem_t *sem, int pshared, u_int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

#endif  /* semaphore.h */

