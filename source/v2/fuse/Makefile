VERSION=2.8
CFLAGS=-g -Wall

.PHONY: all test
all: cygfuse-$(VERSION).dll fuse.pc
test: cygfuse-test.exe

cygfuse-$(VERSION).dll: cygfuse.c
	gcc $(CFLAGS) \
		-shared -o cygfuse-$(VERSION).dll \
		-Wl,--out-implib=libfuse-$(VERSION).dll.a \
		-I. \
		cygfuse.c
	cp -p cygfuse-$(VERSION).dll cygfuse-$(VERSION).dll.dbg

fuse.pc: fuse.pc.in
	sed "s/@VERSION@/$(VERSION)/g" fuse.pc.in > fuse.pc

cygfuse-test.exe: cygfuse-test.c cygfuse-$(VERSION).dll
	gcc $(CFLAGS) \
		-o cygfuse-test.exe \
		-I. \
		-DCYGFUSE \
		cygfuse-test.c \
		-L. -lfuse-$(VERSION)
	cp -p cygfuse-test.exe cygfuse-test.exe.dbg

clean:
	rm -f *.dll *.dll.a *.pc *.exe
