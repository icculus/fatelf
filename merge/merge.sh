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

mkdir -p /x86_64
mkdir -p /x86

umount /x86_64 || echo "ignore any umount errors"
umount /x86 || echo "ignore any umount errors"

mount -t ext3 /dev/sda1 /x86_64
mount -t ext3 /dev/sdb1 /x86

# Special case: it's a symlink to /lib32, so it causes an endless loop.
rm -f /x86_64/lib/ld-linux.so.2
ln -s ld-2.9.so /x86_64/lib/ld-linux.so.2

# other issues.
if [ -d /x86/lib/udev/devices ] ; then
    mv /x86/lib/udev/devices /x86/tmp_devices
fi

if [ -d /x86/usr/bin/X11 ]; then
    mv /x86/usr/bin/X11 /x86/tmp_X11
fi

# Make sure the home directory is gone.
rm -rf /x86_64/home/fatelf
cp -av /x86_64/etc/skel /x86_64/home/fatelf
chown -R 1000 /x86_64/home/fatelf

gcc -o fatelf-validate -O3 -s -I../../include -I../../utils ../../utils/fatelf-validate.c ../../utils/fatelf-utils.c
gcc -o fatelf-replace -O3 -s -I../../include -I../../utils ../../utils/fatelf-replace.c ../../utils/fatelf-utils.c
gcc -o fatelf-glue -O3 -s -I../../include -I../../utils ../../utils/fatelf-glue.c ../../utils/fatelf-utils.c
gcc -o iself -s -O3 ../iself.c
gcc -o is32bitelf -s -O3 ../is32bitelf.c

if [ ! -f ./binaries-32 ]; then
    time for feh in bin boot etc lib opt sbin usr/bin usr/games usr/sbin usr/X11R6 usr/lib usr/local var/lib ; do find /x86/$feh -follow -type f -exec ./iself {} \; ; done |perl -w -pi -e 's/\A\/x86\///;' |sort |uniq > ./binaries-32
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

# We don't need /lib32 and /lib64, but symlink them to /lib just in case.
rm -rf /x86_64/lib32
rm -rf /x86_64/lib64
ln -s /lib /x86_64/lib32
ln -s /lib /x86_64/lib64

# Set up the boot bits.
mkdir -p /x86_64/boot/x86_64
mkdir -p /x86_64/boot/x86
mv /x86_64/boot/*-generic /x86_64/boot/x86_64/
cp -av /x86/boot/*-generic /x86_64/boot/x86/
cp ../grubmenu.txt /x86_64/boot/grub/menu.lst

# Set the hostname.
echo "fatelf" > /x86_64/etc/hostname

# Hack: force hald to regenerate cache on each run, since it writes a size_t
#  in there that causes crashes when you switch between 64 and 32 bit mode.
echo "rm -f /var/cache/hald/fdi-cache" >> /x86_64/etc/default/hal

# Put some things back.
mv /x86/tmp_devices /x86/lib/udev/devices
mv /x86/tmp_X11 /x86/usr/bin/X11

umount /x86
umount /x86_64

# We disable fsck intervals...this is a demo, after all!
set +e
time fsck.ext3 -D -C 0 -f /dev/sda1
tune2fs -c 0 /dev/sda1
tune2fs -i 0 /dev/sda1

# end of merge.sh ...

