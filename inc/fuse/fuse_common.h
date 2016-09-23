/**
 * @file fuse/fuse_common.h
 * WinFsp FUSE compatible API.
 *
 * This file is derived from libfuse/include/fuse_common.h:
 *     FUSE: Filesystem in Userspace
 *     Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
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

#ifndef FUSE_COMMON_H_
#define FUSE_COMMON_H_

#include "winfsp_fuse.h"
#include "fuse_opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FUSE_MAJOR_VERSION              2
#define FUSE_MINOR_VERSION              8
#define FUSE_MAKE_VERSION(maj, min)     ((maj) * 10 + (min))
#define FUSE_VERSION                    FUSE_MAKE_VERSION(FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION)

#define FUSE_CAP_ASYNC_READ             (1 << 0)
#define FUSE_CAP_POSIX_LOCKS            (1 << 1)
#define FUSE_CAP_ATOMIC_O_TRUNC         (1 << 3)
#define FUSE_CAP_EXPORT_SUPPORT         (1 << 4)
#define FUSE_CAP_BIG_WRITES             (1 << 5)
#define FUSE_CAP_DONT_MASK              (1 << 6)

#define FUSE_IOCTL_COMPAT               (1 << 0)
#define FUSE_IOCTL_UNRESTRICTED         (1 << 1)
#define FUSE_IOCTL_RETRY                (1 << 2)
#define FUSE_IOCTL_MAX_IOV              256

struct fuse_file_info
{
    int flags;
    unsigned int fh_old;
    int writepage;
    unsigned int direct_io:1;
    unsigned int keep_cache:1;
    unsigned int flush:1;
    unsigned int nonseekable:1;
    unsigned int padding:28;
    uint64_t fh;
    uint64_t lock_owner;
};

struct fuse_conn_info
{
    unsigned proto_major;
    unsigned proto_minor;
    unsigned async_read;
    unsigned max_write;
    unsigned max_readahead;
    unsigned capable;
    unsigned want;
    unsigned reserved[25];
};

struct fuse_session;
struct fuse_chan;
struct fuse_pollhandle;

int fuse_version(void);
struct fuse_chan *fuse_mount(const char *mountpoint, struct fuse_args *args);
void fuse_unmount(const char *mountpoint, struct fuse_chan *ch);
int fuse_parse_cmdline(struct fuse_args *args,
    char **mountpoint, int *multithreaded, int *foreground);
void fuse_pollhandle_destroy(struct fuse_pollhandle *ph);
int fuse_daemonize(int foreground);
int fuse_set_signal_handlers(struct fuse_session *se);
void fuse_remove_signal_handlers(struct fuse_session *se);

#ifdef __cplusplus
}
#endif

#endif
