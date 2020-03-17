#!/bin/bash
echo "Assembling all HeavyThing v1.24 binaries using fasmpp..."
echo
echo "fasmpp..."
cd fasmpp
fasm -m 524288 fasmpp.asm && ld -o fasmpp fasmpp.o
cd ..
for p in {dhtool,hnwatch,rwasa,sshtalk,toplip,webslap}; do
	echo
	echo "$p..."
	cd $p
	../fasmpp/fasmpp $p.asm >$p.tmp.asm
	fasm -m 524288 $p.tmp.asm && ld -o $p $p.tmp.o
	rm -f $p.tmp.asm
	cd ..
done
cd webslap
echo
echo "webslap_tlsmin..."
../fasmpp/fasmpp webslap_tlsmin.asm > webslap_tlsmin.tmp.asm
fasm -m 524288 webslap_tlsmin.tmp.asm && ld -o webslap_tlsmin webslap_tlsmin.tmp.o
rm -f webslap_tlsmin.tmp.asm
cd ..
cd rwasa
echo
echo "rwasa_tlsmin..."
../fasmpp/fasmpp rwasa_tlsmin.asm > rwasa_tlsmin.tmp.asm
fasm -m 524288 rwasa_tlsmin.tmp.asm && ld -o rwasa_tlsmin rwasa_tlsmin.tmp.o
rm -f rwasa_tlsmin.tmp.asm
cd ..
cd util
for p in {bigint_tune,make_dh_static,mersenneprimetest}; do
	echo
	echo "util/$p..."
	../fasmpp/fasmpp $p.asm > $p.tmp.asm
	fasm -m 524288 $p.tmp.asm && ld -o $p $p.tmp.o
	rm -f $p.tmp.asm
done
cd ..
cd examples
for p in {echo,hello_world,libsodium,minigzip,multicore_echo,sha256,sha3,simple_socket,sshecho,tlsecho,tuieffects,tuimatrix}; do
	echo
	echo "examples/$p..."
	cd $p
	../../fasmpp/fasmpp $p.asm > $p.tmp.asm
	fasm -m 524288 $p.tmp.asm && ld -o $p $p.tmp.o
	rm -f $p.tmp.asm
	cd ..
done
# our C/C++ examples
# these ones can't make use of fasmpp without modifying the wrappers
echo
echo "C/C++ Integration and Mixing, examples/hello_world_c1..."
cd hello_world_c1
fasm -m 524288 ht.asm
gcc -no-pie -nostdlib -o hello hello.c ht.o
cd ..
cd hello_world_c2
echo
echo "C/C++ Integration and Mixing, examples/hello_world_c2..."
fasm -m 524288 ht.asm
gcc -no-pie -nostdlib -o hello hello.c ht.o
cd ..
echo
echo "C/C++ Integration and Mixing, examples/simplechat_c++..."
cd simplechat_c++
fasm -m 524288 ht.asm
g++ -no-pie -std=c++11 -o simplechat simplechat.cpp ht.o
cd ..
echo
echo "C/C++ Integration and Mixing, examples/simplechat_ssh_auth_c++..."
cd simplechat_ssh_auth_c++
fasm -m 524288 ht.asm
g++ -no-pie -std=c++11 -o simplechat_ssh simplechat_ssh.cpp ht.o
cd ..
echo
echo "C/C++ Integration and Mixing, examples/simplechat_ssh_c++..."
cd simplechat_ssh_c++
fasm -m 524288 ht.asm
g++ -no-pie -std=c++11 -o simplechat_ssh simplechat_ssh.cpp ht.o
cd ..
echo
echo "Done."
