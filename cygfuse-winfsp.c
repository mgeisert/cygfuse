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

#define REMOVE_PARENS(...)              __VA_ARGS__
#define CYGFUSE_API_IMPL_DEF(RET, API, PARAMS, ...)\
    static RET fsp_ ## API PARAMS __VA_ARGS__\
    extern RET (*pfn_ ## API) PARAMS;
#define CYGFUSE_API_IMPL(RET, API, PARAMS, ARGS)\
    static RET (*pfn_fsp_ ## API) (struct fsp_fuse_env *env, REMOVE_PARENS PARAMS);\
    CYGFUSE_API_IMPL_DEF(RET, API, PARAMS, { return pfn_fsp_ ## API (fsp_fuse_env(), REMOVE_PARENS ARGS); })
#define CYGFUSE_API_IMPL_VOID(RET, API)\
    static RET (*pfn_fsp_ ## API) (struct fsp_fuse_env *env);\
    CYGFUSE_API_IMPL_DEF(RET, API, (void), { return pfn_fsp_ ## API (fsp_fuse_env()); })

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
        pfn_ ## n = fsp_ ## n;
#define CYGFUSE_API_GET_NS(n)           \
    pfn_ ## n = fsp_ ## n;

#if 0
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
 */
static inline int cygfuse_daemon(int nochdir, int noclose)
{
    if (-1 == daemon(nochdir, noclose))
        return -1;

    /* force reload of FUSE implementation DLL to workaround fork() problems */
    cygfuse_init(1);

    return 0;
}
#define daemon                          cygfuse_daemon

#define FSP_FUSE_API                    static
#define FSP_FUSE_API_NAME(api)          (* pfn_ ## api)
#define FSP_FUSE_API_CALL(api)          (cygfuse_init(0), pfn_ ## api)
#define FSP_FUSE_SYM(proto, ...)        \
    __attribute__ ((visibility("default"))) proto { __VA_ARGS__ }
#endif

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

    /* winfsp_fuse.h */
    //CYGFUSE_API_GET(h, fsp_fuse_signal_handler);

    /* fuse_common.h */
    //CYGFUSE_API_GET(h, fsp_fuse_version);
    //CYGFUSE_API_GET(h, fsp_fuse_mount);
    //CYGFUSE_API_GET(h, fsp_fuse_unmount);
    //CYGFUSE_API_GET(h, fsp_fuse_parse_cmdline);
    //CYGFUSE_API_GET(h, fsp_fuse_ntstatus_from_errno);

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
    //CYGFUSE_API_GET(h, fsp_fuse_opt_parse);
    //CYGFUSE_API_GET(h, fsp_fuse_opt_add_arg);
    //CYGFUSE_API_GET(h, fsp_fuse_opt_insert_arg);
    //CYGFUSE_API_GET(h, fsp_fuse_opt_free_args);
    //CYGFUSE_API_GET(h, fsp_fuse_opt_add_opt);
    //CYGFUSE_API_GET(h, fsp_fuse_opt_add_opt_escaped);
    //CYGFUSE_API_GET(h, fsp_fuse_opt_match);

    return h;
}
