Version = $(shell sed -n '/^VERSION=/s/VERSION=\(.*\)/\1/p' fuse.cygport)
IncDir = ./inc/fuse
#Debug = -g

cygfuse-$(Version).dll libfuse-$(Version).dll.a fuse.pc: cygfuse.c fuse.pc.in
	gcc $(Debug) -shared -o cygfuse-$(Version).dll -Wl,--out-implib=libfuse-$(Version).dll.a -I$(IncDir) cygfuse.c
	[ -n "$(Debug)" ] || strip cygfuse-$(Version).dll
	sed "s/@Version@/$(Version)/g" fuse.pc.in > fuse.pc

cygfuse-test.exe: cygfuse-test.c cygfuse-$(Version).dll libfuse-$(Version).dll.a
	gcc $(Debug) -o cygfuse-test.exe -I$(IncDir) -DCYGFUSE cygfuse-test.c -L$(PWD) -lfuse-$(Version)

cygport:
	git clean -dfx
	(\
		cd `git rev-parse --show-toplevel` &&\
		Stash=`git stash create` &&\
		git archive --prefix=cygfuse-work/ --format=tar.gz $${Stash:-HEAD}\
			> cygfuse-work.tar.gz\
	)
	CYGPORT_SRC_URI=cygfuse-work.tar.gz CYGPORT_SRC_DIR=cygfuse-work cygport fuse.cygport download prep compile install package
