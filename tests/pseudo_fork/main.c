#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "jump.h"

int gettid()
{
    return syscall(SYS_gettid);
}

#define DEFAULT_STACK_SIZE (256 * 1024)
#define STACK_EXTRA 1024

static int _valid_stack_address(const void* sp, const void* p)
{
    const void* bottom = (uint8_t*)sp + DEFAULT_STACK_SIZE;

    if (p >= sp && p < bottom)
        return 1;

    return 0;
}

static size_t _memsize(const void* ptr)
{
    const uint8_t* p = ptr;
    size_t size = 0;
    const size_t pgsz = 4096;
    int r;

    p = (void*)(((uint64_t)p + pgsz - 1) / pgsz * pgsz);

    while ((r = msync((void*)p, pgsz, MS_SYNC)) == 0)
    {
        p += pgsz;
        size += pgsz;
    }

    printf("r=%d errno=%d\n", r, errno);

    return size;
}

size_t _find_base_pointer_offsets(
    const void* sp,
    const void* bp,
    size_t* offsets,
    size_t size)
{
    size_t n = 0;

    while (n < size && _valid_stack_address(sp, bp))
    {
        offsets[n] = (uint64_t)bp - (uint64_t)sp;
        bp = *((void**)bp);
        n++;
    }

    return n;
}

int _make_child_stack(
    const void* parent_sp,
    const void* parent_bp,
    void** child_stack_out,
    void** child_sp_out,
    void** child_bp_out)
{
    int ret = 0;
    const size_t NOFFSETS = 256;
    size_t offsets[NOFFSETS];
    size_t noffsets;
    size_t stack_base_size;
    size_t stack_size;
    size_t bp_offset;
    uint8_t* child_stack = NULL;
    uint8_t* child_sp = NULL;
    uint8_t* child_bp = NULL;

    size_t memsize = _memsize(parent_sp);
    printf("memsize=%zu\n", memsize);

    if (child_sp_out)
        *child_sp_out = NULL;

    if (child_bp_out)
        *child_bp_out = NULL;

    /* find the offsets to all the base pointers in the parent stack */
    noffsets = _find_base_pointer_offsets(
        (const void*)parent_sp, (const void*)parent_bp, offsets, NOFFSETS);

    /* compute the base offset of the base relative to the stack */
    bp_offset = parent_bp - parent_sp;

    /* compute the new stack base size */
    stack_base_size = (noffsets > 0) ? offsets[noffsets - 1] : bp_offset;

    /* compute the stack size with some extra bytes for final frame */
    /* ATTN */
    stack_size = stack_base_size + STACK_EXTRA;

#if 1
    printf("noffsets=%zu\n", noffsets);

    for (size_t i = 0; i < noffsets; i++)
    {
        uint64_t* addr = (uint64_t*)(((uint8_t*)parent_sp + offsets[i]));
        printf("offsets[%zu]=%zu addr=%lx\n", i, offsets[i], *addr);
    }
#endif

    /* allocate the child stack */
    {
        if (!(child_stack = memalign(16, DEFAULT_STACK_SIZE)))
        {
            ret = -ENOMEM;
            goto done;
        }

        memset(child_stack, 0, DEFAULT_STACK_SIZE);
    }

    /* copy parent stack to middle of child stack */
    child_sp = child_stack + (DEFAULT_STACK_SIZE / 2);
    memcpy(child_sp, parent_sp, stack_size);

    /* calculate the child base pointer */
    child_bp = child_sp + bp_offset;

    /* fixup base pointers on the cloned stack */
    {
        ssize_t delta = (uint64_t)parent_sp - (int64_t)child_sp;

        for (size_t i = 0; i < noffsets; i++)
        {
            uint64_t* addr = (uint64_t*)(((uint8_t*)child_sp + offsets[i]));

            if (_valid_stack_address((void*)parent_sp, (void*)*addr))
            {
                *addr -= delta;
            }
        }
    }

    *child_stack_out = child_stack;
    *child_sp_out = child_sp;
    *child_bp_out = child_bp;

done:
    return ret;
}

struct thread_args
{
    myst_jmp_buf_t env;
    void* child_stack;
    void* child_sp;
    void* child_bp;
};

static void* _thread_func(void* arg)
{
    struct thread_args* args = (struct thread_args*)arg;
    args->env.rsp = (uint64_t)args->child_sp;
    args->env.rbp = (uint64_t)args->child_bp;
    myst_longjmp(&args->env, 1);
    return NULL;
}

__attribute__((__noinline__)) int pseudo_fork(pthread_t* thread)
{
    int tid = 0;
    myst_jmp_buf_t env;

    if (myst_setjmp(&env) == 0)
    {
        struct thread_args* args;
        void* stack = NULL;
        void* sp = NULL;
        void* bp = NULL;

        if (_make_child_stack(
                (void*)env.rsp, (void*)env.rbp, &stack, &sp, &bp) != 0)
        {
            return -ENOMEM;
        }

        if (!(args = calloc(1, sizeof(struct thread_args))))
            return -ENOMEM;

        memcpy(&args->env, &env, sizeof(args->env));
        args->child_stack = stack;
        args->child_sp = sp;
        args->child_bp = bp;

        pthread_create(thread, NULL, _thread_func, args);
        tid = gettid();
        // printf("pseudo_fork.parent.tid=%d\n", gettid());
    }
    else
    {
        // printf("pseudo_fork.child.tid=%d\n", gettid());
        tid = 0;
    }

    // printf("pseudo_fork.common.tid=%d tid=%d &tid=%p\n", gettid(), tid,
    // &tid);

    return tid;
}

int main(int argc, const char* argv[])
{
    pthread_t thread;

    int tid = pseudo_fork(&thread);

    if (tid < 0)
    {
        fprintf(stderr, "pseudo_fork() failed\n");
        exit(1);
    }
    else if (tid > 0)
    {
        printf("parent: tid=%d\n", tid);
        pthread_join(thread, NULL);
    }
    else
    {
        printf("child: tid=%d\n", tid);
        pthread_exit((void*)0);
    }

    return 0;
}
