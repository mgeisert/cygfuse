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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/cygwin.h>

#include "cygfuse-internal.h"

static int fsp_fuse_daemonize(int foreground);
static int fsp_fuse_set_signal_handlers_impl(void *se);

#define FSP_FUSE_ENV_INIT               \
    {                                   \
        'C',                            \
        malloc, free,                   \
        fsp_fuse_daemonize,             \
        fsp_fuse_set_signal_handlers_impl,\
    }
struct fsp_fuse_env
{
    unsigned environment;
    void *(*memalloc)(size_t);
    void (*memfree)(void *);
    int (*daemonize)(int);
    int (*set_signal_handlers)(void *);
    void (*reserved[4])();
};
static struct fsp_fuse_env fsp_fuse_env = FSP_FUSE_ENV_INIT;

#define REMOVE_PARENS(...)              __VA_ARGS__
#define CYGFUSE_API_IMPL_DEF(RET, API, PARAMS, ...)\
    static RET fsp_ ## API PARAMS __VA_ARGS__\
    extern RET (*pfn_ ## API) PARAMS;
#define CYGFUSE_API_IMPL(RET, API, PARAMS, ARGS)\
    static RET (*pfn_fsp_ ## API) (struct fsp_fuse_env *env, REMOVE_PARENS PARAMS);\
    CYGFUSE_API_IMPL_DEF(RET, API, PARAMS, { return pfn_fsp_ ## API (&fsp_fuse_env, REMOVE_PARENS ARGS); })
#define CYGFUSE_API_IMPL_VOID(RET, API)\
    static RET (*pfn_fsp_ ## API) (struct fsp_fuse_env *env);\
    CYGFUSE_API_IMPL_DEF(RET, API, (void), { return pfn_fsp_ ## API (&fsp_fuse_env); })

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

/* fuse_common.h */
CYGFUSE_API_IMPL_VOID(int, fuse_version)
CYGFUSE_API_IMPL(struct fuse_chan *, fuse_mount,
    (const char *mountpoint, struct fuse_args *args),
    (mountpoint, args))
CYGFUSE_API_IMPL(void, fuse_unmount,
    (const char *mountpoint, struct fuse_chan *ch),
    (mountpoint, ch))
CYGFUSE_API_IMPL(int, fuse_parse_cmdline,
    (struct fuse_args *args,
        char **mountpoint, int *multithreaded, int *foreground),
    (args, mountpoint, multithreaded, foreground))
CYGFUSE_API_IMPL_DEF(void, fuse_pollhandle_destroy,
    (struct fuse_pollhandle *ph),
    {})
CYGFUSE_API_IMPL_DEF(int, fuse_daemonize,
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
CYGFUSE_API_IMPL_DEF(int, fuse_set_signal_handlers,
    (struct fuse_session *se),
    {
        return fsp_fuse_set_signal_handlers_impl(se);
    })
CYGFUSE_API_IMPL_DEF(void, fuse_remove_signal_handlers,
    (struct fuse_session *se),
    {
        fsp_fuse_set_signal_handlers_impl(0);
    })

/* fuse.h */
CYGFUSE_API_IMPL(int, fuse_main_real,
    (int argc, char *argv[], const struct fuse_operations *ops, size_t opsize, void *data),
    (argc, argv, ops, opsize, data))
CYGFUSE_API_IMPL(int, fuse_is_lib_option,
    (const char *opt),
    (opt))
CYGFUSE_API_IMPL(struct fuse *, fuse_new,
    (struct fuse_chan *ch, struct fuse_args *args,
        const struct fuse_operations *ops, size_t opsize, void *data),
    (ch, args, ops, opsize, data))
CYGFUSE_API_IMPL(void, fuse_destroy,
    (struct fuse *f),
    (f))
CYGFUSE_API_IMPL(int, fuse_loop,
    (struct fuse *f),
    (f))
CYGFUSE_API_IMPL(int, fuse_loop_mt,
    (struct fuse *f),
    (f))
CYGFUSE_API_IMPL(void, fuse_exit,
    (struct fuse *f),
    (f))
CYGFUSE_API_IMPL_VOID(struct fuse_context *, fuse_get_context)
CYGFUSE_API_IMPL_DEF(int, fuse_getgroups,
    (int size, fuse_gid_t list[]),
    { return -ENOSYS; })
CYGFUSE_API_IMPL_DEF(int, fuse_interrupted,
    (void),
    { return 0; })
CYGFUSE_API_IMPL_DEF(int, fuse_invalidate,
    (struct fuse *f, const char *path),
    { return -EINVAL; })
CYGFUSE_API_IMPL_DEF(int, fuse_notify_poll,
    (struct fuse_pollhandle *ph),
    { return 0; })
CYGFUSE_API_IMPL_DEF(struct fuse_session *, fuse_get_session,
    (struct fuse *f),
    { return (struct fuse_session *)f; })

/* fuse_opt.h */
CYGFUSE_API_IMPL(int, fuse_opt_parse,
    (struct fuse_args *args, void *data,
        const struct fuse_opt opts[], fuse_opt_proc_t proc),
    (args, data, opts, proc))
CYGFUSE_API_IMPL(int, fuse_opt_add_arg,
    (struct fuse_args *args, const char *arg),
    (args, arg))
CYGFUSE_API_IMPL(int, fuse_opt_insert_arg,
    (struct fuse_args *args, int pos, const char *arg),
    (args, pos, arg))
CYGFUSE_API_IMPL(void, fuse_opt_free_args,
    (struct fuse_args *args),
    (args))
CYGFUSE_API_IMPL(int, fuse_opt_add_opt,
    (char **opts, const char *opt),
    (opts, opt))
CYGFUSE_API_IMPL(int, fuse_opt_add_opt_escaped,
    (char **opts, const char *opt),
    (opts, opt))
CYGFUSE_API_IMPL(int, fuse_opt_match,
    (const struct fuse_opt opts[], const char *opt),
    (opts, opt))

#if defined(__LP64__)
#define CYGFUSE_WINFSP_NAME             "winfsp-x64.dll"
#else
#define CYGFUSE_WINFSP_NAME             "winfsp-x86.dll"
#endif
#define CYGFUSE_WINFSP_PATH             "bin\\" CYGFUSE_WINFSP_NAME

#define CYGFUSE_API_GET(h, n)           \
    if (0 == (*(void **)&(pfn_fsp_ ## n) = dlsym(h, "fsp_" #n)))\
        return 0;\
    else\
        pfn_ ## n = fsp_ ## n
#define CYGFUSE_API_GET_DL(h, n)        \
    if (0 == (*(void **)&(pfn_fsp_ ## n) = dlsym(h, "fsp_" #n)))\
        return 0;
#define CYGFUSE_API_GET_NS(n)           \
    pfn_ ## n = fsp_ ## n

void *cygfuse_winfsp_init()
{
    void *h;

    h = dlopen(CYGFUSE_WINFSP_NAME, RTLD_NOW);
    if (0 == h)
    {
        char winpath[260], *psxpath;
        int regfd, bytes;

        regfd = open("/proc/registry32/HKEY_LOCAL_MACHINE"
                     "/Software/WinFsp/InstallDir", O_RDONLY);
        if (-1 == regfd)
            return 0;

        bytes = read(regfd, winpath,
                     sizeof winpath - sizeof CYGFUSE_WINFSP_PATH);
        close(regfd);
        if (-1 == bytes || 0 == bytes)
            return 0;

        if ('\0' == winpath[bytes - 1])
            bytes--;
        memcpy(winpath + bytes,
               CYGFUSE_WINFSP_PATH, sizeof CYGFUSE_WINFSP_PATH);

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
    CYGFUSE_API_GET_DL(h, fuse_signal_handler);

    /* fuse_common.h */
    CYGFUSE_API_GET(h, fuse_version);
    CYGFUSE_API_GET(h, fuse_mount);
    CYGFUSE_API_GET(h, fuse_unmount);
    CYGFUSE_API_GET(h, fuse_parse_cmdline);
    CYGFUSE_API_GET_NS(fuse_pollhandle_destroy);
    CYGFUSE_API_GET_NS(fuse_daemonize);
    CYGFUSE_API_GET_NS(fuse_set_signal_handlers);
    CYGFUSE_API_GET_NS(fuse_remove_signal_handlers);

    /* fuse.h */
    CYGFUSE_API_GET(h, fuse_main_real);
    CYGFUSE_API_GET(h, fuse_is_lib_option);
    CYGFUSE_API_GET(h, fuse_new);
    CYGFUSE_API_GET(h, fuse_destroy);
    CYGFUSE_API_GET(h, fuse_loop);
    CYGFUSE_API_GET(h, fuse_loop_mt);
    CYGFUSE_API_GET(h, fuse_exit);
    CYGFUSE_API_GET(h, fuse_get_context);
    CYGFUSE_API_GET_NS(fuse_getgroups);
    CYGFUSE_API_GET_NS(fuse_interrupted);
    CYGFUSE_API_GET_NS(fuse_invalidate);
    CYGFUSE_API_GET_NS(fuse_notify_poll);
    CYGFUSE_API_GET_NS(fuse_get_session);

    /* fuse_opt.h */
    CYGFUSE_API_GET(h, fuse_opt_parse);
    CYGFUSE_API_GET(h, fuse_opt_add_arg);
    CYGFUSE_API_GET(h, fuse_opt_insert_arg);
    CYGFUSE_API_GET(h, fuse_opt_free_args);
    CYGFUSE_API_GET(h, fuse_opt_add_opt);
    CYGFUSE_API_GET(h, fuse_opt_add_opt_escaped);
    CYGFUSE_API_GET(h, fuse_opt_match);

    return h;
}
