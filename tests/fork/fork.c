// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

extern int myst_fork(void);

int _gettid(void)
{
    return syscall(SYS_gettid);
}

int _printf(const char* fmt, ...)
{
    static pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;
    va_list ap;

    va_start(ap, fmt);
    pthread_mutex_lock(&_lock);
    fprintf(stderr, "_printf: tid=%d: ", _gettid());
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
    pthread_mutex_unlock(&_lock);
    va_end(ap);
}

void point()
{
}

int main(int argc, const char* argv[])
{
    // pid_t pid = fork();
    pid_t pid = syscall(SYS_fork);

    if (pid < 0)
    {
        fprintf(stderr, "%s: fork() failed: %d\n", argv[0], pid);
        exit(1);
    }
    else if (pid == 0)
    {
        _printf("*** inside child\n");
    }
    else
    {
        _printf("*** inside parent\n");
    }

    _printf("sleep: %d\n", pid);

    if (pid == 0)
        sleep(3);
    else
        sleep(2);

    point();

    _printf("done\n");
    exit(0);
}
