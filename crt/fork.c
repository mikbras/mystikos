#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <myst/lfence.h>
#include <myst/setjmp.h>

#define DEFAULT_STACK_SIZE (4 * 1024 * 1024)

/* structure used to fetch the canary value */
typedef struct td
{
    struct td* self;
    uint64_t reserved0;
    uint64_t reserved1;
    uint64_t reserved2;
    uint64_t reserved3;
    uint64_t canary;
    uint64_t canary2;
} td_t;

static td_t* _get_td(void)
{
    td_t* td;
    __asm__ volatile("mov %%fs:0, %0" : "=r"(td));
    assert(td == td->self);
    return td;
}

struct thread_args
{
    myst_jmp_buf_t env;
    void* child_stack;
    void* child_sp;
    void* child_bp;
    pid_t pid;
    uint64_t canary;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
};

/* internal musl function */
extern int __clone(int (*func)(void*), void* stack, int flags, void* arg, ...);

#if 0
static void _wait(volatile int* addr, int val)
{
    int spins = 100;

    while (spins-- && *addr == val)
        __asm__ __volatile__("pause" : : : "memory");

    while (*addr == val)
    {
        const int op = FUTEX_WAIT | FUTEX_WAIT_PRIVATE;
        syscall(SYS_futex, addr, op, val, 0);
    }
}
#endif

#if 0
static void _wake(volatile void* addr)
{
    const int op = FUTEX_WAKE | FUTEX_WAKE_PRIVATE;
    syscall(SYS_futex, addr, op, -1);
}
#endif

/* ATTN: arrange for this function to be called on exit */
__attribute__((__unused__)) static void _thread_args_free(void* arg)
{
    struct thread_args* args = (struct thread_args*)arg;
    free(args->child_stack);
    free(args);
}

static int _valid_stack_address(const void* sp, const void* p)
{
    const void* bottom = (uint8_t*)sp + DEFAULT_STACK_SIZE;

    if (p >= sp && p < bottom)
        return 1;

    return 0;
}

size_t _find_base_pointer_offsets(
    const void* sp,
    const void* bp,
    size_t* offsets,
    size_t size)
{
    size_t n = 0;

    //    while (n < size && _valid_stack_address(sp, bp))
    while (n < size && bp != NULL)
    {
        offsets[n] = (uint64_t)bp - (uint64_t)sp;
        bp = *((void**)bp);
        n++;
    }

    return n;
}

static int _create_child_stack(
    const void* parent_sp,
    const void* parent_bp,
    void** child_stack_out,
    void** child_sp_out,
    void** child_bp_out)
{
    int ret = 0;
    const size_t NOFFSETS = 64;
    size_t offsets[NOFFSETS];
    size_t n;
    size_t parent_stack_depth;
    size_t bp_offset;
    uint8_t* child_stack = NULL;
    uint8_t* child_sp = NULL;
    uint8_t* child_bp = NULL;

    if (child_sp_out)
        *child_sp_out = NULL;

    if (child_bp_out)
        *child_bp_out = NULL;

    /* find offsets to next two levels of base pointers in the parent stack */
    n = _find_base_pointer_offsets(parent_sp, parent_bp, offsets, NOFFSETS);

#if 0
    if (n != NOFFSETS)
    {
        ret = -ENOSYS;
        goto done;
    }
#endif

    printf("nnnnnnnnnnnnnnnnnnn=%zu\n", n);
    for (size_t i = 0; i < n; i++)
        printf("offset=%zu\n", offsets[i]);

    /* compute the base offset of the base relative to the stack */
    bp_offset = parent_bp - parent_sp;

    /* compute the new stack base size */
    parent_stack_depth = offsets[n - 1];
    printf("parent_stack_depth=%zu\n", parent_stack_depth);

#if 0
    printf("parent_stack_depth=%zu\n", parent_stack_depth);
    printf("parent_bp=%p\n", parent_bp);
    printf("noffsets=%zu\n", noffsets);

    for (size_t i = 0; i < noffsets; i++)
    {
        uint64_t* addr = (uint64_t*)(((uint8_t*)parent_sp + offsets[i]));
        printf("offsets[%zu]=%zu addr=%lx\n", i, offsets[i], *addr);
    }
#endif

    /* allocate the child stack */
    {
        const size_t alignment = 16;

        if (!(child_stack = memalign(alignment, DEFAULT_STACK_SIZE)))
        {
            ret = -ENOMEM;
            goto done;
        }

        memset(child_stack, 0, DEFAULT_STACK_SIZE);
    }

    /* copy parent stack to the end of the new child stack */
    child_sp = child_stack + DEFAULT_STACK_SIZE - parent_stack_depth - 128;
    child_sp = (uint8_t*)((uint64_t)child_sp & 0xfffffffffffffff0);
    memcpy(child_sp, parent_sp, parent_stack_depth + sizeof(uint64_t));

    /* calculate the child base pointer */
    child_bp = child_sp + bp_offset;

    /* fixup base pointers on the cloned stack */
    {
        ssize_t delta = (uint64_t)parent_sp - (int64_t)child_sp;

        for (size_t i = 0; i < n; i++)
        {
            uint64_t* addr = (uint64_t*)((child_sp + offsets[i]));

            // if (_valid_stack_address((void*)parent_sp, (void*)*addr))
            {
                printf("inside\n");
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

static int _child_func(void* arg)
{
    struct thread_args* args = (struct thread_args*)arg;
    args->env.rsp = (uint64_t)args->child_sp;
    args->env.rbp = (uint64_t)args->child_bp;

    /* propagate the parent process's canary to avoid stack check assertion */
    //_get_td()->canary = args->canary;

    /* set the pid that the parent is waiting on */
    pthread_mutex_lock(&args->mutex);
    args->pid = getpid();
    pthread_cond_signal(&args->cond);
    pthread_mutex_unlock(&args->mutex);

    /* jump back but on the new child stack */
    myst_longjmp(&args->env, 1);
    return 0;
}

__attribute__((__returns_twice__))
__attribute__((__optimize__("-fno-stack-protector"))) pid_t
myst_fork(void)
{
    pid_t pid = 0;
    myst_jmp_buf_t env;

    if (myst_setjmp(&env) == 0) /* parent */
    {
        struct thread_args* args;
        void* stack = NULL;
        const void* parent_sp = (const void*)env.rsp;
        const void* parent_bp = (const void*)env.rbp;
        void* sp = NULL;
        void* bp = NULL;
        const int clone_flags = CLONE_VM | CLONE_VFORK | SIGCHLD;

        if (_create_child_stack(parent_sp, parent_bp, &stack, &sp, &bp) != 0)
            return -ENOMEM;

        if (!(args = calloc(1, sizeof(struct thread_args))))
            return -ENOMEM;

        memcpy(&args->env, &env, sizeof(args->env));
        args->child_stack = stack;
        args->child_sp = sp;
        args->child_bp = bp;
        args->canary = _get_td()->canary;

        __clone(_child_func, sp, clone_flags, args);

        /* wait for child to set args->pid */
        {
            pthread_mutex_lock(&args->mutex);

            while (args->pid == 0)
                pthread_cond_wait(&args->cond, &args->mutex);

            pid = args->pid;
            pthread_mutex_unlock(&args->mutex);
        }

        // pthread_cond_destroy(&args->cond);
        // pthread_mutex_destroy(&args->mutex);
    }
    else /* child */
    {
        pid = 0;
    }

    return pid;
}
