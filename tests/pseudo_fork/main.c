#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>

#include "jump.h"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int gettid()
{
    return syscall(SYS_gettid);
}

__attribute__((__aligned__(16)))
uint8_t stack[8 * 1024];

void* func(void* arg)
{
    myst_jmp_buf_t jmpbuf;
    uint8_t* sp = stack + (sizeof(stack) / 2);

    memcpy(&jmpbuf, arg, sizeof(jmpbuf));
    size_t size = jmpbuf.rbp - jmpbuf.rsp;
    memcpy(sp, (void*)jmpbuf.rsp, size + 1024);
    jmpbuf.rsp = (uint64_t)sp;
    jmpbuf.rbp = (uint64_t)(sp + size);

    printf("threadfunc.tid=%d\n", gettid());
    myst_longjmp(&jmpbuf, 2);
    return NULL;
}

int main()
{
    pthread_t pt;
    int tid = 0;
    myst_jmp_buf_t jmpbuf;

    // --> fork

    if (myst_setjmp(&jmpbuf) == 0)
    {
        /* copy stack */
        pthread_create(&pt, NULL, func, &jmpbuf);
        tid = gettid();
        printf("parent.tid=%d\n", gettid());
    }
    else
    {
        printf("child.tid=%d\n", gettid());
        sleep(1);
        tid = 0;
    }

    printf("common.tid=%d tid=%d &tid=%p\n", gettid(), tid, &tid);

    if (tid == 0)
    {
        /* child exits */
        pthread_exit((void*)0);
    }
    else
    {
        /* parent waits for child to exit */
        printf("join.tid=%d\n", tid);
        fflush(stdout);
        pthread_join(pt, NULL);
    }

    return 0;
}
