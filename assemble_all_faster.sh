#!/bin/sh

a="/$0"; a=${a%/*}; a=${a:-.}; a=${a#/}/; BINDIR=$(cd $a; pwd)
PATH=$BINDIR/fasmpp:$PATH

assembleo() {
  test -r "$1.o" && return
  echo; echo "$1..." && fasm -m 524288 $1.asm
}

assemble() {
  F=$1
  test -x "$1" && return
  test -r "$1.tmp.asm" && F="$1.tmp"
  assembleo $F $1 && ld -o $1 $F.o && rm $F.o
}

preandasm() {
  test -x "$1" && return
  test -r "$1.tmp.asm" && rm "$1"
  fasmpp $1.asm >$1.tmp.asm
  assemble $1
  rm -f $1.tmp.asm
}

echo "=== Assembling all HeavyThing v1.24 binaries using fasmpp..."
assemble fasmpp/fasmpp

for p in dhtool hnwatch rwasa sshtalk toplip webslap; do
	preandasm $p/$p
done
preandasm webslap/webslap_tlsmin
preandasm rwasa/rwasa_tlsmin
for p in bigint_tune make_dh_static mersenneprimetest; do
        preandasm util/$p
done
for p in echo hello_world libsodium minigzip multicore_echo sha256 sha3 simple_socket sshecho tlsecho tuieffects tuimatrix; do
	preandasm examples/$p/$p
done

mygcc() {
  test -x "$1" && return
  gcc -no-pie -nostdlib -o $1 $1.c ${1%/*}/ht.o
}
mygpp() {
  test -x "$1" && return
  g++ -no-pie -std=c++11 -o $1 $1.cpp ${1%/*}/ht.o
}

# our C/C++ examples
# these ones can't make use of fasmpp without modifying the wrappers
echo
echo "== C/C++ Integration and Mixing"
for p in \
  hello_world_c1/hello \
  hello_world_c2/hello \
  simplechat_c++/simplechat \
  simplechat_ssh_auth_c++/simplechat_ssh \
  simplechat_ssh_c++/simplechat_ssh
do
  assembleo examples/${p%/*}/ht
  test -r examples/$p.c && mygcc examples/$p
  test -r examples/$p.cpp && mygpp examples/$p
done

echo "Done."
