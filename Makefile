Version = $(shell sed -n '/^VERSION=/s/VERSION=\(.*\)/\1/p' cygfuse.cygport)
IncDir = ./inc/fuse
Debug = -g

cygfuse-$(Version).dll libfuse-$(Version).dll.a cygfuse.pc: cygfuse.c cygfuse.pc.in
	gcc $(Debug) -shared -o cygfuse-$(Version).dll -Wl,--out-implib=libfuse-$(Version).dll.a -I$(IncDir) cygfuse.c
	sed "s/@Version@/$(Version)/g" cygfuse.pc.in > cygfuse.pc

cygfuse-test: cygfuse-test.c cygfuse-$(Version).dll libfuse-$(Version).dll.a
	gcc $(Debug) -o cygfuse-test -I$(IncDir) -DCYGFUSE cygfuse-test.c -L$(PWD) -lfuse-$(Version)

cygport:
	git clean -dfx
	(\
		cd `git rev-parse --show-toplevel` &&\
		Stash=`git stash create` &&\
		git archive --prefix=cygfuse-work/ --format=tar.gz $${Stash:-HEAD}\
			> cygfuse-work.tar.gz\
	)
	CYGPORT_SRC_URI=cygfuse-work.tar.gz CYGPORT_SRC_DIR=cygfuse-work cygport cygfuse.cygport download prep compile install package

clean:
	rm -f cygfuse-$(Version).dll cygfuse.pc libfuse-$(Version).dll.a
	rm -f cygfuse-test.exe* cygfuse-work.tar.gz