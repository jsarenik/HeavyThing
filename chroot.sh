#!/bin/sh

a="/$0"; a="${a%/*}"; a="${a:-.}"; a="${a#/}/"; BINDIR="$(cd $a; pwd)"
a="/$PWD/"; a="${a%/*}"; a="${a:-.}"; a="${a#/}/"; CURDIR="$(cd $a; pwd)"
BIN=${1:-"${CURDIR##*/}"}
CHROOT=$CURDIR/chroot-$BIN

genkey() {
  type=${1:-"dsa"}
  F=$CHROOT/etc/ssh/ssh_host_${type}_key
  test -r "$F" && return 0
  ssh-keygen -t "$type" -m PEM -N '' -f $CHROOT/etc/ssh/ssh_host_${type}_key
}

mkdir -p $CHROOT/etc/ssh $CHROOT/proc
cp $BIN $CHROOT
genkey dsa
genkey rsa
exec unshare -U -m -p --fork -r --propagation slave sh -xc "
  mount -t proc proc $CHROOT/proc
  chroot $CHROOT /$BIN
"
