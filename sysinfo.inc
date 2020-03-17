	; ------------------------------------------------------------------------
	; HeavyThing x86_64 assembly language library and showcase programs
	; Copyright © 2015-2018 2 Ton Digital 
	; Homepage: https://2ton.com.au/
	; Author: Jeff Marrison <jeff@2ton.com.au>
	;       
	; This file is part of the HeavyThing library.
	;       
	; HeavyThing is free software: you can redistribute it and/or modify
	; it under the terms of the GNU General Public License, or
	; (at your option) any later version.
	;       
	; HeavyThing is distributed in the hope that it will be useful, 
	; but WITHOUT ANY WARRANTY; without even the implied warranty of
	; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
	; GNU General Public License for more details.
	;       
	; You should have received a copy of the GNU General Public License along
	; with the HeavyThing library. If not, see <http://www.gnu.org/licenses/>.
	; ------------------------------------------------------------------------
	;       
	; sysinfo.inc: Convenience routine to get the # of CPUs
	;
	; NOTE: if you do a syscall_setaffinity before calling this, the results
	; won't obviously be correct (though you can certainly do it if you are
	; careful).
	;
	; glibc parses /proc/cpuinfo, though nproc uses sched_getaffinity ...
	; the single system call certainly seems like a better choice here.
	;

if used sysinfo$cpucount | defined include_everything
	; no arguments, returns # of CPUs in eax
falign
sysinfo$cpucount:
	prolog	sysinfo$cpucount
	sub	rsp, 512		; 4096 cpuset size
	mov	rdi, rsp
	xor	esi, esi
	mov	edx, 512
	call	memset32		; sanity only
	xor	edi, edi		; pid == calling thread
	mov	eax, syscall_sched_getaffinity
	mov	esi, 512
	mov	rdx, rsp
	syscall
	; loop backward
	mov	eax, 4096 - 64
	lea	rsi, [rsp+504]
calign
.loop:
	cmp	qword [rsi], 0
	jne	.gotit
	test	eax, eax
	jz	.retone
	sub	rsi, 8
	sub	eax, 64
	jmp	.loop
calign
.gotit:
	bsr	rdx, [rsi]
	lea	eax, [eax+edx+1]
	add	rsp, 512
	epilog
calign
.retone:
	add	rsp, 512
	mov	eax, 1
	epilog

end if
