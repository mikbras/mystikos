#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <myst/tcall.h>

int myst_tcall_map_file(const char* path, void** addr_out, size_t* length_out)
{
    ssize_t ret = 0;
    int fd = -1;
    size_t length;

    if (addr_out)
        *addr_out = NULL;

    if (length_out)
        *length_out = 0;

    if (!path || !addr_out || !length_out)
    {
        ret = -EINVAL;
        goto done;
    }

    /* open the file */
    if ((fd = open(path, O_RDONLY)) < 0)
    {
        ret = -errno;
        goto done;
    }

    /* get the length of the file */
    {
        struct stat statbuf;

        if (fstat(fd, &statbuf) != 0)
        {
            ret = -errno;
            goto done;
        }

        length = statbuf.st_size;
    }

    /* map the file */
    {
        void* addr = NULL;
        const int prot = PROT_READ;
        const int flags = MAP_PRIVATE;
        const size_t offset = 0;

        if ((addr = mmap(NULL, length, prot, flags, fd, offset)) == MAP_FAILED)
        {
            ret = -errno;
            goto done;
        }

        *addr_out = addr;
        *length_out = length;
    }

    ret = fd;
    fd = -1;

done:

    if (fd >= 0)
        close(fd);

    return ret;
}

int myst_tcall_unmap_file(int fd, void* addr, size_t length)
{
    ssize_t ret = 0;

    if (fd < 0 || !addr || length == 0)
    {
        ret = -EINVAL;
        goto done;
    }

    if (munmap(addr, length) != 0)
    {
        ret = -errno;
        goto done;
    }

    if (close(fd) != 0)
    {
        ret = -errno;
        goto done;
    }

done:

    return ret;
}
