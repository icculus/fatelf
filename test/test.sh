#!/bin/bash

AMD64=0
if [ `uname -m` = "x86_64" ]; then
    AMD64=1
    echo "This is an x86_64 system."
else
    echo "This is NOT an x86_64 system."
fi

set -e
set -x

rm -rf cmake-build
mkdir cmake-build
cd cmake-build
cmake ../..
make -j2

# Build some test programs (hello world, hello world with shared library,
#  hello world that uses dlopen() on shared library).
gcc --std=c99 -O0 -ggdb3 -o hello.so ../hello-lib.c -shared -fPIC -m32
gcc --std=c99 -O0 -ggdb3 -c -o hello-x86.o ../hello.c -m32 -Wl,-rpath,.
gcc --std=c99 -O0 -ggdb3 -o hello-x86 hello-x86.o hello.so -m32 -Wl,-rpath,.
gcc --std=c99 -O0 -ggdb3 -o hello-dlopen-x86 ../hello-dlopen.c -ldl -m32
mv hello.so hello-x86.so

gcc --std=c99 -O0 -ggdb3 -o hello.so ../hello-lib.c -shared -fPIC -m64
gcc --std=c99 -O0 -ggdb3 -c -o hello-amd64.o ../hello.c -m64 -Wl,-rpath,.
gcc --std=c99 -O0 -ggdb3 -o hello-amd64 hello-amd64.o hello.so -m64 -Wl,-rpath,.
gcc --std=c99 -O0 -ggdb3 -o hello-dlopen-amd64 ../hello-dlopen.c -ldl -m64
mv hello.so hello-amd64.so

# Glue them into FatELF binaries.
./fatelf-glue hello hello-x86 hello-amd64
./fatelf-glue hello.o hello-x86.o hello-amd64.o
./fatelf-glue hello.so hello-amd64.so hello-x86.so
./fatelf-glue hello-dlopen hello-dlopen-x86 hello-dlopen-amd64

# fatelf-info tests.
./fatelf-info ./hello
./fatelf-info ./hello.o
./fatelf-info ./hello.so
./fatelf-info ./hello-dlopen

# fatelf-extract tests
./fatelf-extract ./extract-x86 ./hello record0
diff --brief ./hello-x86 ./extract-x86
./fatelf-extract ./extract-amd64 ./hello x86_64:sysv:osabiver0:le:64bit
diff --brief ./hello-amd64 ./extract-amd64

# file(1) tests.
file ./hello
file ./hello.o
file ./hello.so
file ./hello-dlopen

# glibc tests.
ldd ./hello
ldd ./hello.so
ldd ./hello-dlopen

# binutils tests.
size ./hello
size ./hello.o
size ./hello.so
size ./hello-dlopen
nm ./hello
nm ./hello.o
nm ./hello.so
nm ./hello-dlopen
objdump -x ./hello
objdump -x ./hello.o
objdump -x ./hello.so
objdump -x ./hello-dlopen

# Test gdb.
gdb -x ../test.gdb ./hello

# Link directly against a FatELF object file (binutils test).
gcc --std=c99 -m64 -O0 -ggdb3 -o hello-fatlink-obj-amd64 hello.o hello.so -Wl,-rpath,.
gcc --std=c99 -m32 -O0 -ggdb3 -o hello-fatlink-obj-x86 hello.o hello.so -Wl,-rpath,.

# Link directly against a FatELF shared library (binutils test).
gcc --std=c99 -m64 -O0 -ggdb3 -o hello-fatlink-amd64 ../hello.c hello.so -Wl,-rpath,.
gcc --std=c99 -m32 -O0 -ggdb3 -o hello-fatlink-x86 ../hello.c hello.so -Wl,-rpath,.

# and, you know...run the programs themselves (glibc, kernel tests).
./hello
./hello-dlopen
[ "x$AMD64" = "x1" ] && ./hello-fatlink-amd64
[ "x$AMD64" = "x1" ] && ./hello-fatlink-obj-amd64
./hello-fatlink-x86
./hello-fatlink-obj-x86

# end of test.sh ...


