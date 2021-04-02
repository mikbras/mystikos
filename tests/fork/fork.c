// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

extern int myst_fork(void);

void foo()
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
        printf("*** inside child\n");
    }
    else
    {
        printf("*** inside parent\n");
    }

    printf("sleep: %d\n", pid);
    sleep(3);
    foo();
    printf("done\n");
    fflush(stdout);
    exit(0);
}
