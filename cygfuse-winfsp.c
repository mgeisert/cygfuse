/**
 * @file cygfuse/cygfuse-winfsp.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file was  originally part of WinFsp. It has  been relicensed by the
 * original author under the BSD 2-clause license:
 *
 * Copyright (c) 2015-2016, Bill Zissimopoulos. All rights reserved.
 *
 * Redistribution  and use  in source  and  binary forms,  with or  without
 * modification, are  permitted provided that the  following conditions are
 * met:
 *
 * 1.  Redistributions  of source  code  must  retain the  above  copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions  in binary  form must  reproduce the  above copyright
 * notice,  this list  of conditions  and the  following disclaimer  in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY  THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND  ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING, BUT  NOT LIMITED
 * TO,  THE  IMPLIED  WARRANTIES  OF  MERCHANTABILITY  AND  FITNESS  FOR  A
 * PARTICULAR  PURPOSE ARE  DISCLAIMED.  IN NO  EVENT  SHALL THE  COPYRIGHT
 * HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY  DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL   DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO,  PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES; LOSS  OF USE,
 * DATA, OR  PROFITS; OR BUSINESS  INTERRUPTION) HOWEVER CAUSED AND  ON ANY
 * THEORY  OF LIABILITY,  WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE  OR OTHERWISE) ARISING IN  ANY WAY OUT OF  THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modified 2016 by Mark Geisert, designated cygfuse maintainer.
 */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/cygwin.h>

#include "cygfuse-internal.h"

/* WinFsp DLL name/path */
#if defined(__LP64__)
#define WINFSP_NAME                     "winfsp-x64.dll"
#else
#define WINFSP_NAME                     "winfsp-x86.dll"
#endif
#define WINFSP_PATH                     "bin\\" WINFSP_NAME

/* FSP_FUSE_API_IMPL* macros: they (mainly) forward calls to the WinFsp DLL */
#define REMOVE_PARENS(...)              __VA_ARGS__
#define FSP_FUSE_API_IMPL_DEF(RET, API, PARAMS, ...)\
    static RET fsp_ ## API PARAMS __VA_ARGS__\
    extern RET (*pfn_ ## API) PARAMS;
#define FSP_FUSE_API_IMPL(RET, API, PARAMS, ARGS)\
    static RET (*pfn_fsp_ ## API) (struct fsp_fuse_env *env, REMOVE_PARENS PARAMS);\
    FSP_FUSE_API_IMPL_DEF(RET, API, PARAMS, { return pfn_fsp_ ## API (&fsp_fuse_env, REMOVE_PARENS ARGS); })
#define FSP_FUSE_API_IMPL_VOID(RET, API)\
    static RET (*pfn_fsp_ ## API) (struct fsp_fuse_env *env);\
    FSP_FUSE_API_IMPL_DEF(RET, API, (void), { return pfn_fsp_ ## API (&fsp_fuse_env); })

/* FSP_FUSE_API_INIT* macros: they dlsym symbols from the WinFsp DLL */
#define FSP_FUSE_API_INIT(h, n)\
    if (0 == (*(void **)&(pfn_fsp_ ## n) = dlsym(h, "fsp_" #n)))\
        return 0;\
    else\
        pfn_ ## n = fsp_ ## n
#define FSP_FUSE_API_INIT_DLSYM(h, n)\
    if (0 == (*(void **)&(pfn_fsp_ ## n) = dlsym(h, "fsp_" #n)))\
        return 0;
#define FSP_FUSE_API_INIT_NOSYM(n)\
    pfn_ ## n = fsp_ ## n

/*
 * The WinFsp-FUSE environment.
 *
 * This captures important information for the WinFsp DLL,
 * like the Cygwin malloc/free, etc.
 */

#define FSP_FUSE_ENV_INIT               \
    {                                   \
        'C',                            \
        malloc, free,                   \
        fsp_fuse_daemonize,             \
        fsp_fuse_set_signal_handlers_impl,\
    }

struct fsp_fuse_env
{
    unsigned environment;               /* 'C' for Cygwin, 'W' for Windows */
    void *(*memalloc)(size_t);          /* the local malloc */
    void (*memfree)(void *);            /* the local free */
    int (*daemonize)(int);              /* how to daemonize */
    int (*set_signal_handlers)(void *); /* how to set/reset signal handlers */
    void (*reserved[4])();
};

static int fsp_fuse_daemonize(int foreground);
static int fsp_fuse_set_signal_handlers_impl(void *se);

static struct fsp_fuse_env fsp_fuse_env = FSP_FUSE_ENV_INIT;


/*
 * Signal handling.
 *
 * Cygwin supports POSIX signals and we can simply set up signal handlers
 * similar to what libfuse does. However this simple approach does not work
 * within WinFsp, because it uses native API’s that Cygwin cannot interrupt
 * with its signal mechanism. For example, the fuse_loop FUSE call eventually
 * results in a WaitForSingleObject API call that Cygwin cannot interrupt.
 * Even trying with an alertable WaitForSingleObjectEx did not work as
 * unfortunately Cygwin does not issue a QueueUserAPC when issuing a signal.
 * So we need an alternative mechanism to support signals.
 *
 * The alternative is to use sigwait in a separate thread.
 * Fsp_fuse_signal_handler is a WinFsp API that knows how to interrupt that
 * WaitForSingleObject (actually it just signals the waited event).
 */

static void (*pfn_fsp_fuse_signal_handler)(int sig);
static void *fsp_fuse_signal_thread(void *psigmask)
{
    int sig;

    if (0 == sigwait((sigset_t *)psigmask, &sig))
        pfn_fsp_fuse_signal_handler(sig);

    return 0;
}

static int fsp_fuse_set_signal_handlers_impl(void *se)
{
#define FSP_FUSE_SET_SIGNAL_HANDLER(sig, newha)\
    if (-1 != sigaction((sig), 0, &oldsa) &&\
        oldsa.sa_handler == (se ? SIG_DFL : (newha)))\
    {\
        newsa.sa_handler = se ? (newha) : SIG_DFL;\
        sigaction((sig), &newsa, 0);\
    }
#define FSP_FUSE_SIGADDSET(sig)\
    if (-1 != sigaction((sig), 0, &oldsa) &&\
        oldsa.sa_handler == SIG_DFL)\
        sigaddset(&sigmask, (sig));

    static sigset_t sigmask;
    static pthread_t sigthr;
    struct sigaction oldsa, newsa = { 0 };

    if (0 != se)
    {
        if (0 == sigthr)
        {
            FSP_FUSE_SET_SIGNAL_HANDLER(SIGPIPE, SIG_IGN);

            sigemptyset(&sigmask);
            FSP_FUSE_SIGADDSET(SIGHUP);
            FSP_FUSE_SIGADDSET(SIGINT);
            FSP_FUSE_SIGADDSET(SIGTERM);
            if (0 != pthread_sigmask(SIG_BLOCK, &sigmask, 0))
                return -1;

            if (0 != pthread_create(&sigthr, 0, fsp_fuse_signal_thread, &sigmask))
                return -1;
        }
    }
    else
    {
        if (0 != sigthr)
        {
            pthread_cancel(sigthr);
            pthread_join(sigthr, 0);
            sigthr = 0;

            if (0 != pthread_sigmask(SIG_UNBLOCK, &sigmask, 0))
                return -1;
            sigemptyset(&sigmask);

            FSP_FUSE_SET_SIGNAL_HANDLER(SIGPIPE, SIG_IGN);
        }
    }

    return 0;

#undef FSP_FUSE_SIGADDSET
#undef FSP_FUSE_SET_SIGNAL_HANDLER
}


/*
 * FUSE API implementation.
 *
 * Most of the time we defer to the WinFsp DLL, but we also handle some things
 * ourselves (daemonization, signals).
 */

/* fuse_common.h */
FSP_FUSE_API_IMPL_VOID(int, fuse_version)
FSP_FUSE_API_IMPL(struct fuse_chan *, fuse_mount,
    (const char *mountpoint, struct fuse_args *args),
    (mountpoint, args))
FSP_FUSE_API_IMPL(void, fuse_unmount,
    (const char *mountpoint, struct fuse_chan *ch),
    (mountpoint, ch))
FSP_FUSE_API_IMPL(int, fuse_parse_cmdline,
    (struct fuse_args *args,
        char **mountpoint, int *multithreaded, int *foreground),
    (args, mountpoint, multithreaded, foreground))
FSP_FUSE_API_IMPL_DEF(void, fuse_pollhandle_destroy,
    (struct fuse_pollhandle *ph),
    {})
FSP_FUSE_API_IMPL_DEF(int, fuse_daemonize,
    (int foreground),
    {
        if (!foreground)
        {
            if (-1 == daemon(0, 0))
                return -1;

            /*
             * Unfortunately Cygwin fork is very fragile and cannot even correctly
             * handle dlopen'ed DLL's if they are native (rather than Cygwin ones).
             * [MG:] Cygwin doesn't expect non-Cygwin DLLs to be present.  That's
             * likely a design choice but could arguably be considered a bug.  A fix
             * would depend on the bug being reported to the main mailing list.
             *
             * So we have this very nasty hack where we reset the dlopen'ed handle
             * immediately after daemonization. This will force cygfuse_init() to
             * reload the WinFsp DLL and reset all API pointers in the daemonized
             * process.
             * [MG:] pthread_atfork() could be used for child reinitialization after
             * fork().  No pthreads need be involved to use this API.  This is how
             * Cygwin apps deal with the situation.
             * [BZ:] not sure what happens with multiple threads here. Pthread_atfork
             * may be a better solution for this reason alone.
             */

            /* force reload of FUSE implementation DLL to workaround fork() problems */
            cygfuse_init(1);
        }
        else
            chdir("/");

        return 0;
    })
FSP_FUSE_API_IMPL_DEF(int, fuse_set_signal_handlers,
    (struct fuse_session *se),
    {
        return fsp_fuse_set_signal_handlers_impl(se);
    })
FSP_FUSE_API_IMPL_DEF(void, fuse_remove_signal_handlers,
    (struct fuse_session *se),
    {
        fsp_fuse_set_signal_handlers_impl(0);
    })

/* fuse.h */
FSP_FUSE_API_IMPL(int, fuse_main_real,
    (int argc, char *argv[], const struct fuse_operations *ops, size_t opsize, void *data),
    (argc, argv, ops, opsize, data))
FSP_FUSE_API_IMPL(int, fuse_is_lib_option,
    (const char *opt),
    (opt))
FSP_FUSE_API_IMPL(struct fuse *, fuse_new,
    (struct fuse_chan *ch, struct fuse_args *args,
        const struct fuse_operations *ops, size_t opsize, void *data),
    (ch, args, ops, opsize, data))
FSP_FUSE_API_IMPL(void, fuse_destroy,
    (struct fuse *f),
    (f))
FSP_FUSE_API_IMPL(int, fuse_loop,
    (struct fuse *f),
    (f))
FSP_FUSE_API_IMPL(int, fuse_loop_mt,
    (struct fuse *f),
    (f))
FSP_FUSE_API_IMPL(void, fuse_exit,
    (struct fuse *f),
    (f))
FSP_FUSE_API_IMPL_VOID(struct fuse_context *, fuse_get_context)
FSP_FUSE_API_IMPL_DEF(int, fuse_getgroups,
    (int size, fuse_gid_t list[]),
    { return -ENOSYS; })
FSP_FUSE_API_IMPL_DEF(int, fuse_interrupted,
    (void),
    { return 0; })
FSP_FUSE_API_IMPL_DEF(int, fuse_invalidate,
    (struct fuse *f, const char *path),
    { return -EINVAL; })
FSP_FUSE_API_IMPL_DEF(int, fuse_notify_poll,
    (struct fuse_pollhandle *ph),
    { return 0; })
FSP_FUSE_API_IMPL_DEF(struct fuse_session *, fuse_get_session,
    (struct fuse *f),
    { return (struct fuse_session *)f; })

/* fuse_opt.h */
FSP_FUSE_API_IMPL(int, fuse_opt_parse,
    (struct fuse_args *args, void *data,
        const struct fuse_opt opts[], fuse_opt_proc_t proc),
    (args, data, opts, proc))
FSP_FUSE_API_IMPL(int, fuse_opt_add_arg,
    (struct fuse_args *args, const char *arg),
    (args, arg))
FSP_FUSE_API_IMPL(int, fuse_opt_insert_arg,
    (struct fuse_args *args, int pos, const char *arg),
    (args, pos, arg))
FSP_FUSE_API_IMPL(void, fuse_opt_free_args,
    (struct fuse_args *args),
    (args))
FSP_FUSE_API_IMPL(int, fuse_opt_add_opt,
    (char **opts, const char *opt),
    (opts, opt))
FSP_FUSE_API_IMPL(int, fuse_opt_add_opt_escaped,
    (char **opts, const char *opt),
    (opts, opt))
FSP_FUSE_API_IMPL(int, fuse_opt_match,
    (const struct fuse_opt opts[], const char *opt),
    (opts, opt))


/*
 * WinFsp-FUSE initializaation.
 */

void *cygfuse_winfsp_init()
{
    void *h;

    h = dlopen(WINFSP_NAME, RTLD_NOW);
    if (0 == h)
    {
        char winpath[260], *psxpath;
        int regfd, bytes;

        regfd = open("/proc/registry32/HKEY_LOCAL_MACHINE"
                     "/Software/WinFsp/InstallDir", O_RDONLY);
        if (-1 == regfd)
            return 0;

        bytes = read(regfd, winpath,
                     sizeof winpath - sizeof WINFSP_PATH);
        close(regfd);
        if (-1 == bytes || 0 == bytes)
            return 0;

        if ('\0' == winpath[bytes - 1])
            bytes--;
        memcpy(winpath + bytes,
               WINFSP_PATH, sizeof WINFSP_PATH);

        psxpath = (char *)
            cygwin_create_path(CCP_WIN_A_TO_POSIX | CCP_PROC_CYGDRIVE, winpath);
        if (0 == psxpath)
            return 0;

        h = dlopen(psxpath, RTLD_NOW);
        free(psxpath);
        if (0 == h)
            return 0;
    }

    /* originally in winfsp_fuse.h */
    FSP_FUSE_API_INIT_DLSYM(h, fuse_signal_handler);

    /* fuse_common.h */
    FSP_FUSE_API_INIT(h, fuse_version);
    FSP_FUSE_API_INIT(h, fuse_mount);
    FSP_FUSE_API_INIT(h, fuse_unmount);
    FSP_FUSE_API_INIT(h, fuse_parse_cmdline);
    FSP_FUSE_API_INIT_NOSYM(fuse_pollhandle_destroy);
    FSP_FUSE_API_INIT_NOSYM(fuse_daemonize);
    FSP_FUSE_API_INIT_NOSYM(fuse_set_signal_handlers);
    FSP_FUSE_API_INIT_NOSYM(fuse_remove_signal_handlers);

    /* fuse.h */
    FSP_FUSE_API_INIT(h, fuse_main_real);
    FSP_FUSE_API_INIT(h, fuse_is_lib_option);
    FSP_FUSE_API_INIT(h, fuse_new);
    FSP_FUSE_API_INIT(h, fuse_destroy);
    FSP_FUSE_API_INIT(h, fuse_loop);
    FSP_FUSE_API_INIT(h, fuse_loop_mt);
    FSP_FUSE_API_INIT(h, fuse_exit);
    FSP_FUSE_API_INIT(h, fuse_get_context);
    FSP_FUSE_API_INIT_NOSYM(fuse_getgroups);
    FSP_FUSE_API_INIT_NOSYM(fuse_interrupted);
    FSP_FUSE_API_INIT_NOSYM(fuse_invalidate);
    FSP_FUSE_API_INIT_NOSYM(fuse_notify_poll);
    FSP_FUSE_API_INIT_NOSYM(fuse_get_session);

    /* fuse_opt.h */
    FSP_FUSE_API_INIT(h, fuse_opt_parse);
    FSP_FUSE_API_INIT(h, fuse_opt_add_arg);
    FSP_FUSE_API_INIT(h, fuse_opt_insert_arg);
    FSP_FUSE_API_INIT(h, fuse_opt_free_args);
    FSP_FUSE_API_INIT(h, fuse_opt_add_opt);
    FSP_FUSE_API_INIT(h, fuse_opt_add_opt_escaped);
    FSP_FUSE_API_INIT(h, fuse_opt_match);

    return h;
}
