/**
 * @file fuse/fuse_opt.h
 *
 * This file is derived from libfuse/include/fuse_opt.h:
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

#ifndef FUSE_OPT_H_
#define FUSE_OPT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FUSE_OPT_KEY(templ, key)        { templ, -1, key }
#define FUSE_OPT_END                    { NULL, 0, 0 }

#define FUSE_OPT_KEY_OPT                -1
#define FUSE_OPT_KEY_NONOPT             -2
#define FUSE_OPT_KEY_KEEP               -3
#define FUSE_OPT_KEY_DISCARD            -4

#define FUSE_ARGS_INIT(argc, argv)      { argc, argv, 0 }

struct fuse_opt
{
	const char *templ;
	unsigned int offset;
	int value;
};

struct fuse_args
{
	int argc;
	char **argv;
	int allocated;
};

typedef int (*fuse_opt_proc_t)(void *data, const char *arg, int key,
    struct fuse_args *outargs);

int fuse_opt_parse(struct fuse_args *args, void *data,
    const struct fuse_opt opts[], fuse_opt_proc_t proc);
int fuse_opt_add_arg(struct fuse_args *args, const char *arg);
int fuse_opt_insert_arg(struct fuse_args *args, int pos, const char *arg);
void fuse_opt_free_args(struct fuse_args *args);
int fuse_opt_add_opt(char **opts, const char *opt);
int fuse_opt_add_opt_escaped(char **opts, const char *opt);
int fuse_opt_match(const struct fuse_opt opts[], const char *opt);

#ifdef __cplusplus
}
#endif

#endif
