/**
 * @file fuse/winfsp_fuse.h
 * WinFsp FUSE compatible API.
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

#ifndef FUSE_WINFSP_FUSE_H_INCLUDED
#define FUSE_WINFSP_FUSE_H_INCLUDED

#include <errno.h>
#include <stdint.h>
#if !defined(WINFSP_DLL_INTERNAL)
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(FSP_FUSE_API)
#if defined(WINFSP_DLL_INTERNAL)
#define FSP_FUSE_API                    __declspec(dllexport)
#else
#define FSP_FUSE_API                    __declspec(dllimport)
#endif
#endif

#if !defined(FSP_FUSE_API_NAME)
#define FSP_FUSE_API_NAME(n)            (n)
#endif

#if !defined(FSP_FUSE_API_CALL)
#define FSP_FUSE_API_CALL(n)            (n)
#endif

#if !defined(FSP_FUSE_SYM)
#if !defined(CYGFUSE)
#define FSP_FUSE_SYM(proto, ...)        static inline proto { __VA_ARGS__ }
#else
#define FSP_FUSE_SYM(proto, ...)        proto;
#endif
#endif

/*
 * FUSE uses a number of types (notably: struct stat) that are OS specific.
 * Furthermore there are sometimes multiple definitions of the same type even
 * within the same OS. This is certainly true on Windows, where these types
 * are not even native.
 *
 * For this reason we will define our own fuse_* types which represent the
 * types as the WinFsp DLL expects to see them. We will define these types
 * to be compatible with the equivalent Cygwin types as we want WinFsp-FUSE
 * to be usable from Cygwin.
 */

#if defined(_WIN64) || defined(_WIN32)

typedef uint32_t fuse_uid_t;
typedef uint32_t fuse_gid_t;
typedef int32_t fuse_pid_t;

typedef uint32_t fuse_dev_t;
typedef uint64_t fuse_ino_t;
typedef uint32_t fuse_mode_t;
typedef uint16_t fuse_nlink_t;
typedef int64_t fuse_off_t;

#if defined(_WIN64)
typedef uint64_t fuse_fsblkcnt_t;
typedef uint64_t fuse_fsfilcnt_t;
#else
typedef uint32_t fuse_fsblkcnt_t;
typedef uint32_t fuse_fsfilcnt_t;
#endif
typedef int32_t fuse_blksize_t;
typedef int64_t fuse_blkcnt_t;

#if defined(_WIN64)
struct fuse_utimbuf
{
    int64_t actime;
    int64_t modtime;
};
struct fuse_timespec
{
    int64_t tv_sec;
    int64_t tv_nsec;
};
#else
struct fuse_utimbuf
{
    int32_t actime;
    int32_t modtime;
};
struct fuse_timespec
{
    int32_t tv_sec;
    int32_t tv_nsec;
};
#endif

struct fuse_stat
{
    fuse_dev_t st_dev;
    fuse_ino_t st_ino;
    fuse_mode_t st_mode;
    fuse_nlink_t st_nlink;
    fuse_uid_t st_uid;
    fuse_gid_t st_gid;
    fuse_dev_t st_rdev;
    fuse_off_t st_size;
    struct fuse_timespec st_atim;
    struct fuse_timespec st_mtim;
    struct fuse_timespec st_ctim;
    fuse_blksize_t st_blksize;
    fuse_blkcnt_t st_blocks;
    struct fuse_timespec st_birthtim;
};

#if defined(_WIN64)
struct fuse_statvfs
{
    uint64_t f_bsize;
    uint64_t f_frsize;
    fuse_fsblkcnt_t f_blocks;
    fuse_fsblkcnt_t f_bfree;
    fuse_fsblkcnt_t f_bavail;
    fuse_fsfilcnt_t f_files;
    fuse_fsfilcnt_t f_ffree;
    fuse_fsfilcnt_t f_favail;
    uint64_t f_fsid;
    uint64_t f_flag;
    uint64_t f_namemax;
};
#else
struct fuse_statvfs
{
    uint32_t f_bsize;
    uint32_t f_frsize;
    fuse_fsblkcnt_t f_blocks;
    fuse_fsblkcnt_t f_bfree;
    fuse_fsblkcnt_t f_bavail;
    fuse_fsfilcnt_t f_files;
    fuse_fsfilcnt_t f_ffree;
    fuse_fsfilcnt_t f_favail;
    uint32_t f_fsid;
    uint32_t f_flag;
    uint32_t f_namemax;
};
#endif

struct fuse_flock
{
    int16_t l_type;
    int16_t l_whence;
    fuse_off_t l_start;
    fuse_off_t l_len;
    fuse_pid_t l_pid;
};

#elif defined(__CYGWIN__)

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <utime.h>

#define fuse_uid_t                      uid_t
#define fuse_gid_t                      gid_t
#define fuse_pid_t                      pid_t

#define fuse_dev_t                      dev_t
#define fuse_ino_t                      ino_t
#define fuse_mode_t                     mode_t
#define fuse_nlink_t                    nlink_t
#define fuse_off_t                      off_t

#define fuse_fsblkcnt_t                 fsblkcnt_t
#define fuse_fsfilcnt_t                 fsfilcnt_t
#define fuse_blksize_t                  blksize_t
#define fuse_blkcnt_t                   blkcnt_t

#define fuse_utimbuf                    utimbuf
#define fuse_timespec                   timespec

#define fuse_stat                       stat
#define fuse_statvfs                    statvfs
#define fuse_flock                      flock

/*
 * Note that long is 8 bytes long in Cygwin64 and 4 bytes long in Win64.
 * For this reason we avoid using long anywhere in these headers.
 */

#else
#error unsupported environment
#endif

#ifdef __cplusplus
}
#endif

#endif
