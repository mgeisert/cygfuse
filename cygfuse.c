/**
 * @file cygfuse/cygfuse.c
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

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cygfuse-internal.h"


/*
 * FUSE API implementation.
 *
 * We use a "trampoline" idea where all FUSE API calls get forwarded to the
 * appropriate implementation through a function pointer. This function
 * pointer is set up to point to a default implementation when the program
 * first starts. The default implementation calls cygfuse_init and then goes
 * back and calls the corresponding function pointer again. The expectation
 * is that cygfuse_init has set up things properly so that the function
 * pointer now points to the real FUSE API implementation.
 *
 * Subsequent calls to a FUSE API will now see a pointer to the real FUSE API
 * implementation and will circumvent calls to cygfuse_init.
 */

/* CYGFUSE_API_IMPL* macros: they forward calls to a specific FUSE impl */
#define CYGFUSE_API_IMPL(RET, API, PARAMS, ARGS)\
    static RET dfl_ ## API PARAMS;\
    RET (*pfn_ ## API) PARAMS = dfl_ ## API;\
    static RET dfl_ ## API PARAMS\
    {\
        cygfuse_init(0);\
        if (dfl_ ## API == pfn_ ## API)\
            cygfuse_fail("cygfuse: %s FUSE API initialization failed.\n",\
                #API);\
        return pfn_ ## API ARGS;\
    }\
    __attribute__ ((visibility("default"))) RET API PARAMS { return pfn_ ## API ARGS; }

/* fuse_common.h */
CYGFUSE_API_IMPL(int, fuse_version,
    (void),
    ())
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
CYGFUSE_API_IMPL(void, fuse_pollhandle_destroy,
    (struct fuse_pollhandle *ph),
    (ph))
CYGFUSE_API_IMPL(int, fuse_daemonize,
    (int foreground),
    (foreground))
CYGFUSE_API_IMPL(int, fuse_set_signal_handlers,
    (struct fuse_session *se),
    (se))
CYGFUSE_API_IMPL(void, fuse_remove_signal_handlers,
    (struct fuse_session *se),
    (se))

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
CYGFUSE_API_IMPL(struct fuse_context *, fuse_get_context,
    (void),
    ())
CYGFUSE_API_IMPL(int, fuse_getgroups,
    (int size, fuse_gid_t list[]),
    (size, list))
CYGFUSE_API_IMPL(int, fuse_interrupted,
    (void),
    ())
CYGFUSE_API_IMPL(int, fuse_invalidate,
    (struct fuse *f, const char *path),
    (f, path))
CYGFUSE_API_IMPL(int, fuse_notify_poll,
    (struct fuse_pollhandle *ph),
    (ph))
CYGFUSE_API_IMPL(struct fuse_session *, fuse_get_session,
    (struct fuse *f),
    (f))

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


/*
 * Cygfuse init/fail.
 */

static pthread_mutex_t cygfuse_mutex = PTHREAD_MUTEX_INITIALIZER;
static void *cygfuse_handle = 0;
static char *fuse_provider = NULL;

struct provider_t {
    char    *name;
    void    *(*initializer)();
} providers[] = {
    {"WinFSP",  cygfuse_winfsp_init},
    {"Dokany",  cygfuse_dokany_init},
/* Add descriptions of supported FUSE providers above this line. */
};

static int num_providers = sizeof(providers) / sizeof(providers[0]);

void cygfuse_init(int force)
{
    /*
     * Expensive lock is ok, because cygfuse_init calls will be eliminated
     * soon by our "trampoline" code. This is only to protect against
     * concurrent initialization.
     */
    pthread_mutex_lock(&cygfuse_mutex);
    if (force || 0 == cygfuse_handle)
    {
        fuse_provider = getenv("CYGFUSE");
        if (!fuse_provider)
            cygfuse_fail("cygfuse: environment variable CYGFUSE is not set\n");

        for (int i = 0; i < num_providers; i++)
            if (0 == strncasecmp(fuse_provider,
                     providers[i].name, sizeof(providers[i].name)))
            {
                cygfuse_handle = providers[i].initializer();
                break;
            }

        if (0 == cygfuse_handle)
            cygfuse_fail("cygfuse: %s FUSE DLL initialization failed.\n",
                fuse_provider);
    }
    pthread_mutex_unlock(&cygfuse_mutex);
}

void cygfuse_fail(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}
