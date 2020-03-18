#!/bin/sh

a="/$0"; a=${a%/*}; a=${a:-.}; a=${a#/}/; BINDIR=$(cd $a; pwd)
a="/$PWD/"; a=${a%/*}; a=${a:-.}; a=${a#/}/; CURDIR=$(cd $a; pwd)
BIN=${1:-"${CURDIR##*/}"}

mkdir -p chroot/etc/ssh chroot/proc
cp $BIN chroot
test -r chroot/etc/ssh/ssh_host_rsa_key || $BINDIR/genkey.sh
su -c "
  trap 'killall $BIN' QUIT INT
  mount -t proc proc chroot/proc
  chroot ./chroot /$BIN
  umount chroot/proc
"
