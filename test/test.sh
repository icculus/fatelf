#!/bin/sh

set -e
set -x

rm -rf cmake-build
mkdir cmake-build
cd cmake-build
cmake ../..
make -j2

gcc -O0 -ggdb3 -o hello.so ../hello-lib.c -shared -fPIC -m32
gcc -O0 -ggdb3 -o hello-x86 ../hello.c hello.so -m32 -Wl,-rpath,.
gcc -O0 -ggdb3 -o hello-dlopen-x86 ../hello-dlopen.c -ldl -m32
mv hello.so hello-x86.so

gcc -O0 -ggdb3 -o hello.so ../hello-lib.c -shared -fPIC -m64
gcc -O0 -ggdb3 -o hello-amd64 ../hello.c hello.so -m64 -Wl,-rpath,.
gcc -O0 -ggdb3 -o hello-dlopen-amd64 ../hello-dlopen.c -ldl -m64
mv hello.so hello-amd64.so

./fatelf-glue hello hello-x86 hello-amd64
./fatelf-glue hello.so hello-amd64.so hello-x86.so
./fatelf-glue hello-dlopen hello-dlopen-x86 hello-dlopen-amd64

file ./hello
./fatelf-info ./hello

file ./hello.so
./fatelf-info ./hello.so

file ./hello-dlopen
./fatelf-info ./hello-dlopen

ldd ./hello
ldd ./hello.so
ldd ./hello-dlopen

./hello
./hello-dlopen

# end of test.sh ...


