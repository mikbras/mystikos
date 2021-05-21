// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYS_myst_crt_memcheck 2014

int main(int argc, const char* argv[])
{
    char buf[PATH_MAX];

    getcwd(buf, sizeof(buf));
    assert(strcmp(buf, "/") == 0);

    void* p = malloc(1024);
    // memset(p, 0, 1025);
    // memset(p - 1, 0, 1025);
    // free(p);

    printf("=== passed test (%s)\n", argv[0]);

    syscall(SYS_myst_crt_memcheck);

    return 0;
}
