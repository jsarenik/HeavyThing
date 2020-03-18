#!/bin/sh

trap "killall sshecho" QUIT INT
mkdir -p chroot/etc/ssh chroot/proc
cp sshecho chroot
mount -t proc proc chroot/proc
chroot ./chroot /sshecho
umount chroot/proc
