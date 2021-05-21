#define _GNU_SOURCE
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096

int main()
{
    const int prot = PROT_READ | PROT_WRITE;
    const int flags = MAP_ANONYMOUS | MAP_PRIVATE;
    size_t length = 8 * PAGE_SIZE;
    uint8_t* addr;

    /* allocate 8 pages */
    addr = mmap(NULL, length, prot, flags, -1, 0);
    assert(addr != MAP_FAILED);

    /* release half of the mapping ensuring stationary mremap will succeed */
    length /= 2;
    assert(munmap(addr + length, length) == 0);

    /* try growing the mapping with room left on the right */
    {
        size_t old_size = length;
        size_t new_size = old_size * 2;
        uint8_t* new = mremap(addr, old_size, new_size, 0); // MREMAP_MAYMOVE;
        assert(new != MAP_FAILED);
    }

    /* double unmap will work */
    assert(munmap(addr, length * 2) == 0);
    assert(munmap(addr, length * 2) == 0);

    /* allocate 8 pages */
    addr = mmap(NULL, length, prot, flags, -1, 0);
    assert(addr != MAP_FAILED);

    /* allocate 8 pages in the same spot */
    addr = mmap(addr, length, prot, flags, -1, 0);
    assert(addr != MAP_FAILED);

    /* try growing the mapping with no room left on the right */
    {
        size_t old_size = length;
        size_t new_size = old_size * 2;
        uint8_t* new = mremap(addr, old_size, new_size, 0); // MREMAP_MAYMOVE;
        assert(new == MAP_FAILED);
    }

    /* try growing the mapping with MREMAP_MAYMOVE */
    {
        size_t old_size = length;
        size_t new_size = old_size * 2;
        uint8_t* new = mremap(addr, old_size, new_size, MREMAP_MAYMOVE, NULL);
        assert(new != MAP_FAILED);
        assert(munmap(new, new_size) == 0);
    }

    /* unmap all memory */
    assert(munmap(addr, length) == 0);

    /* allocate 8 pages in the same spot */
    addr = mmap(addr, length, prot, flags, -1, 0);
    assert(addr != MAP_FAILED);

    assert(mprotect(addr, length, PROT_NONE) == 0);

    return 0;
}