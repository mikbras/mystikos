#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>

#include <myst/backtrace.h>
#include <myst/panic.h>
#include <myst/printf.h>
#include <myst/syscallext.h>

MYST_PRINTF_FORMAT(4, 5)
MYST_NORETURN void __myst_panic(
    const char* file,
    size_t line,
    const char* func,
    const char* format,
    ...)
{
    va_list ap;
    void* buf[16];

    size_t n = myst_backtrace(buf, MYST_COUNTOF(buf));

    fprintf(stderr, "*** kernel panic: %s(%zu): %s(): ", file, line, func);

    va_start(ap, format);
    fprintf(stderr, format, ap);
    va_end(ap);

    fprintf(stderr, "\n");

    myst_dump_backtrace(buf, n);

    abort();
}

MYST_PRINTF_FORMAT(1, 2)
int myst_eprintf(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    int n = vfprintf(stderr, format, ap);
    va_end(ap);

    return n;
}

static bool _valid_addr(const void* addr)
{
    return syscall(SYS_myst_valid_addr, addr) == 0;
}

MYST_NOINLINE
size_t myst_backtrace_impl(void** start_frame, void** buffer, size_t size)
{
    void** frame = start_frame;
    size_t n = 0;

    while (n < size)
    {
        if (!_valid_addr(frame) || !_valid_addr(frame[1]))
            break;

        buffer[n++] = frame[1];
        frame = (void**)*frame;
    }

    return n;
}

size_t myst_backtrace(void** buffer, size_t size)
{
    return myst_backtrace_impl(__builtin_frame_address(0), buffer, size);
}

void myst_dump_backtrace(void** buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        fprintf(stderr, "%p\n", buffer[i]);
    }
}

#include "../kernel/debugmalloc.c"

void* malloc(size_t size)
{
    return myst_debug_malloc(size);
}

void* calloc(size_t nmemb, size_t size)
{
    return myst_debug_calloc(nmemb, size);
}

void* realloc(void* ptr, size_t size)
{
    return myst_debug_realloc(ptr, size);
}

int posix_memalign(void** memptr, size_t alignment, size_t size)
{
    return myst_debug_posix_memalign(memptr, alignment, size);
}

void* memalign(size_t alignment, size_t size)
{
    return myst_debug_memalign(alignment, size);
}

void free(void* ptr)
{
    if (ptr)
        return myst_debug_free(ptr);
}

void memcheck(void)
{
    myst_debug_malloc_check(true);
}

void* aligned_alloc(size_t alignment, size_t size)
{
    /* size must be a multiple of alignment */
    if (size % alignment)
        return NULL;

    return memalign(alignment, size);
}
