#!/bin/sh

set -e
set -x

rm -rf cmake-build
mkdir cmake-build
cd cmake-build
cmake ../..
make -j2

# Build some test programs (hello world, hello world with shared library,
#  hello world that uses dlopen() on shared library).
gcc -O0 -ggdb3 -o hello.so ../hello-lib.c -shared -fPIC -m32
gcc -O0 -ggdb3 -o hello-x86 ../hello.c hello.so -m32 -Wl,-rpath,.
gcc -O0 -ggdb3 -o hello-dlopen-x86 ../hello-dlopen.c -ldl -m32
mv hello.so hello-x86.so

gcc -O0 -ggdb3 -o hello.so ../hello-lib.c -shared -fPIC -m64
gcc -O0 -ggdb3 -o hello-amd64 ../hello.c hello.so -m64 -Wl,-rpath,.
gcc -O0 -ggdb3 -o hello-dlopen-amd64 ../hello-dlopen.c -ldl -m64
mv hello.so hello-amd64.so

# Glue them into FatELF binaries.
./fatelf-glue hello hello-x86 hello-amd64
./fatelf-glue hello.so hello-amd64.so hello-x86.so
./fatelf-glue hello-dlopen hello-dlopen-x86 hello-dlopen-amd64

# fatelf-info tests.
./fatelf-info ./hello
./fatelf-info ./hello.so
./fatelf-info ./hello-dlopen

# file(1) tests.
file ./hello
file ./hello.so
file ./hello-dlopen

# glibc tests.
ldd ./hello
ldd ./hello.so
ldd ./hello-dlopen

# binutils tests.
size ./hello
size ./hello.so
size ./hello-dlopen
nm ./hello
nm ./hello.so
nm ./hello-dlopen
objdump -x ./hello
objdump -x ./hello.so
objdump -x ./hello-dlopen

# Link directly against a FatELF shared library (binutils test).
gcc -m64 -O0 -ggdb3 -o hello-fatlink-amd64 ../hello.c hello.so -Wl,-rpath,.
gcc -m32 -O0 -ggdb3 -o hello-fatlink-x86 ../hello.c hello.so -Wl,-rpath,.

# and, you know...run the programs themselves (glibc, kernel tests).
./hello
./hello-dlopen
./hello-fatlink-amd64
./hello-fatlink-x86

# end of test.sh ...

