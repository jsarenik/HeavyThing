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
	; fasmpp: fasm preprocessor to dramatically decrease compile times.
	;
	; Burning Purpose: HeavyThing compilation times have really been irritating of late
	; so this program takes a HeavyThing application source filename as input, and then
	; parses what is hopefully a narrower subset of the HeavyThing library so that poor
	; fasm doesn't have to deal with the entire thing.
	;
	; Of course, if your application uses the entire HeavyThing library all at once,
	; this program is of little use. More often than not though, only a small subset is
	; used.

include '../ht_defaults.inc'
include '../ht.inc'

globals
{
	; list of vectors
	inputargument	dq	0
	sourcefiles	dq	0
	ht_sourcefiles	dq	0
	; accounting
	sourcefilecount	dq	0
	sourcebytes	dq	0
	sourcelines	dq	0
	outputbytes	dq	0
	outputlines	dq	0
	inputhash	dq	0
	; accounting formatter
	withcommas	dq	0
	; special vectors for heavything mandatories
	ht_settings	dq	0
	ht		dq	0
	ht_data		dq	0
	dataseg_macros	dq	0
	align_macros	dq	0
	profiler	dq	0
	fcleartext	dq	0
	fsyscall	dq	0
	fvdso		dq	0
	fansi_colors	dq	0
	fbreakpoint	dq	0
	frdtsc		dq	0
	fsleeps		dq	0
	fdh_groups	dq	0
	fdh_pool_2k	dq	0
	fdh_pool_3k	dq	0
	fdh_pool_4k	dq	0
	fdh_pool_6k	dq	0
	fdh_pool_8k	dq	0
	fdh_pool_16k	dq	0
	use_dh_bits	dq	0
	conf_dh_bits	dq	0
	in_ht		dq	0
	used_ansi_colors	dq	0
	; csymbols == every symbol from the if used lines in all the source files (but only if in_ht)
	csymbols	dq	0
	; dependencies == unsignedmap of source vector pointers as keys, unsignedmap dependants source vectors value (this is source keys with subet of sources that depend on it)
	dependencies	dq	0
	; emitted == unsignedmap (set) of source vectors that we have already spat out
	emitted		dq	0
}

	; single argument in rdi: url object file://... to our source
falign
loadsource:
	prolog	loadsource
	push	rbx r12 r13 r14 r15
	mov	rbx, rdi

	mov	rdi, [rdi+url_path_ofs]
	mov	esi, 1		; it is a path
	call	url$encode
	push	rax
	mov	rdi, rax
	call	file$to_buffer
	pop	rdi
	push	rax
	call	heap$free
	pop	rax
	test	rax, rax
	jz	.bad
	add	qword [sourcefilecount], 1
	mov	rdx, [rax+buffer_length_ofs]
	mov	r12, rax
	mov	r13, [rax+buffer_itself_ofs]
	mov	r14, [rax+buffer_endptr_ofs]
	add	[sourcebytes], rdx

	; add this to our input hash
	mov	rdi, [inputhash]
	mov	rsi, r13
	; length in rdx is already there
	call	sha256$update

	; if we are in ht.inc, set in_ht so that sourcefiles get added to the right list
	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_ht_inc
	call	string$equals
	test	eax, eax
	jz	.notht
	mov	qword [in_ht], 1
.notht:
	; purposefully ignore string16.inc
	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_string16_inc
	call	string$equals
	test	eax, eax
	jnz	.string16_inc

	; count how many linefeeds we have first
	xor	edi, edi
	cmp	r13, r14
	je	.bad
calign
.lfcount:
	lea	edx, [edi+1]
	cmp	byte [r13], 10
	cmove	edi, edx
	add	r13, 1
	cmp	r13, r14
	jb	.lfcount
	test	edi, edi
	jz	.bad
	add	[sourcelines], rdi
	mov	r13, [r12+buffer_itself_ofs]
	call	vector$new
	mov	r15, rax
	; parse line by line, each include line gets auto-recursion called
calign
.linebyline:
	; first up: skip WS
	movzx	ecx, byte [r13]
	cmp	ecx, 32
	ja	.startofline
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.startofline
	; else, we hit either a 32, 9, 10, or 13
.emptyline:
	add	r13, 1
	cmp	r13, r14
	jb	.linebyline
.alldone:
	; if we fell through to hear searching, we ran out of input
	mov	rdi, r12
	call	buffer$destroy
	; add our vector to the sourcefiles, unless it is ht_defaults, ht.inc, or ht_data.inc
	mov	rdi, r15
	call	vector$front
	mov	rdi, rax
	mov	rsi, .s_format_elf64
	call	string$equals
	test	eax, eax
	jnz	.ht_defaults
	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_ht_inc
	call	string$equals
	test	eax, eax
	jnz	.ht
	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_ht_data_inc
	call	string$equals
	test	eax, eax
	jnz	.ht_data
	
	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_dataseg_macros_inc
	call	string$equals
	test	eax, eax
	jnz	.dataseg_macros
	
	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_align_macros_inc
	call	string$equals
	test	eax, eax
	jnz	.align_macros
	
	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_profiler_inc
	call	string$equals
	test	eax, eax
	jnz	.profiler

	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_cleartext_inc
	call	string$equals
	test	eax, eax
	jnz	.cleartext

	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_syscall_inc
	call	string$equals
	test	eax, eax
	jnz	.syscall

	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_vdso_inc
	call	string$equals
	test	eax, eax
	jnz	.vdso

	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_rdtsc_inc
	call	string$equals
	test	eax, eax
	jnz	.rdtsc

macro checkfile s, j {
	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, s
	call	string$equals
	test	eax, eax
	jnz	j
}
	checkfile .s_sleeps_inc, .sleeps
	checkfile .s_dh_groups_inc, .dh_groups
	checkfile .s_dh_pool_2k_inc, .dh_pool_2k
	checkfile .s_dh_pool_3k_inc, .dh_pool_3k
	checkfile .s_dh_pool_4k_inc, .dh_pool_4k
	checkfile .s_dh_pool_6k_inc, .dh_pool_6k
	checkfile .s_dh_pool_8k_inc, .dh_pool_8k
	checkfile .s_dh_pool_16k_inc, .dh_pool_16k

	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_breakpoint_inc
	call	string$equals
	test	eax, eax
	jnz	.breakpoint

	mov	rdi, [rbx+url_file_ofs]
	mov	rsi, .s_ansi_colors_inc
	call	string$equals
	test	eax, eax
	jnz	.ansi_colors

	; if we are in_ht, add to ht_sourcefiles instead

	mov	rdi, [sourcefiles]
	mov	rsi, r15
	mov	rdx, [ht_sourcefiles]
	cmp	qword [in_ht], 1
	cmove	rdi, rdx
	call	list$push_back

	; done
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.string16_inc:
	mov	rdi, r12
	call	buffer$destroy
	pop	r15 r14 r13 r12 rbx
	epilog
	
cleartext .s_format_elf64, 'format ELF64'
cleartext .s_ht_inc, 'ht.inc'
cleartext .s_ht_data_inc, 'ht_data.inc'
cleartext .s_dataseg_macros_inc, 'dataseg_macros.inc'
cleartext .s_align_macros_inc, 'align_macros.inc'
cleartext .s_profiler_inc, 'profiler.inc'
cleartext .s_string16_inc, 'string16.inc'
cleartext .s_cleartext_inc, 'cleartext.inc'
cleartext .s_syscall_inc, 'syscall.inc'
cleartext .s_vdso_inc, 'vdso.inc'
cleartext .s_rdtsc_inc, 'rdtsc.inc'
cleartext .s_sleeps_inc, 'sleeps.inc'
cleartext .s_breakpoint_inc, 'breakpoint.inc'
cleartext .s_ansi_colors_inc, 'tui_ansi.inc'
cleartext .s_dh_groups_inc, 'dh_groups.inc'
cleartext .s_dh_pool_2k_inc, 'dh_pool_2k.inc'
cleartext .s_dh_pool_3k_inc, 'dh_pool_3k.inc'
cleartext .s_dh_pool_4k_inc, 'dh_pool_4k.inc'
cleartext .s_dh_pool_6k_inc, 'dh_pool_6k.inc'
cleartext .s_dh_pool_8k_inc, 'dh_pool_8k.inc'
cleartext .s_dh_pool_16k_inc, 'dh_pool_16k.inc'

calign
.ht_defaults:
	mov	[ht_settings], r15
	; done
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.ht:
	mov	[ht], r15
	mov	qword [in_ht], 0
	; done
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.ht_data:
	mov	[ht_data], r15
	; done
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.dataseg_macros:
	mov	[dataseg_macros], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.align_macros:
	mov	[align_macros], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.profiler:
	mov	[profiler], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.cleartext:
	mov	[fcleartext], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.syscall:
	mov	[fsyscall], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.vdso:
	mov	[fvdso], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.rdtsc:
	mov	[frdtsc], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.sleeps:
	mov	[fsleeps], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.breakpoint:
	mov	[fbreakpoint], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.dh_groups:
	mov	[fdh_groups], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.dh_pool_2k:
	mov	[fdh_pool_2k], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.dh_pool_3k:
	mov	[fdh_pool_3k], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.dh_pool_4k:
	mov	[fdh_pool_4k], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.dh_pool_6k:
	mov	[fdh_pool_6k], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.dh_pool_8k:
	mov	[fdh_pool_8k], r15
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.dh_pool_16k:
	mov	[fdh_pool_16k], r15
	pop	r15 r14 r13 r12 rbx
	epilog

calign
.ansi_colors:
	mov	[fansi_colors], r15
	pop	r15 r14 r13 r12 rbx
	epilog

calign
.startofline:
	; common case: line is -only- a comment line
	cmp	byte [r13], ';'
	je	.commentline
	mov	rdi, r13		; start of non-comment character on line
	xor	esi, esi		; character count that we're about to turn into a string
	; walk forward til LF (if the current character is an LF, empty line)
	cmp	byte [r13], 10
	je	.emptyline
calign
.endoflinesearch:
	cmp	byte [r13], 10
	je	.endofline
	add	esi, 1
	add	r13, 1
	cmp	r13, r14
	jb	.endoflinesearch
	; if we fellthrough, no EOL on last line of file?
	mov	rdi, .s_notrailingeol
	call	string$to_stderr
	mov	rdi, [rbx+url_path_ofs]
	call	string$to_stderrln
	mov	eax, syscall_exit
	mov	edi, 1
	syscall
calign
.endofline:
	call	string$from_utf8
	; if this line starts with 'include ', do the deed
	push	rax
	mov	rdi, rax
	mov	rsi, .s_includespace
	call	string$starts_with
	test	eax, eax
	jnz	.includeline
	; if this line starts with 'file ', we need to change its parameter so that the pathname remains relative
	mov	rdi, [rsp]
	mov	rsi, .s_filespace
	call	string$starts_with
	test	eax, eax
	jnz	.filedirective
	mov	rdi, [rsp]
	mov	rsi, .s_filetab
	call	string$starts_with
	test	eax, eax
	jnz	.filedirective
	; if we are inside a macro definition, check for the trailing }
	cmp	dword [r12+buffer_user_ofs], 1
	je	.endofline_inmacro
	; if the line begins with macro
	mov	rdi, [rsp]
	mov	rsi, .s_macro_space
	call	string$starts_with
	test	eax, eax
	jnz	.macrodef

	; if we are not in_ht, just add the line
	cmp	qword [in_ht], 0
	je	@f
	; if the line starts with 'if used ', these are the symbols we are interested in
	mov	rdi, [rsp]
	mov	rsi, .s_if_used
	call	string$starts_with
	test	eax, eax
	jnz	.ifusedline

@@:
	pop	rsi
	mov	rdi, r15
	call	vector$push_back
	jmp	.linebyline
cleartext .s_macro_space, 'macro '
cleartext .s_closebrace, '}'
cleartext .s_if_used, 'if used '
cleartext .s_filespace, 'file '
cleartext .s_filetab, 'file',9
calign
.ifusedline:
	; split the string on the space and get our symbol name
	mov	rdi, [rsp]
	mov	esi, ' '
	call	string$split
	push	rax
	mov	rdi, rax
	call	list$pop_front
	mov	rdi, rax
	call	heap$free
	mov	rdi, [rsp]
	call	list$pop_front
	mov	rdi, rax
	call	heap$free
	mov	rdi, [rsp]
	; add the symbol name to the csymbols map
	call	list$pop_front
	push	rax
	mov	rdi, [csymbols]
	mov	rsi, rax
	mov	rdx, r15
	mov	rcx, [rax]
	call	stringmap$insert_unique
	pop	rdi
	test	eax, eax
	jnz	@f
	call	heap$free
@@:
	; symbol added, clean up the rest of our splitlist
	mov	rdi, [rsp]
	mov	rsi, heap$free
	call	list$clear
	pop	rdi
	call	heap$free

	pop	rsi
	mov	rdi, r15
	call	vector$push_back
	jmp	.linebyline
calign
.endofline_inmacro:
	mov	rdi, [rsp]
	mov	rsi, .s_closebrace
	call	string$equals		; hmmm, might have to do startswith, i dunno
	test	eax, eax
	jnz	.endofline_endofmacro
	pop	rsi
	mov	rdi, r15
	call	vector$push_back
	jmp	.linebyline
calign
.endofline_endofmacro:
	mov	dword [r12+buffer_user_ofs], 0
	pop	rsi
	mov	rdi, r15
	call	vector$push_back	
	jmp	.linebyline
calign
.macrodef:
	mov	dword [r12+buffer_user_ofs], 1
	pop	rsi
	mov	rdi, r15
	call	vector$push_back
	jmp	.linebyline
calign
.commentline:
	cmp	byte [r13], 10
	je	.linebyline
	add	r13, 1
	cmp	r13, r14
	jb	.commentline
	jmp	.alldone
cleartext .s_includespace, 'include '
cleartext .s_notrailingeol, 'Missing trailing LF in source: '
calign
.bad:
	mov	rdi, .s_bad
.badexit:
	call	string$to_stderr
	mov	rdi, [rbx+url_path_ofs]
	call	string$to_stderrln
	mov	eax, syscall_exit
	mov	edi, 1
	syscall
cleartext .s_bad, 'unable to load input file: '
cleartext .s_badincludeline, 'invalid include line: '
cleartext .s_badincludefile, 'bad include filename directive: '
cleartext .s_badfileline, 'invalid file line: '
cleartext .s_filespacequote, "file '"
cleartext .s_quote, "'"
calign
.filedirective:
	mov	rdi, [rsp]
	mov	esi, "'"
	call	string$indexof_charcode
	mov	rdi, .s_badfileline
	cmp	rax, 0
	jl	.badexit
	; same as includeline below but we have to modify it
	mov	rdi, [rsp]
	mov	esi, "'"
	call	string$split
	push	rax			; [rsp] == list$new, [rsp+8] == original string for the include line
	mov	rdi, rax
	call	list$pop_back
	push	rax			; [rsp] == filename string from list$new, [rsp+8] == list$new, [rsp+16] == original string
	; this should be our filename... so construct a new url object from that
	mov	rdi, rbx
	mov	rsi, rax
	call	url$new
	mov	rdi, .s_badfileline
	test	rax, rax
	jz	.badexit
	push	rax			; [rsp] == new url object, [rsp+8] == filename str, [rsp+16] == list$new, [rsp+24] == orig

	; so now, we need a brand new string of the url_path_ofs
	mov	rdi, .s_filespacequote
	mov	rsi, [rax+url_path_ofs]
	call	string$concat
	push	rax
	mov	rdi, rax
	mov	rsi, .s_quote
	call	string$concat
	mov	rdi, [rsp]
	mov	[rsp], rax
	call	heap$free
	; new string is in [rsp], add that to our source
	mov	rdi, r15
	pop	rsi
	call	vector$push_back

	pop	rdi
	call	url$destroy	
	pop	rdi
	call	heap$free	; our string we popped from the list
	mov	rdi, [rsp]
	mov	rsi, heap$free
	call	list$clear
	pop	rdi
	call	heap$free
	; string of the original include line is next
	pop	rdi
	call	heap$free
	jmp	.linebyline
	
calign
.includeline:
	; mov	rdi, [rsp]
	; call	string$to_stderrln
	mov	rdi, [rsp]
	mov	esi, "'"
	call	string$indexof_charcode
	mov	rdi, .s_badincludeline
	cmp	rax, 0
	jl	.badexit
	; we know this is going to contain a quoted filename to include
	mov	rdi, [rsp]
	mov	esi, "'"
	call	string$split
	push	rax			; [rsp] == list$new, [rsp+8] == original string for the include line
	mov	rdi, rax
	call	list$pop_back
	push	rax			; [rsp] == filename string from list$new, [rsp+8] == list$new, [rsp+16] == original string
	; this should be our filename... so construct a new url object from that
	mov	rdi, rbx
	mov	rsi, rax
	call	url$new
	mov	rdi, .s_badincludefile
	test	rax, rax
	jz	.badexit
	push	rax			; [rsp] == new url object, [rsp+8] == filename str, [rsp+16] == list$new, [rsp+24] == orig
	mov	rdi, rax
	call	loadsource
	pop	rdi
	call	url$destroy	
	pop	rdi
	call	heap$free	; our string we popped from the list
	mov	rdi, [rsp]
	mov	rsi, heap$free
	call	list$clear
	pop	rdi
	call	heap$free
	; string of the original include line is next
	pop	rdi
	call	heap$free
	jmp	.linebyline



	; single argument: vector of strings (file contents with comment-only lines and preceding whitespace removed)
falign
emitsource:
	prolog	emitsource
	push	rbx
	mov	rbx, rdi
	mov	rsi, rdi
	mov	rdi, [emitted]
	call	unsignedmap$find_value
	test	eax, eax
	jz	.new
	pop	rbx
	epilog
calign
.new:
	mov	rdi, [emitted]
	mov	rsi, rbx
	xor	edx, edx
	call	unsignedmap$insert_unique
	mov	rdi, rbx
	mov	rsi, .eachsourceline
	call	vector$foreach
	pop	rbx
	epilog
falign
.eachsourceline:
	push	rdi
	call	string$utf8_length
	add	rax, 1
	add	[outputbytes], rax
	add	qword [outputlines], 1
	pop	rdi
	call	string$to_stdoutln
	ret


	; single argument: vector of strings
falign
destroysource:
	prolog	destroysource
	push	rdi
	mov	rsi, heap$free
	call	vector$foreach
	pop	rdi
	call	vector$destroy
	epilog

falign
emitheader:
	prolog	emitheader
	push	rbx
	mov	rdi, .header
	mov	rsi, [inputargument]
	call	string$concat
	mov	rbx, rax

	mov	rdi, rax
	call	string$utf8_length
	add	rax, 1
	add	qword [outputbytes], rax
	add	qword [outputlines], 25
	mov	rdi, rbx
	call	string$to_stdoutln

	mov	rdi, rbx
	call	heap$free

	mov	rdi, .header2
	call	string$utf8_length
	add	rax, 1
	add	qword [outputbytes], rax
	add	qword [outputlines], 1
	mov	rdi, .header2
	call	string$to_stdout

	; output accounting for loaded
	mov	rdi, [withcommas]
	mov	rsi, [sourcefilecount]
	call	formatter$doit
	push	rax
	mov	rdi, rax
	call	string$to_stdout
	pop	rdi
	mov	rsi, [rdi]
	add	[outputbytes], rsi
	call	heap$free

	mov	rdi, .s_sourcefiles
	mov	rsi, [rdi]
	add	[outputbytes], rsi
	call	string$to_stdout

	mov	rdi, [withcommas]
	mov	rsi, [sourcelines]
	call	formatter$doit
	push	rax
	mov	rdi, rax
	call	string$to_stdout
	pop	rdi
	mov	rsi, [rdi]
	add	[outputbytes], rsi
	call	heap$free

	mov	rdi, .s_lines
	mov	rsi, [rdi]
	add	[outputbytes], rsi
	call	string$to_stdout

	mov	rdi, [withcommas]
	mov	rsi, [sourcebytes]
	call	formatter$doit
	push	rax
	mov	rdi, rax
	call	string$to_stdout
	pop	rdi
	mov	rsi, [rdi]
	add	[outputbytes], rsi
	call	heap$free

	mov	rdi, .s_bytes
	mov	rsi, [rdi]
	add	[outputbytes], rsi
	call	string$to_stdout

	mov	rdi, .header3
	mov	rsi, [rdi]
	add	[outputbytes], rsi
	add	qword [outputlines], 1
	call	string$to_stdout

	sub	rsp, 32
	mov	rdi, [inputhash]
	mov	rsi, rsp
	mov	edx, edx
	call	sha256$final
	mov	rdi, rsp
	mov	esi, 32
	call	string$from_bintohex
	push	rax
	mov	rdi, rax
	call	string$to_stdoutln
	pop	rdi
	mov	rsi, [rdi]
	add	rsi, 1
	add	[outputbytes], rsi
	call	heap$free

	mov	rdi, .header4
	mov	rsi, [rdi]
	add	[outputbytes], rsi
	add	qword [outputlines], 1
	call	string$to_stdout

	pop	rbx
	epilog
cleartext .header, 9,'; ------------------------------------------------------------------------',10\
,9,'; All or parts of this file are a part of the HeavyThing library:',10\
,9,'; Copyright © 2015-2018 2 Ton Digital ',10\
,9,'; Homepage: https://2ton.com.au/',10\
,9,'; Author: Jeff Marrison <jeff@2ton.com.au>',10\
,9,';',10\
,9,'; This file is an intermediate compilation stage of a program that',10\
,9,'; uses the HeavyThing library. As such, it contains direct copies of',10\
,9,'; files from the HeavyThing library, and the HeavyThing license',10\
,9,'; applies equally to this file.',10\
,9,';',10\
,9,'; HeavyThing is free software: you can redistribute it and/or modify',10\
,9,'; it under the terms of the GNU General Public License, or',10\
,9,'; (at your option) any later version.',10\
,9,';',10\
,9,'; HeavyThing is distributed in the hope that it will be useful, ',10\
,9,'; but WITHOUT ANY WARRANTY; without even the implied warranty of',10\
,9,'; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the ',10\
,9,'; GNU General Public License for more details.',10\
,9,';',10\
,9,'; You should have received a copy of the GNU General Public License along',10\
,9,'; with the HeavyThing library. If not, see <http://www.gnu.org/licenses/>.',10\
,9,'; ------------------------------------------------------------------------',10\
,9,';',10\
,9,'; fasmpp input argument: '
cleartext .header2, 9,'; fasmpp inputs: '
cleartext .header3, 9,'; input SHA256: '
cleartext .header4, 9,';',10
cleartext .s_sourcefiles, ' sources, '
cleartext .s_lines, ' lines, '
cleartext .s_bytes, ' bytes.',10



	; single argument: vector of strings (file contents with comment-only lines and preceding whitespace removed)
falign
scansource:
	prolog	scansource
	mov	rsi, .eachsourceline
	mov	rdx, rdi
	call	vector$foreach_arg
	epilog
	; rdi == key, rsi == value (ptr to other vector of contents), rdx == line of our source, rcx == source file it corresponds to

        ; rdi == key, rsi == value (ptr to other vector of contents), rdx == line of our source, rcx == source file it corresponds to
falign
.eachcsymbol:
        ; rdi & rsi are from csymbols itself, we need to see if rdx contains rdi, and if it does, then rcx depends on rsi, and we descend but only if we haven't
        ; already for that rsi
	cmp	rsi, rcx
	je	.eachcsymbol_ret
        push    r12 r13 r14 r15
        mov     r12, rdi
        mov     r13, rsi
        mov     r14, rdx
        mov     r15, rcx
        mov     rsi, rdi
        mov     rdi, rdx
        call    string$indexof
        cmp     rax, 0
        jge     .match
        pop     r15 r14 r13 r12
.eachcsymbol_ret:
        ret
calign
.match:
        ; see if we have already processed this value/rsi/r13
        ; hmmm maybe if we have a unsignedmap that contains unsignedmaps of sources, that'd be a much better plan
        mov     rdi, [dependencies]
        mov     rsi, r13
        call    unsignedmap$find_value
        test    eax, eax
        jz      .match_new
        ; otherwise, we have already seen this, make sure our source file is it its list
        mov     rdi, rdx                ; its unsignedmap value
        mov     rsi, r15                ; the source file that depended on it
        xor     edx, edx                ; don't care about the value here, it is just a set
        call    unsignedmap$insert_unique
        ; done, dusted
        pop     r15 r14 r13 r12
        ret
calign
.match_new:
        ; we have not yet seen this sourcefile, create a new unsignedmap set for it, add it to the dependencies map
        ; and then recurse
        xor     edi, edi                ; sort order is fine
        call    unsignedmap$new
        push    rax
        ; add us to that map
        mov     rdi, rax
        mov     rsi, r15
        xor     edx, edx                ; don't care about the value here, it is just a set
        call    unsignedmap$insert_unique
        pop     rdx
        mov     rdi, [dependencies]
        mov     rsi, r13
        call    unsignedmap$insert_unique
        ; now we can recursive-descend into this new one's dependencies
        mov     rdi, r13
        call    scansource
        ; done, dusted
        pop     r15 r14 r13 r12
        ret
falign
.eachsourceline:
	; rdi == line of our source
	; rsi == vector of strings it belongs to
	; see if we can resolve the dependency chain using just calls
	push	rbx r12 r13
	mov	rbx, rdi
	mov	r12, rsi
	mov	rsi, .s_call
	call	string$starts_with
	test	eax, eax
	jnz	.call

	cmp	qword [used_ansi_colors], 0
	jne	@f
	mov	rdi, rbx
	mov	rsi, .s_ansi_underscore
	call	string$starts_with
	test	eax, eax
	jz	@f
	mov	qword [used_ansi_colors], 1
@@:

	; see if the line contains dh$pool, if so, set use_dh_bits = 1
	mov	rdi, rbx
	mov	rsi, .s_dh_pool
	call	string$indexof
	cmp	rax, 0
	jl	@f
	mov	qword [use_dh_bits], 1
@@:

	pop	r13 r12 rbx
	ret
cleartext .s_call, 'call'
cleartext .s_ansi_underscore, 'ansi_'
cleartext .s_dh_pool, ' dh$pool'
calign
.call:
	; the line started wtih 'call' ... 
        ; .... so now, for every csymbol, see if it is in this line of source
        ; and if it is, use the target of hte csymbol to recursively scansource on it too
        mov     rdi, [csymbols]
        mov     r13, [rdi+_avlofs_next]
calign
.iter:
        mov     rdi, [r13+_avlofs_key]
        mov     rsi, [r13+_avlofs_value]
        mov     rdx, rbx
        mov     rcx, r12
        call    .eachcsymbol
        mov     r13, [r13+_avlofs_next]
        test    r13, r13
        jnz     .iter
        pop     r13 r12 rbx
        ret


falign
emitdependencies:
	prolog	emitdependencies
	; sort the list by # of dependencies ascending

	push	rbx
	xor	edi, edi		; sort order please
	call	unsignedmap$new
	mov	rbx, rax

	; for every item in our dependencies map, add key = # of dependants, value = sourcevec
	mov	rdi, [dependencies]
	mov	rsi, .eachdep
	mov	rdx, rax
	call	unsignedmap$foreach_arg

	; for every one of those in sort order, output
	mov	rdi, rbx
	mov	rsi, .eachemit
	call	unsignedmap$reverse_foreach

	epilog
falign
.eachdep:
	; rdi == sourcevec, rsi == subset, rdx == destination unsignedmap
	xchg	rdi, rdx
	mov	rsi, [rsi+_avlofs_right]	; # node count
	call	unsignedmap$insert		; not unique!!
	ret
falign
.eachemit:
	; rdi == key == # node count
	; rsi == sourcevec
	mov	rdi, rsi
	call	emitsource
	ret

	

falign
find_dh_bits_conf:
	prolog	find_dh_bits_conf
	; search the ht_settings sourcevec for dh_bits =
	mov	rdi, [ht_settings]
	mov	rsi, .eachsetting
	call	vector$foreach
	epilog
falign
.eachsetting:
	push	rdi
	mov	rsi, .s_dh_bits_equals
	call	string$starts_with
	pop	rdi
	test	eax, eax
	jnz	.foundit
	ret
cleartext .s_dh_bits_equals, 'dh_bits = '
calign
.foundit:
	mov	esi, 10
	mov	rdx, -1
	call	string$substr
	push	rax
	mov	rdi, rax
	call	string$to_unsigned
	mov	[conf_dh_bits], rax
	pop	rdi
	call	heap$free
	ret

	
public _start
falign
_start:
	call	ht$init

	cmp	dword [argc], 1
	jbe	.usage

	xor	edi, edi
	call	formatter$new
	mov	[withcommas], rax
	mov	rdi, rax
	mov	esi, 1
	xor	edx, edx
	call	formatter$add_unsigned

	mov	rdi, .s_banner
	call	string$to_stderr

	call	sha256$new
	mov	[inputhash], rax

	call	list$new
	mov	[sourcefiles], rax

	call	list$new
	mov	[ht_sourcefiles], rax

	xor	edi, edi	; sort order
	call	stringmap$new
	mov	[csymbols], rax

	mov	edi, 1		; insert order
	call	unsignedmap$new
	mov	[dependencies], rax

	xor	edi, edi	; sort order
	call	unsignedmap$new
	mov	[emitted], rax

	mov	rdi, [env]
	mov	rsi, .s_pwd
	call	stringmap$find_value
	test	eax, eax
	jz	.nopwd
	; turn that into file:// url
	mov	rdi, .s_filepreface
	mov	rsi, rdx
	call	string$concat
	push	rax
	; add a trailing slash to that
	mov	rdi, rax
	mov	rsi, .s_slash
	call	string$concat
	mov	rdi, [rsp]
	mov	[rsp], rax
	call	heap$free
	; turn that into a url object

	xor	edi, edi
	mov	rsi, [rsp]
	call	url$new
	mov	rdi, .s_nopwd
	test	rax, rax
	jz	.errexit
	mov	r12, rax
	pop	rdi
	call	heap$free
	; url object for the base context sitting in r12

	mov	rdi, [argv]
	call	list$pop_back	
	push	rax
	; create a new url context from that
	mov	rdi, r12	; base context url object
	mov	rsi, rax	; commandline source filename
	call	url$new
	mov	rdi, .s_nodeal
	mov	r13, rax
	test	rax, rax
	jz	.errexit

	mov	rdx, [rax+url_path_ofs]
	mov	[inputargument], rdx
	
	mov	rdi, r13
	call	loadsource

	; output accounting for loaded
	mov	rdi, [withcommas]
	mov	rsi, [sourcefilecount]
	call	formatter$doit
	push	rax
	mov	rdi, rax
	call	string$to_stderr
	pop	rdi
	call	heap$free

	mov	rdi, .s_sourcefiles
	call	string$to_stderr

	mov	rdi, [withcommas]
	mov	rsi, [sourcelines]
	call	formatter$doit
	push	rax
	mov	rdi, rax
	call	string$to_stderr
	pop	rdi
	call	heap$free

	mov	rdi, .s_lines
	call	string$to_stderr

	mov	rdi, [withcommas]
	mov	rsi, [sourcebytes]
	call	formatter$doit
	push	rax
	mov	rdi, rax
	call	string$to_stderr
	pop	rdi
	call	heap$free

	mov	rdi, .s_bytes
	call	string$to_stderr

	mov	rdi, r13
	call	url$destroy
	mov	rdi, r12
	call	url$destroy

	mov	rdi, .s_missing_ht_settings
	cmp	qword [ht_settings], 0
	je	.errexit
	mov	rdi, .s_missing_ht
	cmp	qword [ht], 0
	je	.errexit
	mov	rdi, .s_missing_ht_data
	cmp	qword [ht_data], 0
	je	.errexit

	; so now, we have everything we need... 
	; output our intermediary GPL header for our temporary output
	call	emitheader

	; emit the settings standalone-like
	mov	rdi, [ht_settings]
	call	emitsource

	mov	rdi, [dataseg_macros]
	call	emitsource

	mov	rdi, [align_macros]
	call	emitsource

	mov	rdi, [fcleartext]
	call	emitsource

	mov	rdi, [fsyscall]
	call	emitsource

	mov	rdi, [frdtsc]
	call	emitsource

	mov	rdi, [fsleeps]
	call	emitsource

	mov	rdi, [fbreakpoint]
	call	emitsource

	; the first two lines of [ht] need output next
	repeat 2
		mov	rdi, [ht]
		call	vector$pop_front
		mov	rsi, [rax]
		push	rax
		mov	rdi, rax
		add	rsi, 1
		add	qword [outputlines], 1
		add	qword [outputbytes], rsi
		call	string$to_stdoutln
		pop	rdi
		call	heap$free
	end repeat

	mov	rdi, [profiler]
	call	emitsource

	mov	rdi, [fvdso]
	call	emitsource

	; now, for every non-ht sourcefile, scansource (which will look for csymbols)
	; (the last of which is our command-line passed source file)
	mov	rdi, [sourcefiles]
	mov	rsi, scansource
	call	list$foreach

	cmp	qword [used_ansi_colors], 0
	je	@f
	
	mov	rdi, [fansi_colors]
	call	emitsource

@@:
	; and emit ht.inc (it is sans the include lines)
	mov	rdi, [ht]
	call	emitsource

	cmp	qword [use_dh_bits], 0
	je	.no_dh_pool
	; scan the ht_settings file for whatever dh_bits =
	call	find_dh_bits_conf

	mov	rdi, [fdh_groups]
	call	emitsource

	mov	rdi, [fdh_pool_2k]
	mov	rsi, [fdh_pool_3k]
	mov	rdx, [fdh_pool_4k]
	mov	rcx, [fdh_pool_6k]
	mov	r8, [fdh_pool_8k]
	mov	r9, [fdh_pool_16k]

	cmp	qword [conf_dh_bits], 3072
	cmove	rdi, rsi
	cmp	qword [conf_dh_bits], 4096
	cmove	rdi, rdx
	cmp	qword [conf_dh_bits], 6144
	cmove	rdi, rcx
	cmp	qword [conf_dh_bits], 8192
	cmove	rdi, r8
	cmp	qword [conf_dh_bits], 16384
	cmove	rdi, r9

	call	emitsource

.no_dh_pool:
	call	emitdependencies

	; and the sourcefiles themselves
	mov	rdi, [sourcefiles]
	mov	rsi, emitsource
	call	list$foreach

	; emit the trailing ht_data
	mov	rdi, [ht_data]
	call	emitsource

	; output accounting
	mov	rdi, [withcommas]
	mov	rsi, [outputlines]
	call	formatter$doit
	push	rax
	mov	rdi, rax
	call	string$to_stderr
	pop	rdi
	call	heap$free

	mov	rdi, .s_lines
	call	string$to_stderr

	mov	rdi, [withcommas]
	mov	rsi, [outputbytes]
	call	formatter$doit
	push	rax
	mov	rdi, rax
	call	string$to_stderr
	pop	rdi
	call	heap$free

	mov	rdi, .s_bytes2
	call	string$to_stderrln


	mov	rdi, .s_reduction
	call	string$to_stderr
	
	mov	rdi, [outputlines]
	mov	rsi, [sourcelines]
	call	.dumpperc

	mov	rdi, .s_slash
	call	string$to_stderr

	mov	rdi, [outputbytes]
	mov	rsi, [sourcebytes]
	call	.dumpperc

	mov	rdi, .s_lf
	call	string$to_stderr
	
	
	mov	eax, syscall_exit
	xor	edi, edi
	syscall
.usage:
	mov	rdi, .s_usage
.errexit:
	call	string$to_stderrln
	mov	eax, syscall_exit
	mov	edi, 1
	syscall
.nopwd:
	mov	rdi, .s_nopwd
	jmp	.errexit
cleartext .s_usage, 'usage: fasmpp input.asm'
cleartext .s_banner, 'fasmpp v1.24 ',0xc2,0xa9,' 2018 2 Ton Digital. Author: Jeff Marrison',10,'Processing...'
cleartext .s_sourcefiles, ' files of '
cleartext .s_lines, ' lines and '
cleartext .s_bytes, ' bytes downto '
cleartext .s_bytes2, ' bytes.'
cleartext .s_reduction, 'Reduction lines/bytes: '
cleartext .s_nopwd, 'no PWD environment variable'
cleartext .s_pwd, 'PWD'
cleartext .s_filepreface, 'file://'
cleartext .s_slash, '/'
cleartext .s_lf, 10
cleartext .s_perc, '%'
cleartext .s_nodeal, 'Unable to create URL object from path and PWD'
cleartext .s_missing_ht_settings, 'We did not encounter a HeavyThing settings file.'
cleartext .s_missing_ht, 'We did not encounter a HeavyThing include file.'
cleartext .s_missing_ht_data, 'We did not encounter a HeavyThing data file.'
falign
.dumpperc:
	movq	xmm2, [_math_onehundred]
	cvtsi2sd xmm0, rdi
	cvtsi2sd xmm1, rsi
	mov	edi, 100
	divsd	xmm0, xmm1
	mulsd	xmm0, xmm2
	cvtsd2si rax, xmm0
	sub	edi, eax


	mov	esi, 10
	call	string$from_unsigned
	push	rax
	mov	rdi, rax
	call	string$to_stderr
	pop	rdi
	call	heap$free
	mov	rdi, .s_perc
	call	string$to_stderr
	ret
	

include '../ht_data.inc'
