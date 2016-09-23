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

#include "cygfuse-internal.h"

#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <sys/cygwin.h>

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

void *cygfuse_init_winfsp()
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
            return cygfuse_init_fail();

        bytes = read(regfd, winpath,
                     sizeof winpath - sizeof CYGFUSE_WINFSP_PATH);
        close(regfd);
        if (-1 == bytes || 0 == bytes)
            return cygfuse_init_fail();

        if ('\0' == winpath[bytes - 1])
            bytes--;
        memcpy(winpath + bytes,
               CYGFUSE_WINFSP_PATH, sizeof CYGFUSE_WINFSP_PATH);

        psxpath = (char *)
            cygwin_create_path(CCP_WIN_A_TO_POSIX | CCP_PROC_CYGDRIVE, winpath);
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
