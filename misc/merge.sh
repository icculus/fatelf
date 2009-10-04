#!/bin/bash

if [ "x`id -u`" != "x0" ]; then
    echo "not root."
    exit 1
fi

set -x
set -e

rm -rf cmake-build
mkdir cmake-build
cd cmake-build

cmake -DCMAKE_BUILD_TYPE=Release ../..
make -j2
gcc -o iself -s -O3 ../iself.c

#time for feh in bin boot etc lib opt sbin usr var ; do find /$feh -type f -exec ./iself {} \; ; done |perl -w -pi -e 's/\A\///;' |grep -v "usr/lib32/" |sort |uniq > ./binaries-64
time for feh in bin boot etc lib opt sbin usr var ; do find /x86/$feh -type f -exec ./iself {} \; ; done |perl -w -pi -e 's/\A\/x86\///;' |grep -v "usr/lib64" |sort |uniq > ./binaries-32

for feh in `cat binaries-32` ; do
    mkdir -p --mode=0755 `dirname "/$feh"`
    if [ -f "/$feh" ]; then
        ./fatelf-glue tmp-fatelf "/$feh" "/x86/$feh"
        chmod --reference="/$feh" tmp-fatelf
        mv tmp-fatelf "/$feh"
    else
        cp -a "/x86/$feh" "/$feh"
    fi
done

# end of merge.sh ...

