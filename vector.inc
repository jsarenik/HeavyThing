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
	; vector.inc: Simple 64-bit holding resizeable "vector"-like object
	;
	; I don't normally prefer to use variable length arrays like the one
	; presented herein, but there are a few specific case scenarios where
	; they are more-or-less required.

if used vector$new | used vector$new_reserve | used vector$new_fromlist | defined include_everything

vector_space_ofs = 0		; how large (in items, not bytes) it is
vector_count_ofs = 8		; how many items we have
vector_array_ofs = 16		; heap$alloc'd pointer to the actual array ([vector_space_ofs] shl 3 in size)

vector_size = 24


vector_default_reserve = 8	; how many items worth of space do we reserve by default?

vector_default_extend = 32	; how many items worth of space do we add at a time if we have to extend?


	; no arguments, returns new initialized vector object in rax
falign
vector$new:
	prolog	vector$new
	mov	edi, vector_default_reserve
	call	vector$new_reserve
	epilog

end if


if used vector$new_reserve | defined include_everything
	; single argument in rdi: how many items (not bytes) to reserve
	; returns new initialized vector object in rax
falign
vector$new_reserve:
	prolog	vector$new_reserve
	push	rbx
	mov	rbx, rdi
	mov	edi, vector_size
	call	heap$alloc
	push	rax
	lea	rdi, [rbx*8]
	call	heap$alloc
	mov	rsi, rax
	mov	rcx, rbx
	pop	rax rbx
	xor	edx, edx
	mov	[rax+vector_space_ofs], rcx
	mov	[rax+vector_count_ofs], rdx
	mov	[rax+vector_array_ofs], rsi
	epilog

end if

if used vector$new_fromunsigned | defined include_everything
	; single argument in rdi: first item to populate into it
	; convenience function to prepopulate it with one item
falign
vector$new_fromunsigned:
	prolog	vector$new_fromunsigned
	push	rbx
	mov	rbx, rdi
	mov	edi, vector_size
	call	heap$alloc
	push	rax
	mov	edi, vector_default_reserve * 8
	call	heap$alloc
	mov	[rax], rbx
	mov	rsi, rax
	pop	rax rbx
	xor	edx, edx
	mov	qword [rax+vector_space_ofs], vector_default_reserve
	mov	qword [rax+vector_count_ofs], 1
	mov	[rax+vector_array_ofs], rsi
	epilog

end if


if used vector$new_fromlist | defined include_everything
	; single argument in rdi: a list object
	; returns a new initialized and populated vector object in rax
falign
vector$new_fromlist:
	prolog	vector$new_fromlist
	push	rbx
	mov	rbx, rdi
	mov	rsi, vector_default_reserve
	mov	rdi, [rdi+_list_size_ofs]
	cmp	rdi, rsi
	cmovb	rdi, rsi
	call	vector$new_reserve
	mov	rdi, rbx
	mov	rbx, rax
	mov	rsi, .pushback
	mov	rdx, rax
	call	list$foreach_arg
	mov	rax, rbx
	pop	rbx
	epilog
falign
.pushback:
	; rdi == list value, rsi == our vector object
	xchg	rdi, rsi
	call	vector$push_back
	ret

end if


if used vector$destroy | defined include_everything
	; single argument in rdi: vector object to destroy
falign
vector$destroy:
	prolog	vector$destroy
	push	rdi
	mov	rdi, [rdi+vector_array_ofs]
	call	heap$free
	pop	rdi
	call	heap$free
	epilog

end if


if used vector$destroy_clear | defined include_everything
	; single argument in rdi: vector object to destroy
falign
vector$destroy_clear:
	prolog	vector$destroy_clear
	push	rdi
	mov	rdi, [rdi+vector_array_ofs]
	call	heap$free_clear
	pop	rdi
	call	heap$free_clear
	epilog

end if


if used vector$size | defined include_everything
	; single argument in rdi: vector object
	; returns item count in rax
	; placeholder function really
falign
vector$size:
	prolog	vector$size
	mov	rax, [rdi+vector_count_ofs]
	epilog

end if


if used vector$shrink_to_fit | defined include_everything
	; single argument in rdi: vector object
	; this creates a new array allocation exact size, frees the old one
falign
vector$shrink_to_fit:
	prolog	vector$shrink_to_fit
	cmp	qword [rdi+vector_count_ofs], 0
	je	.empty
	push	rdi
	mov	rsi, [rdi+vector_count_ofs]
	mov	[rdi+vector_space_ofs], rsi
	mov	rdi, [rdi+vector_array_ofs]
	shl	rsi, 3
	call	heap$alloc_blockcopy
	pop	rdx
	mov	rdi, [rdx+vector_array_ofs]
	mov	[rdx+vector_array_ofs], rax
	call	heap$free
	epilog
calign
.empty:
	mov	qword [rdi+vector_space_ofs], vector_default_reserve
	mov	qword [rdi+vector_count_ofs], 0
	push	rdi
	mov	edi, vector_default_reserve shl 3
	call	heap$alloc
	pop	rdx
	mov	rdi, [rdx+vector_array_ofs]
	mov	[rdx+vector_array_ofs], rax
	call	heap$free
	epilog

end if


if used vector$push_back | defined include_everything
	; two arguments: rdi == vector object, rsi == value to add to the end
falign
vector$push_back:
	prolog	vector$push_back
.again:
	mov	rdx, [rdi+vector_count_ofs]
	mov	rcx, [rdi+vector_array_ofs]
	cmp	rdx, [rdi+vector_space_ofs]
	je	.needmore
	mov	[rcx+rdx*8], rsi
	add	qword [rdi+vector_count_ofs], 1
	epilog
calign
.needmore:
	push	rdi rsi
	mov	esi, vector_default_extend
	call	vector$reserve
	pop	rsi rdi
	jmp	.again

end if


if used vector$push_front | defined include_everything
	; two arguments: rdi == vector object, rsi == value to add to the head
	; note: this does a memmove of the entire array (obviously)
falign
vector$push_front:
	prolog	vector$push_front
	mov	rdx, [rdi+vector_count_ofs]
	mov	rcx, [rdi+vector_array_ofs]
	cmp	rdx, [rdi+vector_space_ofs]
	je	.needmore
	test	rdx, rdx
	jz	.empty
	; otherwise, there is room, move the array forward 8 bytes
	push	rdi rsi rcx
	mov	rsi, rcx
	shl	rdx, 3
	lea	rdi, [rcx+8]
	call	memmove
	pop	rcx rsi rdi
	mov	[rcx], rsi
	add	qword [rdi+vector_count_ofs], 1
	epilog
calign
.needmore:
	; rather than let vector$reserve do the extend, and then re-memmove, do it inline here:
	add	qword [rdi+vector_count_ofs], 1
	add	qword [rdi+vector_space_ofs], vector_default_extend
	push	rdi rsi rdx rcx
	; first up, allocate a new block rdx+vector_default_extend in size
	mov	rdi, [rdi+vector_space_ofs]
	add	rdi, vector_default_extend
	shl	rdi, 3
	call	heap$alloc
	pop	rsi rdx rcx
	; rsi == original array, rdx == original count/space, rcx == value to add to the front
	lea	rdi, [rax+8]		; new destination
	mov	[rax], rcx		; store new value at the head
	push	rax
	test	rdx, rdx
	jz	.nocopy
	shl	rdx, 3
	call	memcpy
.nocopy:
	pop	rax rsi
	mov	rdi, [rsi+vector_array_ofs]
	mov	[rsi+vector_array_ofs], rax
	call	heap$free
	epilog
calign
.empty:
	; there is space, but our array is empty so no move required
	mov	[rcx], rsi
	add	qword [rdi+vector_count_ofs], 1
	epilog

end if


if used vector$front | defined include_everything
	; single argument in rdi: vector object
	; returns the first item in the vector, or -1 if empty
	; does not remove it/modify the vector
falign
vector$front:
	prolog	vector$front
	mov	rsi, [rdi+vector_array_ofs]
	mov	rax, -1
	cmp	qword [rdi+vector_count_ofs], 0
	je	.ret
	mov	rax, [rsi]
.ret:
	epilog

end if


if used vector$back | defined include_everything
	; single argument in rdi: vector object
	; returns the last item in the vector, or -1 if empty
	; does not remove it/modify the vector
falign
vector$back:
	prolog	vector$back
	mov	rdx, [rdi+vector_count_ofs]
	mov	rsi, [rdi+vector_array_ofs]
	mov	rax, -1
	test	rdx, rdx
	jz	.ret
	sub	rdx, 1
	mov	rax, [rsi+rdx*8]
.ret:
	epilog

end if


if used vector$pop_back | defined include_everything
	; single argument in rdi: vector object
	; returns last item in rax (or -1 if vector is empty)
	; removes the item from the vector
falign
vector$pop_back:
	prolog	vector$pop_back
	mov	rdx, [rdi+vector_count_ofs]
	mov	rsi, [rdi+vector_array_ofs]
	mov	rax, -1
	test	rdx, rdx
	jz	.ret
	sub	rdx, 1
	mov	rax, [rsi+rdx*8]
	mov	[rdi+vector_count_ofs], rdx
.ret:
	epilog

end if


if used vector$pop_front | defined include_everything
	; single argument in rdi: vector object
	; returns first item in rax (or -1 if vector is empty)
	; removes the item from the vector
	; NOTE: this memmoves the rest of the items as it must
falign
vector$pop_front:
	prolog	vector$pop_front
	mov	rdx, [rdi+vector_count_ofs]
	mov	rsi, [rdi+vector_array_ofs]
	mov	rax, -1
	test	rdx, rdx
	jz	.ret
	push	qword [rsi]
	sub	rdx, 1
	mov	[rdi+vector_count_ofs], rdx
	mov	rdi, rsi
	add	rsi, 8
	shl	rdx, 3
	call	memmove
	pop	rax
.ret:
	epilog

end if


if used vector$search | defined include_everything
	; two arguments: rdi == vector object, rsi == item to search for
	; returns the position of the search item, or -1 if not found
falign
vector$search:
	prolog	vector$search
	mov	rax, -1
	mov	rcx, [rdi+vector_count_ofs]
	mov	rdx, [rdi+vector_array_ofs]
	test	rcx, rcx
	jz	.ret
	xor	eax, eax
calign
.searchloop:
	cmp	qword [rdx+rax*8], rsi
	je	.ret
	add	rax, 1
	sub	rcx, 1
	jnz	.searchloop
	mov	rax, -1
.ret:
	epilog

end if


if used vector$insert | defined include_everything
	; three arguments: rdi == vector object, rsi == index of position to insert at, rdx == value to insert
	; if !rsi, then calls push_front, if rsi >= count, calls push_back, else inserts it
falign
vector$insert:
	prolog	vector$insert
.again:
	mov	rcx, [rdi+vector_count_ofs]
	mov	r8, [rdi+vector_array_ofs]
	test	rsi, rsi
	jz	.push_front
	cmp	rsi, rcx
	jae	.push_back
	cmp	rcx, [rdi+vector_space_ofs]
	je	.needmore
	; otherwise, there is room for it..
	; make a space at rsi by moving rcx-rsi worth of items forward one
	push	rdi rsi rdx r8
	mov	rdx, rcx
	sub	rdx, rsi
	lea	rdi, [r8+rsi*8+8]
	lea	rsi, [r8+rsi*8]
	shl	rdx, 3
	call	memmove
	pop	r8 rdx rsi rdi
	lea	rcx, [r8+rsi*8]
	mov	[rcx], rdx
	add	qword [rdi+vector_count_ofs], 1
	epilog
calign
.needmore:
	push	rdi rsi rdx
	mov	esi, vector_default_extend
	call	vector$reserve
	pop	rdx rsi rdi
	jmp	.again
calign
.push_front:
	mov	rsi, rdx
	call	vector$push_front
	epilog
calign
.push_back:
	mov	rsi, rdx
	call	vector$push_back
	epilog

end if


if used vector$remove | defined include_everything
	; two arguments: rdi == vector object, rsi == index of position to remove
	; if !rsi, calls pop_front, if rsi >= count, calls pop_back, else removes and memmoves
falign
vector$remove:
	prolog	vector$remove
	mov	rcx, [rdi+vector_count_ofs]
	mov	r8, [rdi+vector_array_ofs]
	test	rsi, rsi
	jz	.pop_front
	cmp	rsi, rcx
	jae	.pop_back
	; remove the space at rsi by moving rcx-rsi-1 worth of items back one
	sub	qword [rdi+vector_count_ofs], 1
	mov	rdx, rcx
	sub	rdx, rsi
	lea	rdi, [r8+rsi*8]
	lea	rsi, [r8+rsi*8+8]
	sub	rdx, 1
	shl	rdx, 3
	call	memmove
	epilog
calign
.pop_front:
	call	vector$pop_front
	epilog
calign
.pop_back:
	call	vector$pop_back
	epilog

end if


if used vector$get | defined include_everything
	; two arguments: rdi == vector object, rsi == index of position to retrieve
	; returns value in rax (or -1 if rsi was out of bounds)
falign
vector$get:
	prolog	vector$get
	mov	rdx, [rdi+vector_array_ofs]
	mov	rcx, [rdi+vector_count_ofs]
	mov	rax, -1
	cmp	rsi, rcx
	jae	.ret
	mov	rax, [rdx+rsi*8]
.ret:
	epilog

end if


if used vector$at | defined include_everything
	; two arguments: rdi == vector object, rsi == index of position to retrieve
	; returns value in rax (or -1 if rsi was out of bounds)
	; NOTE: this is just a copy/paste of vector$get, in case at is preferred function name
falign
vector$at:
	prolog	vector$at
	mov	rdx, [rdi+vector_array_ofs]
	mov	rcx, [rdi+vector_count_ofs]
	mov	rax, -1
	cmp	rsi, rcx
	jae	.ret
	mov	rax, [rdx+rsi*8]
.ret:
	epilog
	
end if


if used vector$set | defined include_everything
	; three arguments: rdi == vector object, rsi == index of position to set, rdx == value to set
	; returns bool in eax as to whether rsi was ok or not (0 == rsi was out of bounds, 1 == okay)
falign
vector$set:
	prolog	vector$set
	mov	rcx, [rdi+vector_array_ofs]
	mov	r8, [rdi+vector_count_ofs]
	xor	eax, eax
	cmp	rsi, r8
	jae	.ret
	mov	eax, 1
	mov	[rcx+rsi*8], rdx
.ret:
	epilog

end if


if used vector$reserve | defined include_everything
	; two arguments: rdi == vector object, rsi == number of free items to reserve
	; note: this ensures there are at least rsi many unused but allocated spaces
	; e.g. [vector_space_ofs] - [vector_count_ofs] >= rsi on return
falign
vector$reserve:
	prolog	vector$reserve
	mov	rdx, [rdi+vector_space_ofs]
	mov	rcx, [rdi+vector_count_ofs]
	mov	r9, rdx
	sub	rdx, rcx
	mov	r8d, vector_default_extend
	cmp	rdx, rsi
	jae	.ret
	; we need to add rsi-rdx items
	sub	rsi, rdx
	cmp	rsi, r8
	cmovb	rsi, r8
	add	rsi, r9
	push	rdi
	mov	[rdi+vector_space_ofs], rsi
	test	rcx, rcx
	jz	.justalloc
	lea	rdi, [rsi*8]
	call	heap$alloc
	mov	rcx, [rsp]		; vector object
	mov	rdi, rax
	mov	rsi, [rcx+vector_array_ofs]
	mov	rdx, [rcx+vector_count_ofs]
	push	rax
	shl	rdx, 3
	call	memcpy
	pop	rax rcx
	mov	rdi, [rcx+vector_array_ofs]
	mov	[rcx+vector_array_ofs], rax
	call	heap$free
.ret:
	epilog
calign
.justalloc:
	; no count exists
	lea	rdi, [rsi*8]
	call	heap$alloc
	pop	rcx		; vector object
	mov	rdi, [rcx+vector_array_ofs]
	mov	[rcx+vector_array_ofs], rax
	call	heap$free
	epilog
	
end if


if used vector$foreach | defined include_everything
	; two arguments: rdi == vector object, rsi == function to call with single arg in rdi of the value
falign
vector$foreach:
	prolog	vector$foreach
	push	rbx r12 r13
	mov	rbx, [rdi+vector_array_ofs]
	mov	r12, [rdi+vector_count_ofs]
	mov	r13, rsi
	test	r12, r12
	jz	.return
calign
.loop:
	mov	rdi, [rbx]
	add	rbx, 8
	call	r13
	sub	r12, 1
	jnz	.loop
.return:
	pop	r13 r12 rbx
	epilog

end if


if used vector$foreach_limit | defined include_everything
	; three arguments: rdi == vector object, rsi == function to call with single arg in rdi of the value, rdx == count/limit
falign
vector$foreach_limit:
	prolog	vector$foreach_limit
	push	rbx r12 r13
	mov	rbx, [rdi+vector_array_ofs]
	mov	r12, [rdi+vector_count_ofs]
	mov	r13, rsi
	cmp	r12, rdx
	cmova	r12, rdx
	test	r12, r12
	jz	.return
calign
.loop:
	mov	rdi, [rbx]
	add	rbx, 8
	call	r13
	sub	r12, 1
	jnz	.loop
.return:
	pop	r13 r12 rbx
	epilog

end if


if used vector$foreach_arg | defined include_everything
	; three arguments: rdi == vector object, rsi == function to call with the value, rdx == argument to pass to the function in rsi
	; rsi will get called with rdi as the vector value, rsi will be what is given in rdx
falign
vector$foreach_arg:
	prolog	vector$foreach_arg
	push	rbx r12 r13 r14
	mov	rbx, [rdi+vector_array_ofs]
	mov	r12, [rdi+vector_count_ofs]
	mov	r13, rsi
	mov	r14, rdx
	test	r12, r12
	jz	.return
calign
.loop:
	mov	rdi, [rbx]
	mov	rsi, r14
	add	rbx, 8
	call	r13
	sub	r12, 1
	jnz	.loop
.return:
	pop	r14 r13 r12 rbx
	epilog

end if


if used vector$reverse_foreach | defined include_everything
	; two arguments: rdi == vector object, rsi == function to call with single arg in rdi of the value
falign
vector$reverse_foreach:
	prolog	vector$reverse_foreach
	push	rbx r12 r13
	mov	rbx, [rdi+vector_array_ofs]
	mov	r12, [rdi+vector_count_ofs]
	mov	r13, rsi
	test	r12, r12
	jz	.return
	lea	rcx, [r12-1]
	lea	rbx, [rbx+rcx*8]
calign
.loop:
	mov	rdi, [rbx]
	sub	rbx, 8
	call	r13
	sub	r12, 1
	jnz	.loop
.return:
	pop	r13 r12 rbx
	epilog

end if


if used vector$reverse_foreach_limit | defined include_everything
	; three arguments: rdi == vector object, rsi == function to call with single arg in rdi of the value, rdx == count/limit
	; passing 0 in rdx is silly and will result in the whole vector being traversed
falign
vector$reverse_foreach_limit:
	prolog	vector$reverse_foreach_limit
	mov	rbx, [rdi+vector_array_ofs]
	mov	r12, [rdi+vector_count_ofs]
	mov	r13, rsi
	lea	rcx, [r12-1]
	lea	rbx, [rbx+rcx*8]
	cmp	r12, rdx
	cmova	r12, rdx
	test	r12, r12
	jz	.return
calign
.loop:
	mov	rdi, [rbx]
	sub	rbx, 8
	call	r13
	sub	r12, 1
	jnz	.loop
.return:
	pop	r13 r12 rbx
	epilog

end if


if used vector$reverse_foreach_arg | defined include_everything
	; three arguments: rdi == vector object, rsi == function to call with the value, rdx == argument to pass to the function in rsi
	; rsi will get called with rdi as the vector value, rsi will be what is given in rdx
falign
vector$reverse_foreach_arg:
	prolog	vector$reverse_foreach_arg
	push	rbx r12 r13 r14
	mov	rbx, [rdi+vector_array_ofs]
	mov	r12, [rdi+vector_count_ofs]
	mov	r13, rsi
	mov	r14, rdx
	test	r12, r12
	jz	.return
	lea	rcx, [r12-1]
	lea	rbx, [rbx+rcx*8]
calign
.loop:
	mov	rdi, [rbx]
	mov	rsi, r14
	sub	rbx, 8
	call	r13
	sub	r12, 1
	jnz	.loop
.return:
	pop	r14 r13 r12 rbx
	epilog

end if


if used vector$to_list | defined include_everything
	; single argument in rdi: vector object
	; returns new list object in rax populated with the vector contents
falign
vector$to_list:
	prolog	vector$to_list
	push	rdi
	call	list$new
	mov	rdi, [rsp]
	mov	[rsp], rax
	mov	rsi, .pushback
	mov	rdx, rax
	call	vector$foreach_arg
	pop	rax
	epilog
falign
.pushback:
	; rdi == vector item, rsi == list object
	xchg	rdi, rsi
	call	list$push_back
	ret

end if


if used vector$clear | defined include_everything
	; single argument in rdi: vector object
	; this just sets the item count to 0 (does not modify reserve space)
falign
vector$clear:
	prolog	vector$clear
	mov	qword [rdi+vector_count_ofs], 0
	epilog

end if


if used vector$clear_arg | defined include_everything
	; three arguments: rdi == vector object, rsi == function to call for each value, rdx == argument to pass to func in rsi
	; this will call the function in rsi with rdi == vector item, rsi == argument passed to this in rdx
	; and then set the item count to 0
falign
vector$clear_arg:
	prolog	vector$clear_arg
	push	rdi
	call	vector$foreach_arg
	pop	rdi
	mov	qword [rdi+vector_count_ofs], 0
	epilog

end if


if used vector$shuffle | defined include_everything
	; single argument in rdi: vector to shuffle
	; NOTE: this requires the size of our vector to be a 32 bit value (vector size of 4b+ items? yikes, do it some other way haha)
falign
vector$shuffle:
	prolog	vector$shuffle
	push	rbx r12 r13
	mov	rbx, [rdi+vector_array_ofs]
	mov	r12, [rdi+vector_count_ofs]
	xor	r13d, r13d
	test	r12, r12
	jz	.ret
calign
.loop:
	mov	esi, r12d
	xor	edi, edi
	sub	esi, r13d
	sub	esi, 1
	call	rng$int
	add	eax, r13d
	mov	rcx, [rbx+rax*8]
	mov	rdx, [rbx+r13*8]
	mov	[rbx+rax*8], rdx
	mov	[rbx+r13*8], rcx
	add	r13d, 1
	cmp	r13d, r12d
	jb	.loop
.ret:
	pop	r13 r12 rbx
	epilog

end if


if used vector$sort | defined include_everything
	; single argument in rdi: vector to sort
	; NOTE: this is by no means efficient, and instead is my lazy implementation
	; that constructs an unsignedmap, inserts all of our list items, then resets our vector contents
falign
vector$sort:
	prolog	vector$sort
	push	rbx r12
	mov	rbx, rdi
	xor	edi, edi		; sort order please
	call	unsignedmap$new
	mov	r12, rax
	mov	rdi, rbx
	mov	rsi, .insert
	mov	rdx, rax
	call	vector$foreach_arg
	mov	rdi, rbx
	call	vector$clear
	mov	rdi, r12
	mov	rsi, .pushback
	mov	rdx, rbx
	call	unsignedmap$foreach_arg
	; cleanup our map
	mov	rdi, r12
	xor	esi, esi
	call	unsignedmap$clear
	mov	rdi, r12
	call	unsignedmap$destroy
	pop	r12 rbx
	epilog
falign
.insert:
	; rdi == vector item, rsi == unsignedmap
	xchg	rdi, rsi
	xor	edx, edx
	call	unsignedmap$insert
	ret
falign
.pushback:
	; rdi == map key, rsi == map value, rdx == vector
	mov	rsi, rdi
	mov	rdi, rdx
	call	vector$push_back
	ret

end if
