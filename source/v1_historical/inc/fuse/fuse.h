/**
 * @file fuse/fuse.h
 *
 * This file is derived from libfuse/include/fuse.h:
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

#ifndef FUSE_H_
#define FUSE_H_

#include "fuse_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fuse;

typedef int (*fuse_fill_dir_t)(void *buf, const char *name,
    const struct fuse_stat *stbuf, fuse_off_t off);
typedef struct fuse_dirhandle *fuse_dirh_t;
typedef int (*fuse_dirfil_t)(fuse_dirh_t h, const char *name,
    int type, fuse_ino_t ino);

struct fuse_operations
{
    int (*getattr)(const char *path, struct fuse_stat *stbuf);
    int (*getdir)(const char *path, fuse_dirh_t h, fuse_dirfil_t filler);
    int (*readlink)(const char *path, char *buf, size_t size);
    int (*mknod)(const char *path, fuse_mode_t mode, fuse_dev_t dev);
    int (*mkdir)(const char *path, fuse_mode_t mode);
    int (*unlink)(const char *path);
    int (*rmdir)(const char *path);
    int (*symlink)(const char *dstpath, const char *srcpath);
    int (*rename)(const char *oldpath, const char *newpath);
    int (*link)(const char *srcpath, const char *dstpath);
    int (*chmod)(const char *path, fuse_mode_t mode);
    int (*chown)(const char *path, fuse_uid_t uid, fuse_gid_t gid);
    int (*truncate)(const char *path, fuse_off_t size);
    int (*utime)(const char *path, struct fuse_utimbuf *timbuf);
    int (*open)(const char *path, struct fuse_file_info *fi);
    int (*read)(const char *path, char *buf, size_t size, fuse_off_t off,
        struct fuse_file_info *fi);
    int (*write)(const char *path, const char *buf, size_t size, fuse_off_t off,
        struct fuse_file_info *fi);
    int (*statfs)(const char *path, struct fuse_statvfs *stbuf);
    int (*flush)(const char *path, struct fuse_file_info *fi);
    int (*release)(const char *path, struct fuse_file_info *fi);
    int (*fsync)(const char *path, int datasync, struct fuse_file_info *fi);
    int (*setxattr)(const char *path, const char *name, const char *value, size_t size,
        int flags);
    int (*getxattr)(const char *path, const char *name, char *value, size_t size);
    int (*listxattr)(const char *path, char *namebuf, size_t size);
    int (*removexattr)(const char *path, const char *name);
    int (*opendir)(const char *path, struct fuse_file_info *fi);
    int (*readdir)(const char *path, void *buf, fuse_fill_dir_t filler, fuse_off_t off,
        struct fuse_file_info *fi);
    int (*releasedir)(const char *path, struct fuse_file_info *fi);
    int (*fsyncdir)(const char *path, int datasync, struct fuse_file_info *fi);
    void *(*init)(struct fuse_conn_info *conn);
    void (*destroy)(void *data);
    int (*access)(const char *path, int mask);
    int (*create)(const char *path, fuse_mode_t mode, struct fuse_file_info *fi);
    int (*ftruncate)(const char *path, fuse_off_t off, struct fuse_file_info *fi);
    int (*fgetattr)(const char *path, struct fuse_stat *stbuf, struct fuse_file_info *fi);
    int (*lock)(const char *path, struct fuse_file_info *fi, int cmd, struct fuse_flock *lock);
    int (*utimens)(const char *path, const struct fuse_timespec tv[2]);
    int (*bmap)(const char *path, size_t blocksize, uint64_t *idx);
    unsigned int flag_nullpath_ok:1;
    unsigned int flag_reserved:31;
    int (*ioctl)(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
        unsigned int flags, void *data);
    int (*poll)(const char *path, struct fuse_file_info *fi,
        struct fuse_pollhandle *ph, unsigned *reventsp);
};

struct fuse_context
{
    struct fuse *fuse;
    fuse_uid_t uid;
    fuse_gid_t gid;
    fuse_pid_t pid;
    void *private_data;
    fuse_mode_t umask;
};

#define fuse_main(argc, argv, ops, data)\
    fuse_main_real(argc, argv, ops, sizeof *(ops), data)

int fuse_main_real(int argc, char *argv[],
    const struct fuse_operations *ops, size_t opsize, void *data);
int fuse_is_lib_option(const char *opt);
struct fuse *fuse_new(struct fuse_chan *ch, struct fuse_args *args,
    const struct fuse_operations *ops, size_t opsize, void *data);
void fuse_destroy(struct fuse *f);
int fuse_loop(struct fuse *f);
int fuse_loop_mt(struct fuse *f);
void fuse_exit(struct fuse *f);
struct fuse_context *fuse_get_context(void);
int fuse_getgroups(int size, fuse_gid_t list[]);
int fuse_interrupted(void);
int fuse_invalidate(struct fuse *f, const char *path);
int fuse_notify_poll(struct fuse_pollhandle *ph);
struct fuse_session *fuse_get_session(struct fuse *f);

#ifdef __cplusplus
}
#endif

#endif
