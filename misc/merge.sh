#!/bin/bash

if [ "x`id -u`" != "x0" ]; then
    echo "not root."
    exit 1
fi

set -x
set -e

rm -rf cmake-build
mkdir -p cmake-build
cd cmake-build

# Special case: it's a symlink to /lib32, so it causes an endless loop.
rm -f /x86_64/lib/ld-linux.so.2
ln -s ld-2.9.so /x86_64/lib/ld-linux.so.2

cmake -DCMAKE_BUILD_TYPE=Release ../..
make -j2
gcc -o iself -s -O3 ../iself.c
gcc -o is32bitelf -s -O3 ../is32bitelf.c

if [ ! -f ./binaries-32 ]; then
    time for feh in bin boot etc lib opt sbin usr/bin usr/games usr/sbin usr/X11R6 usr/lib usr/local var/lib ; do find /x86/$feh -type f -exec ./iself {} \; ; done |perl -w -pi -e 's/\A\/x86\///;' |grep -v "usr/lib64" |sort |uniq > ./binaries-32
fi

for feh in `cat binaries-32` ; do
    mkdir -p --mode=0755 `dirname "/x86_64/$feh"`
    if [ ! -f "/x86_64/$feh" ]; then
        cp -a "/x86/$feh" "/x86_64/$feh"
    else
        ISFATELF=0
        ./fatelf-validate "/x86_64/$feh" && ISFATELF=1
        if [ "x$ISFATELF" = "x1" ]; then
            ./fatelf-replace tmp-fatelf "/x86_64/$feh" "/x86/$feh"
            chmod --reference="/x86_64/$feh" tmp-fatelf
            mv tmp-fatelf "/x86_64/$feh"
        else
            SRCIS32BIT=0
            DSTIS32BIT=0
            ./is32bitelf "/x86/$feh" && SRCIS32BIT=1
            ./is32bitelf "/x86_64/$feh" && DSTIS32BIT=1
            if [ "x$SRCIS32BIT" != "x$DSTIS32BIT" ]; then
                ./fatelf-glue tmp-fatelf "/x86_64/$feh" "/x86/$feh"
                chmod --reference="/x86_64/$feh" tmp-fatelf
                mv tmp-fatelf "/x86_64/$feh"
            fi
        fi
    fi
done

rm -rf /x86_64/lib32
rm -rf /x86_64/lib64
ln -s /lib /x86_64/lib32
ln -s /lib /x86_64/lib64

# end of merge.sh ...

