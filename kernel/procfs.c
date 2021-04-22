// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <myst/eraise.h>
#include <myst/file.h>
#include <myst/fs.h>
#include <myst/kernel.h>
#include <myst/mmanutils.h>
#include <myst/mount.h>
#include <myst/printf.h>
#include <myst/process.h>
#include <myst/procfs.h>

static myst_fs_t* _procfs;

int procfs_setup()
{
    int ret = 0;
    struct vars
    {
        char fdpath[PATH_MAX];
    };
    struct vars* v = NULL;

    if (!(v = malloc(sizeof(struct vars))))
        ERAISE(-ENOMEM);

    if (myst_init_ramfs(myst_mount_resolve, &_procfs) != 0)
    {
        myst_eprintf("failed initialize the proc file system\n");
        ERAISE(-EINVAL);
    }

    if (myst_mkdirhier("/proc", 777) != 0)
    {
        myst_eprintf("cannot create mount point for procfs\n");
        ERAISE(-EINVAL);
    }

    if (myst_mount(_procfs, "/", "/proc") != 0)
    {
        myst_eprintf("cannot mount proc file system\n");
        ERAISE(-EINVAL);
    }

    /* Create /proc/[pid]/fd directory for main thread */
    const size_t n = sizeof(v->fdpath);
    snprintf(v->fdpath, n, "/proc/%d/fd", myst_getpid());
    if (myst_mkdirhier(v->fdpath, 777) != 0)
    {
        myst_eprintf("cannot create the /proc/[pid]/fd directory\n");
        ERAISE(-EINVAL);
    }

done:

    if (v)
        free(v);

    return ret;
}

int procfs_teardown()
{
    if ((*_procfs->fs_release)(_procfs) != 0)
    {
        myst_eprintf("failed to release procfs\n");
        return -1;
    }

    return 0;
}

int procfs_pid_cleanup(pid_t pid)
{
    int ret = 0;
    struct vars
    {
        char pid_dir_path[PATH_MAX];
    };
    struct vars* v = NULL;

    if (!(v = malloc(sizeof(struct vars))))
        ERAISE(-ENOMEM);

    if (!pid)
        ERAISE(-EINVAL);

    snprintf(v->pid_dir_path, sizeof(v->pid_dir_path), "/%d", pid);
    ECHECK(myst_release_tree(_procfs, v->pid_dir_path));

done:

    if (v)
        free(v);

    return ret;
}

static int _meminfo_vcallback(myst_buf_t* vbuf)
{
    int ret = 0;
    size_t totalram;
    size_t freeram;

    ECHECK(myst_get_total_ram(&totalram));
    ECHECK(myst_get_free_ram(&freeram));

    myst_buf_clear(vbuf);
    char tmp[128];
    const size_t n = sizeof(tmp);
    snprintf(tmp, n, "MemTotal:       %lu\n", totalram);
    myst_buf_append(vbuf, tmp, strlen(tmp));
    snprintf(tmp, n, "MemFree:        %lu\n", freeram);
    myst_buf_append(vbuf, tmp, strlen(tmp));

done:
    return ret;
}

static int _self_vcallback(myst_buf_t* vbuf)
{
    int ret = 0;

    struct vars
    {
        char linkpath[PATH_MAX];
    };
    struct vars* v = NULL;

    if (!(v = malloc(sizeof(struct vars))))
        ERAISE(-ENOMEM);

    const size_t n = sizeof(v->linkpath);
    snprintf(v->linkpath, n, "/proc/%d", myst_getpid());
    myst_buf_clear(vbuf);
    myst_buf_append(vbuf, v->linkpath, sizeof(v->linkpath));

done:

    if (v)
        free(v);

    return ret;
}

int create_proc_root_entries()
{
    int ret = 0;

    /* Create /proc/meminfo */
    ECHECK(myst_create_virtual_file(
        _procfs, "/meminfo", S_IFREG, _meminfo_vcallback));

    /* Create /proc/self */
    ECHECK(
        myst_create_virtual_file(_procfs, "/self", S_IFLNK, _self_vcallback));

done:
    return ret;
}
