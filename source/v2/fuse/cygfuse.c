/**
 * @file fuse/cygfuse.c
 *
 * @copyright 2015-2022 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/cygwin.h>
#include <sys/stat.h>

static void *cygfuse_init_slow(int force);
static void *cygfuse_init_winfsp();

static pthread_mutex_t cygfuse_mutex = PTHREAD_MUTEX_INITIALIZER;
static void *cygfuse_handle = 0;

static inline void *cygfuse_init_fast(void)
{
    void *handle = cygfuse_handle;
    __sync_synchronize(); /* memory barrier */
    if (0 == handle)
        handle = cygfuse_init_slow(0);
    return handle;
}

static void *cygfuse_init_slow(int force)
{
    void *handle;
    pthread_mutex_lock(&cygfuse_mutex);
    handle = cygfuse_handle;
    if (force || 0 == handle)
    {
        handle = cygfuse_init_winfsp();
        __sync_synchronize(); /* memory barrier */
        cygfuse_handle = handle;
    }
    pthread_mutex_unlock(&cygfuse_mutex);
    return handle;
}

/*
 * Unfortunately Cygwin fork is very fragile and cannot even correctly
 * handle dlopen'ed DLL's if they are native (rather than Cygwin ones).
 *
 * So we have this very nasty hack where we reset the dlopen'ed handle
 * immediately after daemonization. This will force cygfuse_init() to
 * reload the WinFsp DLL and reset all API pointers in the daemonized
 * process.
 */
static inline int cygfuse_daemon(int nochdir, int noclose)
{
    if (-1 == daemon(nochdir, noclose))
        return -1;

    /* force reload of WinFsp DLL to workaround fork() problems */
    cygfuse_init_slow(1);

    return 0;
}
#define daemon                          cygfuse_daemon

#define FSP_FUSE_API                    static
#define FSP_FUSE_API_NAME(api)          (* pfn_ ## api)
#define FSP_FUSE_API_CALL(api)          (cygfuse_init_fast(), pfn_ ## api)
#define FSP_FUSE_SYM(proto, ...)        __attribute__ ((visibility("default"))) proto { __VA_ARGS__ }
#include <fuse_common.h>
#include <fuse.h>
#include <fuse_opt.h>

#if defined(__LP64__)
#define CYGFUSE_WINFSP_NAME             "winfsp-x64.dll"
#else
#define CYGFUSE_WINFSP_NAME             "winfsp-x86.dll"
#endif
#define CYGFUSE_WINFSP_PATH             "bin\\" CYGFUSE_WINFSP_NAME
#define CYGFUSE_GET_API(h, n)           \
    if (0 == (*(void **)&(pfn_ ## n) = dlsym(h, #n)))\
        return cygfuse_init_fail();

static void *cygfuse_init_fail();
static void *cygfuse_init_winfsp()
{
    void *h;

    h = dlopen(CYGFUSE_WINFSP_NAME, RTLD_NOW);
    if (0 == h)
    {
        char winpath[260], *psxpath;
        int regfd, bytes;

        regfd = open("/proc/registry32/HKEY_LOCAL_MACHINE/Software/WinFsp/InstallDir", O_RDONLY);
        if (-1 == regfd)
            return cygfuse_init_fail();

        bytes = read(regfd, winpath, sizeof winpath - sizeof CYGFUSE_WINFSP_PATH);
        close(regfd);
        if (-1 == bytes || 0 == bytes)
            return cygfuse_init_fail();

        if ('\0' == winpath[bytes - 1])
            bytes--;
        memcpy(winpath + bytes, CYGFUSE_WINFSP_PATH, sizeof CYGFUSE_WINFSP_PATH);

        psxpath = (char *)cygwin_create_path(CCP_WIN_A_TO_POSIX | CCP_PROC_CYGDRIVE, winpath);
        if (0 == psxpath)
            return cygfuse_init_fail();

        h = dlopen(psxpath, RTLD_NOW);
        free(psxpath);
        if (0 == h)
            return cygfuse_init_fail();
    }

    /* winfsp_fuse.h */
    CYGFUSE_GET_API(h, fsp_fuse_signal_handler);

    /* fuse_common.h */
    CYGFUSE_GET_API(h, fsp_fuse_version);
    CYGFUSE_GET_API(h, fsp_fuse_mount);
    CYGFUSE_GET_API(h, fsp_fuse_unmount);
    CYGFUSE_GET_API(h, fsp_fuse_parse_cmdline);
    CYGFUSE_GET_API(h, fsp_fuse_ntstatus_from_errno);

    /* fuse.h */
    CYGFUSE_GET_API(h, fsp_fuse_main_real);
    CYGFUSE_GET_API(h, fsp_fuse_is_lib_option);
    CYGFUSE_GET_API(h, fsp_fuse_new);
    CYGFUSE_GET_API(h, fsp_fuse_destroy);
    CYGFUSE_GET_API(h, fsp_fuse_loop);
    CYGFUSE_GET_API(h, fsp_fuse_loop_mt);
    CYGFUSE_GET_API(h, fsp_fuse_exit);
    CYGFUSE_GET_API(h, fsp_fuse_exited);
    CYGFUSE_GET_API(h, fsp_fuse_get_context);

    /* fuse_opt.h */
    CYGFUSE_GET_API(h, fsp_fuse_opt_parse);
    CYGFUSE_GET_API(h, fsp_fuse_opt_add_arg);
    CYGFUSE_GET_API(h, fsp_fuse_opt_insert_arg);
    CYGFUSE_GET_API(h, fsp_fuse_opt_free_args);
    CYGFUSE_GET_API(h, fsp_fuse_opt_add_opt);
    CYGFUSE_GET_API(h, fsp_fuse_opt_add_opt_escaped);
    CYGFUSE_GET_API(h, fsp_fuse_opt_match);

    return h;
}

static void *cygfuse_init_fail()
{
    fprintf(stderr, "cygfuse: initialization failed: " CYGFUSE_WINFSP_NAME " not found\n");
    exit(1);
    return 0;
}

void *cygfuse_report(char *host, char *path, char *mntpoint, char *type)
{
    char *fname = "/var/run/fuse.mounts"; // file has 80-byte records
#define RECLEN 80
    char *rec = alloca(RECLEN);
    struct stat stat;

    // try to open logfile for binary append; create if not already present
    FILE *f = fopen(fname, "ab");
    if (!f) {
        fprintf(stderr, "cygfuse: open logfile: %s\n", strerror(errno));
        goto bailout;
    }
    if (-1 == fstat(fileno(f), &stat)) {
        fprintf(stderr, "cygfuse: stat logfile: %s\n", strerror(errno));
        goto bailout;
    }
    if (0 == stat.st_size) {
        // new file.. initialize it
        memset(rec, 0, RECLEN);
/*                            0         1         2         3         4 */
/*                             1234567890123456789012345678901234567890 */
        snprintf(rec, RECLEN, "# Updated by FUSE apps; stale entries OK"
                              "; DO NOT EDIT this binary file!\n");
        fwrite(rec, 1, RECLEN, f);
    } else {
        // existing file.. reopen for binary read+write
        f = freopen(fname, "r+b", f);
        if (!f) {
            fprintf(stderr, "cygfuse: reopen logfile: %s\n", strerror(errno));
            goto bailout;
        }
        // look through logfile for record with matching mountpoint
        size_t len = strlen(mntpoint);
        off_t offset = RECLEN;
        fseek(f, offset, SEEK_SET); // position to first data record
        while (offset < stat.st_size) {
            fread(rec, 1, RECLEN, f);
            if (rec[len] == ' ' && !strncmp(rec, mntpoint, len))
                break; // that's a match
            offset += RECLEN;
        }
        fseek(f, offset, SEEK_SET); // position to matching record (or EOF)
    }

    // massage input args as needed
    char *ptr = strrchr(type, '/');
    if (ptr)
        type = ptr;
    memset(rec, 0, RECLEN);
    snprintf(rec, RECLEN, "%s %s %d //%s/%s\n",
             mntpoint, type, getpid(), host, path);

    // write or rewrite record to logfile at current position and finish up
    fwrite(rec, 1, RECLEN, f);
    fclose(f);
    return 0;
bailout:
    exit(1);
#undef RECLEN
}
