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

#define CYGFUSE_API_IMPL(RET, API, PARAMS, ARGS)\
    static RET dfl_ ## API PARAMS;\
    RET (*pfn_ ## API) PARAMS = dfl_ ## API;\
    static RET dfl_ ## API PARAMS { cygfuse_init(0); return pfn_ ## API ARGS; }\
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

static pthread_mutex_t cygfuse_mutex = PTHREAD_MUTEX_INITIALIZER;
static void *cygfuse_handle = 0;
static char *fuse_variant = NULL;

void cygfuse_init(int force)
{
    fuse_variant = getenv("CYGFUSE");
    if (!fuse_variant)
        cygfuse_fail("cygfuse: environment variable CYGFUSE is not set\n");

    pthread_mutex_lock(&cygfuse_mutex);
    if (force || 0 == cygfuse_handle)
    {
        /* Add call to additional FUSE implementation initializers here. */
        if (0 == strncasecmp(fuse_variant, WINFSP, strlen(WINFSP)))
            cygfuse_handle = cygfuse_winfsp_init();
        else if (0 == strncasecmp(fuse_variant, DOKANY, strlen(DOKANY)))
            cygfuse_handle = cygfuse_dokany_init();

        if (0 == cygfuse_handle)
            cygfuse_fail("cygfuse: %s FUSE DLL initialization failed.\n",
                fuse_variant);
    }
    pthread_mutex_unlock(&cygfuse_mutex);
}

void cygfuse_fail(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
