/*************************************************************************
    > File Name: testsem.c
    > Author: Luting Wang
    > Mail: 2457348692@qq.com
    > Created Time: 2019年06月23日 星期日 14时55分11秒
 ************************************************************************/

#include "lib.h"

// pthread_create
#define MAGIC ((void *) 123)
#define MAGIC2 ((void *) 246)

static void *routine_pthread_create1(void *arg) {
        user_assert(arg == MAGIC);
        return NULL;
}

static void *routine_pthread_create2(void *arg) {
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

// pthread_join
#define MAGIC ((void *) 456)

static void *routine_pthread_join(void *arg) {
        return MAGIC;
}

static void test_pthread_join(void) {
        pthread_t th;
        void *retval;
        pthread_create(&th, NULL, routine_pthread_join, NULL);
        pthread_join(th, &retval);
        user_assert(retval == (void *) MAGIC);
        return;
}

#undef MAGIC

// pthread_exit
#define MAGIC ((void *) 789)

static void *routine_pthread_exit(void *arg) {
        pthread_exit((void *) MAGIC);
        return NULL; // never executed
}

static void test_pthread_exit(void) {
        pthread_t th;
        void *retval;
        pthread_create(&th, NULL, routine_pthread_exit, NULL);
        pthread_join(th, &retval);
        user_assert(retval == (void *) MAGIC);
        return;
}

#undef MAGIC

// pthread_cancel
static void *routine_pthread_cancel(void *arg) {
        int i;
        writef("\nroutine: ");
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
        writef("\ntest_main: ");
        for (i = 1000; i < 1050; i++) {
                writef("%d ", i);
        }

        pthread_cancel(th);
        return;
}

// sem_trywait
static void *routine_sem_trywait1(void *arg) {
        sem_t *s = (sem_t *) arg;
        int c;
        sem_getvalue(s, &c);
        user_assert(c == 0);
        user_assert(sem_trywait(s) == -1);
        sem_getvalue(s, &c);
        user_assert(c == 0);
        sem_post(s+1);
        return NULL;
}

static void *routine_sem_trywait2(void *arg) {
        sem_t *s = (sem_t *) arg;
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
        sem_init(s+1, 0, 0);
        pthread_create(&th1, NULL, routine_sem_trywait1, s);
        pthread_create(&th2, NULL, routine_sem_trywait2, s);
        pthread_join(th1, NULL);
        pthread_join(th2, NULL);
        return;
}

// sem_destroy
static void *routine_sem_destroy1(void *arg) {
        sem_t *s = (sem_t *) arg;
        int c;
        sem_getvalue(s, &c);
        user_assert(c <= 0);
        sem_wait(s);
        writef("routine1: waken\n");
        return NULL;
}

static void *routine_sem_destroy2(void *arg) {
        sem_t *s = (sem_t *) arg;
        int c;
        sem_getvalue(s, &c);
        user_assert(c <= 0);
        sem_wait(s);
        writef("routine2: waken\n");
        return NULL;
}

void test_sem_destroy(void) {
        sem_t s[2];
        pthread_t th1, th2;
        int c;
        sem_init(s, 0, 0);
        sem_init(s+1, 0, 0);
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

// stack share
static void *routine_shared_stack1(void *arg) {
        int i = 200;
        int *a = (int *) 0x7ebfdfd0;
        sem_t *s = (sem_t *) arg;
        sem_post(s);
        sem_wait(s+1);
        writef("routine1: i=%d, &i=%x, a=%x, *a=%d\n", i, &i, a, *a);
        sem_post(s);
        sem_wait(s+1);
        sem_post(s+2);
        return NULL;
}

static void *routine_shared_stack2(void *arg) {
        int i = 100;
        int *a = (int *) 0x7effdfd0;
        sem_t *s = (sem_t *) arg;
        sem_post(s+1);
        sem_wait(s);
        writef("routine2: i=%d, &i=%x, a=%x, *a=%d\n", i, &i, a, *a);
        sem_post(s+1);
        sem_wait(s);
        sem_post(s+2);
        return NULL;
}

static void test_shared_stack(void) {
        sem_t s[3];
        sem_init(s, 0, 0);
        sem_init(s+1, 0, 0);
        sem_init(s+2, 0, 0);
        pthread_t th1, th2;
        pthread_create(&th1, NULL, routine_shared_stack1, s);
        pthread_create(&th2, NULL, routine_shared_stack2, s);
        sem_wait(s+2);
        return;
}

// id
static void *routine_id1(void *arg) {
        writef("routine1: env id is %x\n", syscall_getenvid());
        writef("routine1: thread id is %x\n", syscall_getthreadid());
        return NULL;
}

static void *routine_id2(void *arg) {
        writef("routine2: env id is %x\n", syscall_getenvid());
        writef("routine2: thread id is %x\n", syscall_getthreadid());
        return NULL;
}

static void test_id(void) {
        pthread_t th1, th2;
        pthread_create(&th1, NULL, routine_id1, NULL);
        pthread_create(&th2, NULL, routine_id2, NULL);
        pthread_join(th1, NULL);
        pthread_join(th2, NULL);
        writef("test_main: env id is %x\n", syscall_getenvid());
        writef("test_main: thread id is %x\n", syscall_getthreadid());
        return;
}

#define TEST(name) \
        do {\
                if (fork() == 0) { \
                        writef("\n--------------------------------------------------------------------------------\ntest " #name " begin\n"); \
                        test_##name(); \
                        writef("\ntest " #name " passed\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"); \
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


