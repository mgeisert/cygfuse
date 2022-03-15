# cygfuse - Cygwin interface to Windows FUSE providers

Filesystem in Userspace (FUSE) is a simple interface for user
programs to export a virtual filesystem to Cygwin. It also aims
to provide a secure method for non-privileged users to create
and mount their own filesystem implementations.

**cygfuse** supplies an emulation of Linux libfuse/libfuse3. It
allows FUSE clients such as sshfs, memfs, ftpfs, etc to
make use of certain FUSE providers implemented for Windows.

**cygfuse** was originally a Cygwin add-on to **WinFSP** provided by
the author of **WinFSP**, Bill Zissimopoulos. It is still available
in that form from _source/v1_historical_ or any **WinFSP** release.
There will soon be a standard Cygwin package of **cygfuse**
available via Cygwin's setup.exe.

At this time **WinFSP** is the only Windows FUSE provider supported.
Information about **WinFSP** can be found at _http://www.secfs.net/winfsp_.


## License

GPLv3. See the file License.txt.


## Build and Install

[Details to be supplied.] In the meantime, see the file
_source/v1_historical/README.md_ which is dated but has the essentials.
