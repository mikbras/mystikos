// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const char* arg0;

static bool _reached_parent;
static bool _reached_child;

void test(bool exec)
{
    pid_t pid = vfork();

    if (pid < 0)
    {
        fprintf(
            stderr, "%s: fork() failed: %d: %s\n", arg0, pid, strerror(errno));
        assert(0);
    }
    else if (pid == 0) /* child */
    {
        printf("=== inside child\n");
        _reached_child = true;
        sleep(1);

        /* verify that vfork() suspended execution of the parent process */
        assert(_reached_parent == false);

        if (exec)
        {
            char* args[] = {"/bin/child", NULL};
            char* env[] = {NULL};
            execve("/bin/child", args, env);
            assert(false);
        }

        _exit(123);
    }
    else /* parent */
    {
        printf("=== inside parent\n");
        int wstatus;
        _reached_parent = true;
        assert(waitpid(pid, &wstatus, 0) == pid);
        assert(WIFEXITED(wstatus));
        assert(WEXITSTATUS(wstatus) == 123);
    }

    assert(_reached_child == true);
    assert(_reached_parent == true);

    printf("=== passed test (%s): %s\n", arg0, (exec ? "exec" : "exit"));
}

int main(int argc, const char* argv[])
{
    arg0 = argv[0];

    test(true);

    _reached_parent = false;
    _reached_child = false;
    test(false);

    printf("=== passed test (%s)\n", argv[0]);

    return 0;
}
