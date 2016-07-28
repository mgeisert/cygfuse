# cygfuse - FUSE for Cygwin

The cygfuse project intends to provide FUSE (Filesystem in Userspace) services for Cygwin.
It is a glue library that interfaces with native Windows projects that provide user mode file system functionality.

## License

This project is licensed under the BSD. The full license can be found in the LICENSE.md file.

The original author of this project is Bill Zissimopoulos.
The current maintainer of this project is Mark Geisert.

## Build and Install

How to build:

    make cygport

How to install:

    tar -C/ -xaf fuse-2.8-4.x86_64/dist/fuse/fuse-2.8-4.tar.xz

How to uninstall:

    tar taf fuse-2.8-4.x86_64/dist/fuse/fuse-2.8-4.tar.xz | sed 's@.*@/&@' | xargs rm
