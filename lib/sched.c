#include <env.h>
#include <pmap.h>
#include <printf.h>

/* Overview:
 *  Implement simple round-robin scheduling.
 *  Search through 'envs' for a runnable environment ,
 *  in circular fashion statrting after the previously running env,
 *  and switch to the first such environment found.
 *
 * Hints:
 *  The variable which is for counting should be defined as 'static'.
 */
void sched_yield(void) {
#ifdef DEBUG
        printf("sched_yield@sched.c called\n");
#endif
        static int pos = 0; /* current queue index */
        static int times = 0; /* remaining times to exec */
        static struct Env *e = NULL;
        if (e != NULL && e->env_status != ENV_RUNNABLE) {
#ifdef DEBUG
                printf("sched_yield@sched.c: deleting not runnable env %d from env_sched_list\n", e-envs);
#endif
                LIST_REMOVE(e, env_sched_link);
                e = NULL;
                times = 0;
        }
        if (times == 0) { // time is up
#ifdef DEBUG
                if (e != NULL) {
                        printf("sched_yield@sched.c: time up for current env %d\n", e-envs);
                }
#endif
                if (LIST_EMPTY(env_sched_list+pos)) {
                        pos ^= 1;
#ifdef DEBUG
                        printf("sched_yield@sched.c: current env sched list is empty, switched to env sched list %d\n", pos);
#endif
                }
                if (LIST_EMPTY(env_sched_list+pos)) {
#ifdef DEBUG
                        printf("sched_yield@sched.c: env sched list %d is also empty, quiting\n", pos);
#endif
                        panic("sched_yield@sched.c: no available process to schedual\n");
                        return;
                }
                e = LIST_FIRST(env_sched_list+pos);
#ifdef DEBUG
                printf("sched_yield@sched.c: fetched new env %x from list %d\n", e-envs, pos);
#endif
                times = e->env_pri;
                LIST_REMOVE(e, env_sched_link);
                LIST_INSERT_HEAD(env_sched_list+1-pos, e, env_sched_link);
#ifdef DEBUG
                printf("sched_yield@sched.c: moved env %x to list %d\n", e-envs, 1-pos);
#endif
        }
#ifdef DEBUG
        printf("sched_yield@sched.c: preparing to execute, remaining time %d\n", times);
#endif
        times--;
        env_run(e);
}

