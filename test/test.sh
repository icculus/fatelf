#!/bin/sh

set -e
set -x

rm -rf cmake-build
mkdir cmake-build
cd cmake-build
cmake ../..
make -j2

gcc -o hello.so ../hello-lib.c -shared -fPIC -m32
gcc -o hello-x86 ../hello.c hello.so -m32 -Wl,-rpath,.
mv hello.so hello-x86.so

gcc -o hello.so ../hello-lib.c -shared -fPIC -m64
gcc -o hello-amd64 ../hello.c hello.so -m64 -Wl,-rpath,.
mv hello.so hello-amd64.so

./fatelf-glue hello hello-x86 hello-amd64
./fatelf-glue hello.so hello-amd64.so hello-x86.so

file ./hello
./fatelf-info ./hello

file ./hello.so
./fatelf-info ./hello.so

ldd ./hello
ldd ./hello.so

./hello

# end of test.sh ...


