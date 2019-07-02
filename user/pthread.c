/*************************************************************************
    > File Name: pthread.c
    > Author: Luting Wang
    > Mail: 2457348692@qq.com
    > Created Time: 2019年06月11日 星期二 22时56分48秒
 ************************************************************************/

#include "lib.h"

// #define DPOSIX

static void thread_wrapper(void *(*start_routine)(void *), void *arg, u_int envid) {
        void *retval;
        *thread = envs + ENVX(envid);
#ifdef DPOSIX
        writef("thread_start_wrapper@pthread.c: ready to launch thread %x\n", *thread);
#endif
        retval = (*start_routine)(arg);
        pthread_exit(retval);
}

/* create a new thread
 * pre-condition:
 *     thread - out arg of new thread id
 *     attr - attributes of the new thread
 *     start_routine - function of the new thread
 *     arg - argument to be passed in start_routine
 * post-condition:
 *     On success, return 0; else, return error number
 */
int pthread_create(pthread_t *newthread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
#ifdef DPOSIX
        writef("pthread_create@pthread.c called with (pthread_t *newthread: %x, const pthread_attr_t *attr: %x, void *(*start_routine)(void *): %x, void *arg: %x)\n", newthread, attr, start_routine, arg);
#endif
        if (attr != NULL) {
                user_panic("pthread_create@pthread.c: pthread_attr not implemented\n");
        }
        u_int newthreadid = sfork((u_int) thread_wrapper, (u_int) start_routine, (u_int) arg, USTACKTOP - PDMAP * (env->tcb_cnum + 1));
        u_int i;
        for (i = 0; i < env->tcb_cnum; i++) {
                if (env->tcb_children[i]->env_id == newthreadid) {
                        *newthread = i + 1;
                }
        }
        return 0;
}

static void pexit(struct Env *e, void *retval) {
        int i;
        e->retval = retval;
        e->dead = 1;
        if (e->tcb_super == NULL) {
                writef("pexit@pthread.c: this thread is not child\n");
                for (i = 0; i < e->tcb_cnum; i++) {
                        pthread_join(i + 1, NULL);
                }
                syscall_env_destroy(e->env_id);
        }
        char s[] = "pexit@pthread.c: this is normal for env %x\n";
        if (e->env_id != (*thread)->env_id) {
                e->env_tf.regs[4] = (u_int) __FILE__;
                e->env_tf.regs[5] = __LINE__;
                e->env_tf.regs[6] = (u_int) s;
                e->env_tf.regs[7] = (u_int) e;
                e->env_tf.pc = (u_int) _user_panic;
        } else {
                user_panic(s, e);
        }
}

/* terminate calling thread
 * pre-condition:
 *     retval - return value of calling thread
 */
void pthread_exit(void *retval) {
#ifdef DPOSIX
        writef("pthread_exit@pthread.c called with (void *retval: %x)\n", retval);
#endif
        pexit(*thread, retval);
}

/* join with a terminated thread
 * pre-condition:
 *     thread - id of the target thread caller is waiting for
 *     retval - out arg for return value of target thread
 * post-condition:
 *     On success, return 0; else, return error number
 */
int pthread_join(pthread_t th, void **thread_return) {
#ifdef DPOSIX
        writef("pthread_join@pthread.c called with (pthread_t th: %x, void **thread_return: %x) %x\n", th, thread_return, env);
#endif
        struct Env *e = th == 0 ? env : env->tcb_children[th-1];
#ifdef DPOSIX
        writef("pthread_join@pthread.c: fetched coresponding thread %x, dead=%d\n", e, e->dead);
#endif
        while (e->dead == 0) {
                syscall_yield();
        }
        if (thread_return != NULL) {
                *thread_return = e->retval;
#ifdef DPOSIX
                writef("pthread_join@pthread.c: thread %x ended with %x\n", th, *thread_return);
#endif
        }
        return 0;
}

/* send a cancellation request to a thread
 * pre-condition:
 *     thread - target thread to be cancelled
 * post-condition:
 *     On success, return 0; else, return error number
 */
int pthread_cancel(pthread_t th) {
#ifdef DPOSIX
        writef("pthread_cancel@pthread.c called with (pthread_t th: %x)\n", th);
#endif
        if (th == 0) {
                pexit(env, NULL);
        } else {
                pexit(env->tcb_children[th-1], NULL);
        }
        return 0;
}

