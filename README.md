# FatELF

The latest information about FatELF can be found at https://icculus.org/fatelf/

## What is this?

FatELF is a simple file format that allows you to package several ELF binaries
in one file. The most obvious use for this is supplying a single executable
that will run on different processors; a common scenario would be to pack
both an amd64 and an x86 binary into one file, and let the operating system
choose the correct one at runtime. FatELF can be used with both executable
files and shared libraries.

At this time, this documentation assumes a GNU/Linux system, but FatELF could
theoretically be of use to most modern Unix systems that use ELF binaries.
FatELF is of no use to Windows, as its EXE files are not ELF format. Mac OS X,
while it has a Unix heritage, does not use ELF either, but their Mach-O
format provides something similar to FatELF (Apple refers to this technology
as "Universal Binaries." It is incompatible with FatELF.) Systems like
FreeBSD and OpenSolaris use ELF binaries, and could theoretically be made
to support FatELF as well.


## Source code licensing information:

Patches are contributed under their original licenses. For example, the Linux
kernel patch is GPLv2, the binutils patch is GPLv3, and the glibc patch is
LGPL.

The command line utilities (fatelf-glue, etc) are public domain.


## Using FatELF as an end-user:

If you're an end-user that just wants to run a FatELF binary that someone
gave you, you have two options. If you want everything to run smoothly, so
you run the FatELF files like any other binary, you need to patch your
system if it doesn't already have FatELF support. It's possible your Linux
distribution already did this for you. If not, GNU/Linux systems will need
the patches for the Linux kernel and glibc. Once you have these in place,
if you just want to run FatELF binaries, you're done. Have fun!

If you want file(1) to recognize FatELF binaries instead of calling them
"data", you'll want that patch, too.

If you can't patch your system, you can use the command line tools to extract
the specific ELF file you need, and run that instead. See the "building the
command line tools" section, then try running this:

    fatelf-split my_fatelf_binary

If the FatELF file contained x86 and amd64 binaries, you'll end up with files
named "my_fatelf_binary-i386" and "my_fatelf_binary-x86_64". Use whichever
you need at this point like any other program. Another option is to extract
just the binary you need, like this...

    fatelf-extract my_elf_binary my_fatelf_binary x86_64

...this will extract the amd64 ELF binary from my_fatelf_binary, and place it
in the file "my_elf_binary".


## Using FatELF as a developer:

If you're a developer that wants to work with FatELF files, you will want to
have your system patched (Linux kernel, glibc), so that you can run FatELF
binaries directly. You may want to patch binutils and gdb, too, but this is
only necessary if you want to link directly against a FatELF library or
debug a FatELF binary/library. In many cases, you can get away
without this functionality. You'll want to get the command line tools
installed (see the "Building the command line tools" section).

The most common workflow is usually to build your software for all platforms,
separately, and then use fatelf-glue to create FatELF files out of the
separate ELF binaries. So long as the soname doesn't change, it is safe to
link against an ELF shared library and then later turn that shared library
into a FatELF file. The system loaders will still do the right thing.
Alternately, if you patched binutils, you can create the FatELF shared
library first, and then link against it as normal.

In many cases, you only care about FatELF when you are ready to ship your
binaries to the outside world, so it's possible to simply use fatelf-glue at
the end of the process and never patch a single thing on your development box.


## Using FatELF as a system/tool developer:

If you are bringing FatELF support to a new platform or development tool, you
should probably examine include/fatelf.h for the data structures, and read
fatelf-specification.txt. The format is not particularly complex. The only
requirement is that your platform support ELF binaries, as that is where
most of the complexity lies. FatELF is just a simple wrapper around
traditional ELF files. The existing command line tools will probably Just
Work on your platform out of the box.

Please drop Ryan a line at icculus@icculus.org if you add FatELF support to
your software, so he can post a link to it on the FatELF website.


## Building the command line tools:

You will need to have a system set up for compiling C code, and you will need
[CMake](https://www.cmake.org/). If you are looking for the latest and
greatest version of the tools, you'll need git to download them. If you're
installing from a source tarball, git isn't necessary.

If you're on a Debian or Ubuntu system, you can probably run this to prepare:

    sudo apt-get install build-essential cmake git

Once you are ready, you should get the source code. If downloading via git,
this command line should do:

    git clone https://github.com/icculus/fatelf

If you are using a source tarball, just extract it.

Now make a build directory, configure the build, and compile the sources:

    mkdir fatelf-build
    cd fatelf-build
    cmake ../fatelf
    make

You should end up with a pile of binaries, with names like "fatelf-glue" and
"fatelf-info", etc. If you want to install them system-wide, you can do so
now:

    sudo make install

(`sudo make install DESTDIR=/some/other/path` also works.)


## Using the command line tools:

Most of the tools expect command line arguments in some form like...

    fatelf-COMMAND OUTPUT INPUT TARGET

...where `COMMAND` is the tool to run, `OUTPUT` is the file you want to create
with the results of the operation, `INPUT` is the file you want to process,
and `TARGET` is the target platform. Please note that output files will
overwrite existing files of the same name without warning.

`TARGET` takes the form of either a specific record number in the form
"recordX" where X is the index of the record in the FatELF file ("record0" is
the first record, "record1" the next, etc), or a formal target name. Formal
names look like this: "x86_64:64bits:le:sysv:osabiver0" ... that's a real
mouthful! All of the fields in a target name are optional. The only
requirement is that the name be unique enough to only refer to a single
record in a given FatELF file. In most cases, you'll only need the machine
architecture; if you have a FatELF file with x86 and amd64 ELF binaries, your
target names can be, simply, "i386" and "x86_64". If you have two SPARC
binaries that are only separated by word size, you could do "32bits" and
"64bits", but if you added an amd64 binary to that FatELF file, you'd need
to do "sparc:64bits" to stay unique. Two PowerPC binaries, one for bigendian
mode and one for little endian, could say "le" or "be" to be unique. If you
want the full target name for a given record, fatelf-info will list them for
you.



The actual tools are:

    fatelf-glue OUTPUT INPUT1 INPUT2 [... INPUTn]

This takes the ELF binaries listed on the command line (as `INPUT*`), and
glues them together into a FatELF binary named `OUTPUT`. The files' ELF
headers are read to construct the proper FatELF data structures. It is an
error to try to glue two ELF binaries with the same target together, and
fatelf-glue will refuse to do so.


    fatelf-info INPUT

Report interesting information about FatELF file `INPUT`. This will list
all fields of all FatELF records, and the full formal target name for
each.


    fatelf-extract OUTPUT INPUT TARGET

Extract a copy of the ELF binary that matches `TARGET` from FatELF file `INPUT`,
and write it to `OUTPUT`. If `TARGET` is ambiguous, this operation fails.


    fatelf-remove OUTPUT INPUT TARGET

Remove the ELF binary that matches `TARGET` from FatELF file INPUT`,
and write a new FatELF file that lacks that ELF binary to `OUTPUT`.
If `TARGET` is ambiguous, this operation fails.


    fatelf-replace OUTPUT INPUT NEWELF

Replace an ELF binary in FatELF file `INPUT` with the one in file `NEWELF`,
and write a new FatELF file with the replacement made. This tool figures
out which binary to replace by reading the headers in `NEWELF`.


    fatelf-split INPUT

Split FatELF file `INPUT` into multiple ELF files, one per included target.
The files will be named `INPUT-targetname`, where `targetname` is a formal
target name in the shortest form possible that prevents ambiguity between
included targets. This can overwrite existing files in the same directory
as `INPUT` without warning, so use with caution. fatelf-extract can be a
safer alternative.


    fatelf-verify INPUT TARGET

Check if there is an ELF binary in FatELF file `INPUT` that matches the
target name `TARGET`, and set the process exit code appropriately. This
reports zero if `TARGET` was found, and non-zero if not. If `TARGET` is
ambiguous, this is considered an error, and non-zero is reported.


    fatelf-validate INPUT

Run several tests on FatELF file `INPUT` to make sure the data is consistent
and sane. This will return non-zero if there are problems detected, or
zero if the tool believes that the FatELF file is correctly formed. Please
note that this is meant to be a sanity check and debugging aid, but will
not detect most forms of file corruption, either intentional or accidental.


