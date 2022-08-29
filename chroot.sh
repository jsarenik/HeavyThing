#!/bin/sh

a="/$0"; a="${a%/*}"; a="${a:-.}"; a="${a#/}/"; BINDIR="$(cd $a; pwd)"
a="/$PWD/"; a="${a%/*}"; a="${a:-.}"; a="${a#/}/"; CURDIR="$(cd $a; pwd)"
BIN=${1:-"${CURDIR##*/}"}
CHROOT=$CURDIR/chroot-$BIN

genkey() {
  ssh-keygen -t rsa -m PEM -N '' -f $CHROOT/etc/ssh/ssh_host_rsa_key
}

mkdir -p $CHROOT/etc/ssh $CHROOT/proc
cp $BIN $CHROOT
test -r $CHROOT/etc/ssh/ssh_host_rsa_key || genkey
exec unshare -U -m -p --fork -r --propagation slave sh -xc "
  mount -t proc proc $CHROOT/proc
  chroot $CHROOT /$BIN
"
