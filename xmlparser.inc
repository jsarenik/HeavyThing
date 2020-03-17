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
	; xmlparser.inc: A very fast XML parser/navigator that will handle and
	;   parse UTF8, UTF16, and UTF32 (passed as a parameter to xmlparser$new)
	;   Optionally, xmlparser$new_string will take a native string as well.
	;
	; This is a "streaming parser" that allows forward and backward movement
	; through an XML document/fragment (see xmlparser$next and xmlparser$prev)
	; (pull style).
	;
	; Update v1.22+: xmlparser$prev now behaves itself correctly. Previously
	; it choked on <elem attr=">"> and <![CDATA[<<>>>>>>]]> situations, thanks
	; to it paying attention to > and nesting levels, etc. The new version
	; correctly deals with these and now sanely parses them. Additionally,
	; we also deal with unescaped > in text/attrs (messy haha).
	;
	; Since a pointer, length and character width are passed to xmlparser$new,
	; we do not create actual library strings for each, and instead keep
	; start and end offsets for everywhere would-be strings are, and present
	; convenience functions to create library strings from them for when they
	; are desired. (this allows heap$alloc-free parsing/traversal speed)
	;


if used xmltag$new | used xmltag$reset | defined include_everything

; this define sets the upper bound on the maximum number of attributes permitted
; our default here is pretty reasonable, it is rare for an xml element to contain
; more than 32 attributes ... if you encounter the xmlparser_tagattributeoverflow
; error return from the xmlparser, increase this value... 
xmltag_maxattr			= 32

; when unescaping strings, this setting determines max size to use for stack-based
; unescape temporary buffer space (note this size is in characters, not bytes)
xmltag_unescape_stacklimit	= 16384

; defines for possible tag types
xmltag_notype			= 0
xmltag_element			= 1
xmltag_textnode			= 2
xmltag_cdata			= 3
xmltag_processinginstruction	= 4
xmltag_comment			= 5
xmltag_doctypedeclaration	= 6
xmltag_xmldeclaration		= 7

; each individual attribute contains:
xmltagattr_namestart_ofs	= 0		; offset to start of the name
xmltagattr_nameend_ofs		= 8		; offset to end of the name
xmltagattr_valuestart_ofs	= 16		; offset to start of value
xmltagattr_valueend_ofs		= 24		; offset to end of value
xmltagattr_namecolon_ofs	= 32		; dword offset (relative to nameend-namestart) of colon in the attribute name (-1 otherwise)

xmltagattr_size = 40				; wasted 4 bytes each

; and our xmltag itself needs:
xmltag_textstart_ofs		= 0		; offset to start of tag text
xmltag_textend_ofs		= 8		; offset to start of tag text end
xmltag_textcolon_ofs		= 16		; dword offset (relative to textend-textstart) of colon in the tag name (-1 otherwise)
xmltag_empty_ofs		= 20		; dword bool whether tag is empty or not (think <br/>)
xmltag_nodetype_ofs		= 24		; dword one of the 8 nodetypes listed above
xmltag_attrcount_ofs		= 28		; dword count of how many attributes we have
xmltag_realstart_ofs		= 32		; offset to the real start of the tag
xmltag_realend_ofs		= 40		; offset to the real end of the tag
xmltag_base_ofs			= 48		; copy of the base pointer from the xmlparser
xmltag_width_ofs		= 56		; copy of the width setting from the xmlparser (wasting 4 bytes)
xmltag_attributes_ofs		= 64		; xmltag_maxattr * xmltagattr_size bytes

xmltag_size = xmltag_attributes_ofs + (xmltag_maxattr * xmltagattr_size)

	; NOTE: this can of course also be stack allocated, but you should call xmltag$reset on it before use

	; no arguments, heap$alloc's and initialises an xmltag object
	; returns new xmltag object
falign
xmltag$new:
	prolog	xmltag$new
	mov	edi, xmltag_size
	call	heap$alloc
	xor	ecx, ecx
	mov	edx, -1
	mov	[rax+xmltag_textstart_ofs], rcx
	mov	[rax+xmltag_textend_ofs], rcx
	mov	[rax+xmltag_textcolon_ofs], rdx ; writes 0 into empty too
	mov	[rax+xmltag_nodetype_ofs], rcx	; writes over attrcount too
	epilog

end if


if used xmltag$destroy | defined include_everything
	; placeholder function only really, just does a heap$free
	; single argument in rdi: an xmltag object
falign
xmltag$destroy:
	prolog	xmltag$destroy
	call	heap$free
	epilog

end if


if used xmltag$reset | defined include_everything
	; single argument in rdi: an xmltag object to reset
falign
xmltag$reset:
	prolog	xmltag$reset
	xor	ecx, ecx
	mov	edx, -1
	mov	[rdi+xmltag_textstart_ofs], rcx
	mov	[rdi+xmltag_textend_ofs], rcx
	mov	[rdi+xmltag_textcolon_ofs], rdx ; writes 0 into empty too
	mov	[rdi+xmltag_nodetype_ofs], rcx	; writes over attrcount too
	epilog

end if


if used xmltag$newattr | defined include_everything
	; six arguments: rdi == xmltag object, rsi == namestart, rdx == nameend, ecx == colonpos, r8 == valuestart, r9 == valueend
	; returns bool in eax as to whether we succeeded or not (0 == maxattr reached)
	; (leaves rdx as a pointer to the xmltagattr structure if eax = 1)
falign
xmltag$newattr:
	prolog	xmltag$newattr
	mov	eax, [rdi+xmltag_attrcount_ofs]
	cmp	eax, xmltag_maxattr
	je	.zeroret
	imul	eax, eax, xmltagattr_size
	add	dword [rdi+xmltag_attrcount_ofs], 1
	; offset into rdi for our new attribute is rdi+xmltag_attributes_ofs+rax
	lea	rax, [rdi+rax+xmltag_attributes_ofs]
	mov	[rax+xmltagattr_namestart_ofs], rsi
	mov	[rax+xmltagattr_nameend_ofs], rdx
	mov	[rax+xmltagattr_valuestart_ofs], r8
	mov	[rax+xmltagattr_valueend_ofs], r9
	mov	[rax+xmltagattr_namecolon_ofs], rcx
	mov	rdx, rax
	mov	eax, 1
	epilog
.zeroret:
	xor	eax, eax
	epilog

end if


if used xmltag$getattr | defined include_everything
	; two arguments: rdi == xmltag object, esi == attribute # to get
	; returns pointer to xmltagattr structure for esi or 0 on error
falign
xmltag$getattr:
	prolog	xmltag$getattr
	cmp	esi, [rdi+xmltag_attrcount_ofs]
	jae	.zeroret
	imul	esi, esi, xmltagattr_size
	lea	rax, [rdi+rsi+xmltag_attributes_ofs]
	epilog
.zeroret:
	xor	eax, eax
	epilog

end if


if used xmltag$escape_string | defined include_everything
	; single argument in rdi: string to escape
	; returns a new heap$alloc'd string of escaped text
	; note that this is only for the 5 lt, gt, amp, apos, quot
falign
xmltag$escape_string:
	prolog	xmltag$escape_string
	lea	rsi, [rdi+8]
	mov	rcx, [rdi]
	xor	edx, edx
	; determine the size we need first
	mov	r9d, 4
	mov	r10d, 5
	mov	r11d, 6
calign
.firstpass:
if string_bits = 32
	mov	eax, [rsi]
	add	rsi, 4
else
	movzx	eax, word [rsi]
	add	rsi, 2
end if
	mov	r8d, 1
	cmp	eax, '<'
	cmove	r8d, r9d
	cmp	eax, '>'
	cmove	r8d, r9d
	cmp	eax, '&'
	cmove	r8d, r10d
	cmp	eax, 0x27
	cmove	r8d, r11d
	cmp	eax, '"'
	cmove	r8d, r11d
	add	rdx, r8
	sub	rcx, 1
	jnz	.firstpass
	; so now rdx has the new string length we are after
	push	rdx
if string_bits = 32
	shl	rdx, 2
else
	shl	rdx, 1
end if
	push	rdi
	add	rdx, 8
	mov	rdi, rdx
	call	heap$alloc
	pop	rsi
	mov	rdi, rax
	mov	rcx, [rsi]
	add	rsi, 8
	pop	rdx
	mov	[rdi], rdx
	add	rdi, 8
calign
.secondpass:
if string_bits = 32
	mov	edx, [rsi]
	add	rsi, 4
else
	movzx	edx, word [rsi]
	add	rsi, 2
end if
	cmp	edx, '<'
	je	.lt
	cmp	edx, '>'
	je	.gt
	cmp	edx, '&'
	je	.amp
	cmp	edx, 0x27
	je	.apos
	cmp	edx, '"'
	je	.quot
if string_bits = 32
	mov	[rdi], edx
	add	rdi, 4
else
	mov	[rdi], dx
	add	rdi, 2
end if
	sub	rcx, 1
	jnz	.secondpass
	epilog
calign
.lt:
if string_bits = 32
	mov	dword [rdi], '&'
	mov	dword [rdi+4], 'l'
	mov	dword [rdi+8], 't'
	mov	dword [rdi+12], ';'
	add	rdi, 16
else
	mov	word [rdi], '&'
	mov	word [rdi+2], 'l'
	mov	word [rdi+4], 't'
	mov	word [rdi+6], ';'
	add	rdi, 8
end if
	sub	rcx, 1
	jnz	.secondpass
	epilog
calign
.gt:
if string_bits = 32
	mov	dword [rdi], '&'
	mov	dword [rdi+4], 'g'
	mov	dword [rdi+8], 't'
	mov	dword [rdi+12], ';'
	add	rdi, 16
else
	mov	word [rdi], '&'
	mov	word [rdi+2], 'g'
	mov	word [rdi+4], 't'
	mov	word [rdi+6], ';'
	add	rdi, 8
end if
	sub	rcx, 1
	jnz	.secondpass
	epilog
calign
.amp:
if string_bits = 32
	mov	dword [rdi], '&'
	mov	dword [rdi+4], 'a'
	mov	dword [rdi+8], 'm'
	mov	dword [rdi+12], 'p'
	mov	dword [rdi+16], ';'
	add	rdi, 20
else
	mov	word [rdi], '&'
	mov	word [rdi+2], 'a'
	mov	word [rdi+4], 'm'
	mov	word [rdi+6], 'p'
	mov	word [rdi+8], ';'
	add	rdi, 10
end if
	sub	rcx, 1
	jnz	.secondpass
	epilog
calign
.apos:
if string_bits = 32
	mov	dword [rdi], '&'
	mov	dword [rdi+4], 'a'
	mov	dword [rdi+8], 'p'
	mov	dword [rdi+12], 'o'
	mov	dword [rdi+16], 's'
	mov	dword [rdi+20], ';'
	add	rdi, 24
else
	mov	word [rdi], '&'
	mov	word [rdi+2], 'a'
	mov	word [rdi+4], 'p'
	mov	word [rdi+6], 'o'
	mov	word [rdi+8], 's'
	mov	word [rdi+10], ';'
	add	rdi, 12
end if
	sub	rcx, 1
	jnz	.secondpass
	epilog
calign
.quot:
if string_bits = 32
	mov	dword [rdi], '&'
	mov	dword [rdi+4], 'q'
	mov	dword [rdi+8], 'u'
	mov	dword [rdi+12], 'o'
	mov	dword [rdi+16], 't'
	mov	dword [rdi+20], ';'
	add	rdi, 24
else
	mov	word [rdi], '&'
	mov	word [rdi+2], 'q'
	mov	word [rdi+4], 'u'
	mov	word [rdi+6], 'o'
	mov	word [rdi+8], 't'
	mov	word [rdi+10], ';'
	add	rdi, 12
end if
	sub	rcx, 1
	jnz	.secondpass
	epilog


end if



if used xmltag$unescape | defined include_everything
	; three arguments: rdi == xmltag object, rsi == text start offset, rdx == text end offset
	; returns a new heap$alloc'd string of unescaped text
falign
xmltag$unescape:
	prolog	xmltag$unescape
	mov	r8, [rdi+xmltag_base_ofs]
	mov	r9d, [rdi+xmltag_width_ofs]
	mov	rcx, rdx
	lea	r10, [rsi*2]
	lea	r11, [rsi*4]
	sub	rcx, rsi
	jz	.emptystring
	cmp	r9d, 1
	cmove	rsi, r10
	cmp	r9d, 2
	cmove	rsi, r11
	mov	r10, rcx
	lea	rax, [r8+rsi]
	jmp	qword [r9*8+.dispatch]
dalign
.dispatch:
	dq	.utf8, .utf16, .utf32
calign
.emptystring:
	call	string$new
	epilog
calign
.utf8:
	; prescan the string searching for ampersand
	cmp	byte [rax], '&'
	je	.utf8_unescape
	add	rax, 1
	sub	rcx, 1
	jnz	.utf8
	; if we made it here, no ampersands were found
	lea	rdi, [r8+rsi]
	mov	rsi, r10
	call	string$from_utf8
	epilog
calign
.utf16:
	; prescan the string searching for ampersand
	cmp	word [rax], '&'
	je	.utf16_unescape
	add	rax, 2
	sub	rcx, 1
	jnz	.utf16
	; if we made it here, no ampersands were found
	lea	rdi, [r8+rsi]
	lea	rsi, [r10*2]
	call	string$from_utf16
	epilog
calign
.utf32:
	; prescan the string searching for ampersand
	cmp	dword [rax], '&'
	je	.utf32_unescape
	add	rax, 4
	sub	rcx, 1
	jnz	.utf32
	; if we made it here, no ampersands were found
	lea	rdi, [r8+rsi]
	lea	rsi, [r10*4]
	call	string$from_utf32
	epilog
calign
.utf8_unescape:
	; if our text is small enough, unescape on the stack
	; otherwise, do expensive buffer construction
	cmp	r10, xmltag_unescape_stacklimit
	ja	.utf8_unescape_buffer
	mov	rdx, r10
	push	rbx r12 r13 r14
	xor	r14d, r14d
	sub	rsp, xmltag_unescape_stacklimit
	lea	rsi, [r8+rsi]			; beginning of string
	sub	rdx, rcx
	mov	rdi, rsp
	mov	r12, rax			; pointer to first ampersand
	mov	r13, rcx			; bytes left
	lea	rbx, [rsp+rdx]
	call	memcpy
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
calign
.utf8_unescape_semicolon_scan:
	cmp	byte [rsi+rdx], ';'
	je	.utf8_unescape_semicolon
	add	rdx, 1
	sub	rcx, 1
	jnz	.utf8_unescape_semicolon_scan
	; if we made it to here, an ampersand was found but no trailing semicolon
	; despite this being an error, just return the remaining of the string as-is
	mov	rdi, rbx
	mov	rsi, r12
	mov	rdx, r13
	add	rbx, r13
	call	memcpy
.utf8_unescape_return:
	test	r14, r14
	jnz	.utf8_unescape_buffer_return
	; rbx - rsp is how many bytes we have accumulated
	mov	rdi, rsp
	mov	rsi, rbx
	sub	rsi, rsp
	call	string$from_utf8
	add	rsp, xmltag_unescape_stacklimit
	pop	r14 r13 r12 rbx
	epilog
calign
.utf8_unescape_buffer_return:
	mov	rdi, r14
	mov	rsi, rbx
	sub	rsi, r14
	call	string$from_utf8
	mov	rdi, r14
	mov	r14, rax
	call	heap$free
	mov	rax, r14
	pop	r14 r13 r12 rbx
	epilog
calign
.utf8_unescape_semicolon:
	; rdx is our length
	cmp	byte [rsi+1], '#'
	je	.utf8_unescape_codepoint
	cmp	rdx, 3
	je	.utf8_unescape_ltgt
	cmp	rdx, 4
	je	.utf8_unescape_ampersand
	cmp	rdx, 5
	jne	.utf8_unescape_straightin
	; quot or apos check
	mov	eax, '"'
	cmp	dword [rsi+1], 'quot'
	je	.utf8_unescape_char
	mov	eax, 0x27
	cmp	dword [rsi+1], 'apos'
	je	.utf8_unescape_char
	mov	eax, 0xa0
	cmp	dword [rsi+1], 'nbsp'
	je	.utf8_unescape_char
.utf8_unescape_straightin:
	; add from r12 to r12+rdx+1
	mov	rdi, rbx
	mov	rsi, r12
	add	rdx, 1
	add	rbx, rdx
	add	r12, rdx
	sub	r13, rdx
	call	memcpy
	test	r13, r13
	jz	.utf8_unescape_return
	; find the next ampersand
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
calign
.utf8_unescape_amp_scan:
	cmp	byte [rsi+rdx], '&'
	je	.utf8_unescape_amp
	add	rdx, 1
	sub	rcx, 1
	jnz	.utf8_unescape_amp_scan
	; if we made it to here no ampersand was found
	test	rdx, rdx
	jz	.utf8_unescape_return
	mov	rdi, rbx
	mov	rsi, r12
	mov	rdx, r13
	add	rbx, r13
	call	memcpy
	jmp	.utf8_unescape_return
calign
.utf8_unescape_codepoint:
	; word at [rsi] == '&#'
	; byte at [rsi+rdx] == ';'
	lea	rdi, [rsi+2]
	lea	rcx, [rdx-2]
	cmp	rdx, 2			; if it was &#; put it straight in as-is
	je	.utf8_unescape_straightin
	cmp	byte [rdi], 'x'
	je	.utf8_unescape_codepoint_base16
	xor	eax, eax
calign
.utf8_unescape_codepoint_loop:
	movzx	r8d, byte [rdi]
	add	rdi, 1
	sub	r8d, '0'
	cmp	r8d, 10
	jae	.utf8_unescape_straightin
	imul	eax, eax, 10
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf8_unescape_codepoint_loop
.utf8_unescape_codepoint_ready:
	cmp	eax, 256
	jae	.utf8_unescape_codepoint_encoded
	mov	byte [rbx], al
	add	rbx, 1
	add	rdx, 1
	add	r12, rdx
	sub	r13, rdx
	jz	.utf8_unescape_return
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf8_unescape_amp_scan
calign
.utf8_unescape_codepoint_encoded:
	; we can cheat here at create a string object below our current stackframe:
	mov	qword [rsp-64], 1
	mov	qword [rsp-56], rax
	lea	rdi, [rsp-64]
	mov	rsi, rbx
	; setup values for next round:
	add	rdx, 1
	add	r12, rdx
	sub	r13, rdx
	call	string$to_utf8
	add	rbx, rax
	test	r13, r13
	jz	.utf8_unescape_return
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf8_unescape_amp_scan
calign
.utf8_unescape_codepoint_base16:
	add	rdi, 1
	sub	rcx, 1
	jz	.utf8_unescape_straightin	; &#x;
	xor	eax, eax
calign
.utf8_unescape_codepoint_base16_loop:
	movzx	r8d, byte [rdi]
	add	rdi, 1
	cmp	r8d, 'a'
	jae	.utf8_unescape_codepoint_base16_lc
	cmp	r8d, 'A'
	jae	.utf8_unescape_codepoint_base16_uc
	sub	r8d, '0'
	cmp	r8d, 16
	jae	.utf8_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf8_unescape_codepoint_base16_loop
	jmp	.utf8_unescape_codepoint_ready
calign
.utf8_unescape_codepoint_base16_uc:
	sub	r8d, '0'+7
	cmp	r8d, 16
	jae	.utf8_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf8_unescape_codepoint_base16_loop
	jmp	.utf8_unescape_codepoint_ready
calign
.utf8_unescape_codepoint_base16_lc:
	sub	r8d, '0'+7+0x20
	cmp	r8d, 16
	jae	.utf8_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf8_unescape_codepoint_base16_loop
	jmp	.utf8_unescape_codepoint_ready
calign
.utf8_unescape_ampersand:
	mov	eax, '&'
	cmp	dword [rsi], '&amp'
	je	.utf8_unescape_char
	jmp	.utf8_unescape_straightin
calign
.utf8_unescape_ltgt:
	mov	eax, '<'
	cmp	dword [rsi], '&lt;'
	je	.utf8_unescape_char
	mov	eax, '>'
	cmp	dword [rsi], '&gt;'
	jne	.utf8_unescape_straightin
	; fall through to utf8_unescape_char
calign
.utf8_unescape_char:
	mov	[rbx], al
	add	rdx, 1
	add	rbx, 1
	add	r12, rdx
	sub	r13, rdx
	jz	.utf8_unescape_return
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf8_unescape_amp_scan
calign
.utf8_unescape_amp:
	; if rdx is 0, no memcpy
	test	rdx, rdx
	jz	.utf8_unescape_amp_nocopy
	; otherwise, rdx bytes go straight in
	mov	rdi, rbx
	mov	rsi, r12
	add	rbx, rdx
	add	r12, rdx
	sub	r13, rdx
	call	memcpy
.utf8_unescape_amp_nocopy:
	; so now the byte at r12 is an ampersand
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf8_unescape_semicolon_scan
calign
.utf8_unescape_buffer:
	; string is too big to do on the stack, so create a temporary buffer for it
	mov	rdx, r10
	push	rbx r12 r13 r14
	lea	rsi, [r8+rsi]		; beginning of string
	sub	rdx, rcx
	mov	r12, rax		; pointer to first ampersand
	mov	r13, rcx		; bytes left
	push	rsi rdx
	mov	rdi, r10
	call	heap$alloc
	mov	r14, rax
	pop	rdx rsi
	mov	rdi, rax
	lea	rbx, [r14+rdx]
	call	memcpy
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf8_unescape_semicolon_scan



calign
.utf16_unescape:
	; if our text is small enough, unescape on the stack
	; otherwise, do expensive buffer construction
	cmp	r10, xmltag_unescape_stacklimit
	ja	.utf16_unescape_buffer
	mov	rdx, r10
	push	rbx r12 r13 r14
	xor	r14d, r14d
	sub	rsp, xmltag_unescape_stacklimit shl 1
	lea	rsi, [r8+rsi]			; beginning of string
	sub	rdx, rcx
	mov	rdi, rsp
	mov	r12, rax			; pointer to first ampersand
	mov	r13, rcx			; characters left
	lea	rbx, [rsp+rdx*2]
	shl	rdx, 1
	call	memcpy
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
calign
.utf16_unescape_semicolon_scan:
	cmp	word [rsi+rdx*2], ';'
	je	.utf16_unescape_semicolon
	add	rdx, 1
	sub	rcx, 1
	jnz	.utf16_unescape_semicolon_scan
	; if we made it to here, an ampersand was found but no trailing semicolon
	; despite this being an error, just return the remaining of the string as-is
	; r13 is the number of characters left not bytes:
	shl	r13, 1
	mov	rdi, rbx
	mov	rsi, r12
	mov	rdx, r13
	add	rbx, r13
	call	memcpy
.utf16_unescape_return:
	test	r14, r14
	jnz	.utf16_unescape_buffer_return
	; rbx - rsp is how many bytes we have accumulated
	mov	rdi, rsp
	mov	rsi, rbx
	sub	rsi, rsp
	call	string$from_utf16
	add	rsp, xmltag_unescape_stacklimit shl 1
	pop	r14 r13 r12 rbx
	epilog
calign
.utf16_unescape_buffer_return:
	mov	rdi, r14
	mov	rsi, rbx
	sub	rsi, r14
	call	string$from_utf16
	mov	rdi, r14
	mov	r14, rax
	call	heap$free
	mov	rax, r14
	pop	r14 r13 r12 rbx
	epilog
dalign
.qquot:
	dw	'q', 'u', 'o', 't'
.qapos:
	dw	'a', 'p', 'o', 's'
.qnbsp:
	dw	'n', 'b', 's', 'p'
calign
.utf16_unescape_semicolon:
	; rdx is our length
	cmp	word [rsi+2], '#'
	je	.utf16_unescape_codepoint
	cmp	rdx, 3
	je	.utf16_unescape_ltgt
	cmp	rdx, 4
	je	.utf16_unescape_ampersand
	cmp	rdx, 5
	jne	.utf16_unescape_straightin
	; quot or apos check
	mov	eax, '"'
	mov	r8, [rsi+2]
	cmp	r8, [.qquot]
	je	.utf16_unescape_char
	mov	eax, 0x27
	cmp	r8, [.qapos]
	je	.utf16_unescape_char
	mov	eax, 0xa0
	cmp	r8, [.qnbsp]
	je	.utf16_unescape_char
.utf16_unescape_straightin:
	; add from r12 to r12+rdx+1
	mov	rdi, rbx
	mov	rsi, r12
	add	rdx, 1
	sub	r13, rdx
	shl	rdx, 1
	add	rbx, rdx
	add	r12, rdx
	call	memcpy
	test	r13, r13
	jz	.utf16_unescape_return
	; find the next ampersand
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
calign
.utf16_unescape_amp_scan:
	cmp	word [rsi+rdx*2], '&'
	je	.utf16_unescape_amp
	add	rdx, 1
	sub	rcx, 1
	jnz	.utf16_unescape_amp_scan
	; if we made it to here no ampersand was found
	test	rdx, rdx
	jz	.utf16_unescape_return
	; r13 is number of characters left:
	shl	r13, 1
	mov	rdi, rbx
	mov	rsi, r12
	mov	rdx, r13
	add	rbx, r13
	call	memcpy
	jmp	.utf16_unescape_return
calign
.utf16_unescape_codepoint:
	; dword at [rsi] == '&#'
	; word at [rsi+rdx*2] == ';'
	lea	rdi, [rsi+4]
	lea	rcx, [rdx-2]
	cmp	rdx, 2			; if it was &#; put it straight in as-is
	je	.utf16_unescape_straightin
	cmp	word [rdi], 'x'
	je	.utf16_unescape_codepoint_base16
	xor	eax, eax
calign
.utf16_unescape_codepoint_loop:
	movzx	r8d, word [rdi]
	add	rdi, 2
	sub	r8d, '0'
	cmp	r8d, 10
	jae	.utf16_unescape_straightin
	imul	eax, eax, 10
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf16_unescape_codepoint_loop
.utf16_unescape_codepoint_ready:
	cmp	eax, 0xd800
	jae	.utf16_unescape_codepoint_encoded
	mov	word [rbx], ax
	add	rbx, 2
	add	rdx, 1
	sub	r13, rdx
	shl	rdx, 1
	add	r12, rdx
	test	r13, r13
	jz	.utf16_unescape_return
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf16_unescape_amp_scan
calign
.utf16_unescape_codepoint_encoded:
	; we can cheat here at create a string object below our current stackframe:
	mov	qword [rsp-64], 1
	mov	qword [rsp-56], rax
	lea	rdi, [rsp-64]
	mov	rsi, rbx
	; setup values for next round:
	add	rdx, 1
	sub	r13, rdx
	shl	rdx, 1
	add	r12, rdx
	call	string$to_utf16
	shl	rax, 1
	add	rbx, rax
	test	r13, r13
	jz	.utf16_unescape_return
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf16_unescape_amp_scan
calign
.utf16_unescape_codepoint_base16:
	add	rdi, 2
	sub	rcx, 1
	jz	.utf16_unescape_straightin	; &#x;
	xor	eax, eax
calign
.utf16_unescape_codepoint_base16_loop:
	movzx	r8d, word [rdi]
	add	rdi, 2
	cmp	r8d, 'a'
	jae	.utf16_unescape_codepoint_base16_lc
	cmp	r8d, 'A'
	jae	.utf16_unescape_codepoint_base16_uc
	sub	r8d, '0'
	cmp	r8d, 16
	jae	.utf16_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf16_unescape_codepoint_base16_loop
	jmp	.utf16_unescape_codepoint_ready
calign
.utf16_unescape_codepoint_base16_uc:
	sub	r8d, '0'+7
	cmp	r8d, 16
	jae	.utf16_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf16_unescape_codepoint_base16_loop
	jmp	.utf16_unescape_codepoint_ready
calign
.utf16_unescape_codepoint_base16_lc:
	sub	r8d, '0'+7+0x20
	cmp	r8d, 16
	jae	.utf16_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf16_unescape_codepoint_base16_loop
	jmp	.utf16_unescape_codepoint_ready
dalign
.qamp:
	dw	'&', 'a', 'm', 'p'
.qlt:
	dw	'&', 'l', 't', ';'
.qgt:
	dw	'&', 'g', 't', ';'
calign
.utf16_unescape_ampersand:
	mov	r8, [.qamp]
	mov	eax, '&'
	cmp	r8, [rsi]
	je	.utf16_unescape_char
	jmp	.utf16_unescape_straightin
calign
.utf16_unescape_ltgt:
	mov	r8, [rsi]
	mov	eax, '<'
	cmp	r8, [.qlt]
	je	.utf16_unescape_char
	mov	eax, '>'
	cmp	r8, [.qgt]
	jne	.utf16_unescape_straightin
	; fall through to utf16_unescape_char
calign
.utf16_unescape_char:
	mov	[rbx], ax
	add	rdx, 1
	sub	r13, rdx
	shl	rdx, 1
	add	rbx, 2
	add	r12, rdx
	test	r13, r13
	jz	.utf16_unescape_return
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf16_unescape_amp_scan
calign
.utf16_unescape_amp:
	; if rdx is 0, no memcpy
	test	rdx, rdx
	jz	.utf16_unescape_amp_nocopy
	; otherwise, rdx characters go straight in
	mov	rdi, rbx
	mov	rsi, r12
	sub	r13, rdx
	shl	rdx, 1
	add	rbx, rdx
	add	r12, rdx
	call	memcpy
.utf16_unescape_amp_nocopy:
	; so now the word at r12 is an ampersand
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf16_unescape_semicolon_scan
calign
.utf16_unescape_buffer:
	; string is too big to do on the stack, so create a temporary buffer for it
	mov	rdx, r10
	push	rbx r12 r13 r14
	lea	rsi, [r8+rsi]		; beginning of string
	sub	rdx, rcx
	mov	r12, rax		; pointer to first ampersand
	mov	r13, rcx		; characters left
	push	rsi rdx
	mov	rdi, r10
	shl	rdi, 1
	call	heap$alloc
	mov	r14, rax
	pop	rdx rsi
	mov	rdi, rax
	lea	rbx, [r14+rdx]
	shl	rdx, 1
	call	memcpy
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf16_unescape_semicolon_scan


calign
.utf32_unescape:
	; if our text is small enough, unescape on the stack
	; otherwise, do expensive buffer construction
	cmp	r10, xmltag_unescape_stacklimit
	ja	.utf32_unescape_buffer
	mov	rdx, r10
	push	rbx r12 r13 r14
	xor	r14d, r14d
	sub	rsp, xmltag_unescape_stacklimit shl 2
	lea	rsi, [r8+rsi]			; beginning of string
	sub	rdx, rcx
	mov	rdi, rsp
	mov	r12, rax			; pointer to first ampersand
	mov	r13, rcx			; characters left
	lea	rbx, [rsp+rdx*4]
	shl	rdx, 2
	call	memcpy
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
calign
.utf32_unescape_semicolon_scan:
	cmp	dword [rsi+rdx*4], ';'
	je	.utf32_unescape_semicolon
	add	rdx, 1
	sub	rcx, 1
	jnz	.utf32_unescape_semicolon_scan
	; if we made it to here, an ampersand was found but no trailing semicolon
	; despite this being an error, just return the remaining of the string as-is
	; r13 is the number of characters left not bytes:
	shl	r13, 2
	mov	rdi, rbx
	mov	rsi, r12
	mov	rdx, r13
	add	rbx, r13
	call	memcpy
.utf32_unescape_return:
	test	r14, r14
	jnz	.utf32_unescape_buffer_return
	; rbx - rsp is how many bytes we have accumulated
	mov	rdi, rsp
	mov	rsi, rbx
	sub	rsi, rsp
	call	string$from_utf32
	add	rsp, xmltag_unescape_stacklimit shl 2
	pop	r14 r13 r12 rbx
	epilog
calign
.utf32_unescape_buffer_return:
	mov	rdi, r14
	mov	rsi, rbx
	sub	rsi, r14
	call	string$from_utf32
	mov	rdi, r14
	mov	r14, rax
	call	heap$free
	mov	rax, r14
	pop	r14 r13 r12 rbx
	epilog
dalign
.dqquot:
	dd	'q', 'u', 'o', 't'
.dqapos:
	dd	'a', 'p', 'o', 's'
.dqnbsp:
	dd	'n', 'b', 's', 'p'
calign
.utf32_unescape_maybequot:
	cmp	r9, [.dqquot+8]
	jne	.utf32_unescape_notquot
	jmp	.utf32_unescape_char
calign
.utf32_unescape_maybeapos:
	cmp	r9, [.dqapos+8]
	jne	.utf32_unescape_notapos
	jmp	.utf32_unescape_char
calign
.utf32_unescape_maybenbsp:
	cmp	r9, [.dqnbsp+8]
	jne	.utf32_unescape_straightin
	jmp	.utf32_unescape_char
calign
.utf32_unescape_semicolon:
	; rdx is our length
	cmp	dword [rsi+4], '#'
	je	.utf32_unescape_codepoint
	cmp	rdx, 3
	je	.utf32_unescape_ltgt
	cmp	rdx, 4
	je	.utf32_unescape_ampersand
	cmp	rdx, 5
	jne	.utf32_unescape_straightin
	; quot or apos check
	mov	eax, '"'
	mov	r8, [rsi+4]
	mov	r9, [rsi+12]

	cmp	r8, [.dqquot]
	je	.utf32_unescape_maybequot
.utf32_unescape_notquot:
	mov	eax, 0x27
	cmp	r8, [.dqapos]
	je	.utf32_unescape_maybeapos
.utf32_unescape_notapos:
	mov	eax, 0xa0
	cmp	r8, [.dqnbsp]
	je	.utf32_unescape_maybenbsp
.utf32_unescape_straightin:
	; add from r12 to r12+rdx+1
	mov	rdi, rbx
	mov	rsi, r12
	add	rdx, 1
	sub	r13, rdx
	shl	rdx, 2
	add	rbx, rdx
	add	r12, rdx
	call	memcpy
	test	r13, r13
	jz	.utf32_unescape_return
	; find the next ampersand
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
calign
.utf32_unescape_amp_scan:
	cmp	dword [rsi+rdx*4], '&'
	je	.utf32_unescape_amp
	add	rdx, 1
	sub	rcx, 1
	jnz	.utf32_unescape_amp_scan
	; if we made it to here no ampersand was found
	test	rdx, rdx
	jz	.utf32_unescape_return
	; r13 is number of characters left:
	shl	r13, 2
	mov	rdi, rbx
	mov	rsi, r12
	mov	rdx, r13
	add	rbx, r13
	call	memcpy
	jmp	.utf32_unescape_return
calign
.utf32_unescape_codepoint:
	; qword at [rsi] == '&#'
	; dword at [rsi+rdx*4] == ';'
	lea	rdi, [rsi+8]
	lea	rcx, [rdx-2]
	cmp	rdx, 2			; if it was &#; put it straight in as-is
	je	.utf32_unescape_straightin
	cmp	dword [rdi], 'x'
	je	.utf32_unescape_codepoint_base16
	xor	eax, eax
calign
.utf32_unescape_codepoint_loop:
	mov	r8d, dword [rdi]
	add	rdi, 4
	sub	r8d, '0'
	cmp	r8d, 10
	jae	.utf32_unescape_straightin
	imul	eax, eax, 10
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf32_unescape_codepoint_loop
.utf32_unescape_codepoint_ready:
	mov	dword [rbx], eax
	add	rbx, 4
	add	rdx, 1
	sub	r13, rdx
	shl	rdx, 2
	add	r12, rdx
	test	r13, r13
	jz	.utf32_unescape_return
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf32_unescape_amp_scan
calign
.utf32_unescape_codepoint_base16:
	add	rdi, 4
	sub	rcx, 1
	jz	.utf32_unescape_straightin	; &#x;
	xor	eax, eax
calign
.utf32_unescape_codepoint_base16_loop:
	mov	r8d, dword [rdi]
	add	rdi, 4
	cmp	r8d, 'a'
	jae	.utf32_unescape_codepoint_base16_lc
	cmp	r8d, 'A'
	jae	.utf32_unescape_codepoint_base16_uc
	sub	r8d, '0'
	cmp	r8d, 16
	jae	.utf32_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf32_unescape_codepoint_base16_loop
	jmp	.utf32_unescape_codepoint_ready
calign
.utf32_unescape_codepoint_base16_uc:
	sub	r8d, '0'+7
	cmp	r8d, 16
	jae	.utf32_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf32_unescape_codepoint_base16_loop
	jmp	.utf32_unescape_codepoint_ready
calign
.utf32_unescape_codepoint_base16_lc:
	sub	r8d, '0'+7+0x20
	cmp	r8d, 16
	jae	.utf32_unescape_straightin
	imul	eax, eax, 16
	add	eax, r8d
	sub	rcx, 1
	jnz	.utf32_unescape_codepoint_base16_loop
	jmp	.utf32_unescape_codepoint_ready
dalign
.dqamp:
	dd	'&', 'a', 'm', 'p'
.dqlt:
	dd	'&', 'l', 't', ';'
.dqgt:
	dd	'&', 'g', 't', ';'
calign
.utf32_unescape_maybeamp:
	cmp	r9, [rsi+8]
	je	.utf32_unescape_char
	jmp	.utf32_unescape_straightin
calign
.utf32_unescape_ampersand:
	mov	r8, [.dqamp]
	mov	r9, [.dqamp+8]
	mov	eax, '&'
	cmp	r8, [rsi]
	je	.utf32_unescape_maybeamp
	jmp	.utf32_unescape_straightin
calign
.utf32_unescape_maybelt:
	cmp	r9, [.dqlt+8]
	je	.utf32_unescape_char
	jmp	.utf32_unescape_notlt
calign
.utf32_unescape_maybegt:
	cmp	r9, [.dqgt+8]
	je	.utf32_unescape_char
	jmp	.utf32_unescape_straightin
calign
.utf32_unescape_ltgt:
	mov	r8, [rsi]
	mov	r9, [rsi+8]
	mov	eax, '<'
	cmp	r8, [.dqlt]
	je	.utf32_unescape_maybelt
.utf32_unescape_notlt:
	mov	eax, '>'
	cmp	r8, [.dqgt]
	je	.utf32_unescape_maybegt
	jmp	.utf32_unescape_straightin
calign
.utf32_unescape_char:
	mov	[rbx], eax
	add	rdx, 1
	sub	r13, rdx
	shl	rdx, 2
	add	rbx, 4
	add	r12, rdx
	test	r13, r13
	jz	.utf32_unescape_return
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf32_unescape_amp_scan
calign
.utf32_unescape_amp:
	; if rdx is 0, no memcpy
	test	rdx, rdx
	jz	.utf32_unescape_amp_nocopy
	; otherwise, rdx characters go straight in
	mov	rdi, rbx
	mov	rsi, r12
	sub	r13, rdx
	shl	rdx, 2
	add	rbx, rdx
	add	r12, rdx
	call	memcpy
.utf32_unescape_amp_nocopy:
	; so now the word at r12 is an ampersand
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf32_unescape_semicolon_scan
calign
.utf32_unescape_buffer:
	; string is too big to do on the stack, so create a temporary buffer for it
	mov	rdx, r10
	push	rbx r12 r13 r14
	lea	rsi, [r8+rsi]		; beginning of string
	sub	rdx, rcx
	mov	r12, rax		; pointer to first ampersand
	mov	r13, rcx		; characters left
	push	rsi rdx
	mov	rdi, r10
	shl	rdi, 2
	call	heap$alloc
	mov	r14, rax
	pop	rdx rsi
	mov	rdi, rax
	lea	rbx, [r14+rdx]
	shl	rdx, 2
	call	memcpy
	mov	rsi, r12
	mov	rcx, r13
	xor	edx, edx
	jmp	.utf32_unescape_semicolon_scan

end if


if used xmltag$unescape_string | defined include_everything
	; single argument in rdi: string to unescape
	; returns a new heap$alloc'd string of unescaped text
	; NOTE: we cheat a bit here and use the xmltag$unescape version to do the dirty work
falign
xmltag$unescape_string:
	prolog	xmltag$unescape_string
	lea	rcx, [rdi+8]
	xor	esi, esi
	mov	rdx, [rdi]
	sub	rsp, xmltag_attributes_ofs	; we don't need a full xmltag for this
	mov	[rsp+xmltag_base_ofs], rcx
if string_bits = 32
	mov	dword [rsp+xmltag_width_ofs], xmlparser_utf32
else
	mov	dword [rsp+xmltag_width_ofs], xmlparser_utf16
end if
	mov	rdi, rsp
	call	xmltag$unescape
	add	rsp, xmltag_attributes_ofs
	epilog

end if


if used xmltag$text | defined include_everything
	; single argument in rdi: an xmltag object
	; returns a new heap$alloc'd string of the text, possibly unescaped
falign
xmltag$text:
	prolog	xmltag$text
	mov	eax, [rdi+xmltag_width_ofs]
	mov	rsi, [rdi+xmltag_base_ofs]
	mov	rdx, [rdi+xmltag_textstart_ofs]
	mov	rcx, [rdi+xmltag_textend_ofs]
	cmp	dword [rdi+xmltag_nodetype_ofs], xmltag_textnode
	jne	.noescape
	cmp	rcx, rdx
	je	.emptystring
	mov	rsi, rdx
	mov	rdx, rcx
	call	xmltag$unescape
	epilog
calign
.emptystring:
	call	string$new
	epilog
calign
.noescape:
	sub	rcx, rdx
	jz	.emptystring
	jmp	qword [rax*8+.noescape_dispatch]
dalign
.noescape_dispatch:
	dq	.noescape_utf8, .noescape_utf16, .noescape_utf32
calign
.noescape_utf8:
	lea	rdi, [rsi+rdx]
	mov	rsi, rcx
	call	string$from_utf8
	epilog
calign
.noescape_utf16:
	lea	rdi, [rsi+rdx*2]
	lea	rsi, [rcx*2]
	call	string$from_utf16
	epilog
calign
.noescape_utf32:
	lea	rdi, [rsi+rdx*4]
	lea	rsi, [rcx*4]
	call	string$from_utf32
	epilog

end if


if used xmltag$debug | defined include_everything
	; single argument in rdi: an xmltag object
falign
xmltag$debug:
	prolog	xmltag$debug
	mov	eax, [rdi+xmltag_nodetype_ofs]
	push	rbx r12 r13
	mov	rbx, [rdi+xmltag_base_ofs]
	mov	r13d, [rdi+xmltag_width_ofs]
	mov	r12, rdi
	jmp	qword [rax*8+.dispatch]
dalign
.dispatch:
	dq	.notype, .element, .textnode, .cdata, .processinginstruction, .comment, .doctype, .xmldecl
cleartext .tagtype_notype, '(no type) (no output as a result)'
calign
.notype:
	mov	rdi, .tagtype_notype
	call	string$to_stdoutln
	pop	r13 r12 rbx
	epilog
cleartext .tagtype_element, 'Element: '
cleartext .space, ' '
cleartext .empty, ' Empty? '
cleartext .true, 'true'
cleartext .false, 'false'
cleartext .attrs, ' attrcount: '
cleartext .equalquote, '="'
cleartext .quote, '"'
calign
.element:
	mov	rdi, .tagtype_element
	call	string$to_stdout
	mov	rdi, [r12+xmltag_textstart_ofs]
	mov	rsi, [r12+xmltag_textend_ofs]
	call	.textout
	mov	rdi, .empty
	call	string$to_stdout
	mov	rdi, .true
	mov	rsi, .false
	cmp	dword [r12+xmltag_empty_ofs], 0
	cmove	rdi, rsi
	call	string$to_stdout
	mov	rdi, .attrs
	call	string$to_stdout
	mov	edi, [r12+xmltag_attrcount_ofs]
	mov	esi, 10
	call	string$from_unsigned
	push	rax
	mov	rdi, rax
	call	string$to_stdout
	pop	rdi
	call	heap$free
	cmp	dword [r12+xmltag_attrcount_ofs], 0
	je	.element_noattrs
	push	r14
	xor	r14d, r14d
calign
.element_attrloop:
	mov	rdi, .space
	call	string$to_stdout
	mov	rdi, r12
	mov	esi, r14d
	call	xmltag$getattr
	push	rax
	; attribute name first:
	mov	rdi, [rax+xmltagattr_namestart_ofs]
	mov	rsi, [rax+xmltagattr_nameend_ofs]
	call	.textout
	mov	rdi, .equalquote
	call	string$to_stdout
	pop	rax
	; value
	mov	rdi, [rax+xmltagattr_valuestart_ofs]
	mov	rsi, [rax+xmltagattr_valueend_ofs]
	call	.textout
	mov	rdi, .quote
	call	string$to_stdout
	add	r14d, 1
	cmp	r14d, dword [r12+xmltag_attrcount_ofs]
	jne	.element_attrloop
	pop	r14
	; fallthrough to .element_noattrs
calign
.element_noattrs:
	mov	rdi, .lf
	call	string$to_stdout
	pop	r13 r12 rbx
	epilog

cleartext .tagtype_textnode, 'Text: '
calign
.textnode:
	mov	rdi, .tagtype_textnode
	call	string$to_stdout
	mov	rdi, [r12+xmltag_textstart_ofs]
	mov	rsi, [r12+xmltag_textend_ofs]
	call	.textout
	mov	rdi, .lf
	call	string$to_stdout
	pop	r13 r12 rbx
	epilog



cleartext .tagtype_cdata, 'CData: '
calign
.cdata:
	mov	rdi, .tagtype_cdata
	call	string$to_stdout
	mov	rdi, [r12+xmltag_textstart_ofs]
	mov	rsi, [r12+xmltag_textend_ofs]
	call	.textout
	mov	rdi, .lf
	call	string$to_stdout
	pop	r13 r12 rbx
	epilog
	

cleartext .tagtype_processinginstruction, 'ProcessingInstruction: '
calign
.processinginstruction:
	mov	rdi, .tagtype_processinginstruction
	call	string$to_stdout
	mov	rdi, [r12+xmltag_textstart_ofs]
	mov	rsi, [r12+xmltag_textend_ofs]
	call	.textout
	mov	rdi, .lf
	call	string$to_stdout
	pop	r13 r12 rbx
	epilog

cleartext .tagtype_comment, 'Comment: '
calign
.comment:
	mov	rdi, .tagtype_comment
	call	string$to_stdout
	mov	rdi, [r12+xmltag_textstart_ofs]
	mov	rsi, [r12+xmltag_textend_ofs]
	call	.textout
	mov	rdi, .lf
	call	string$to_stdout
	pop	r13 r12 rbx
	epilog

cleartext .tagtype_doctype, 'DOCTYPE: '
calign
.doctype:
	mov	rdi, .tagtype_doctype
	call	string$to_stdout
	mov	rdi, [r12+xmltag_textstart_ofs]
	mov	rsi, [r12+xmltag_textend_ofs]
	call	.textout
	mov	rdi, .lf
	call	string$to_stdout
	pop	r13 r12 rbx
	epilog

cleartext .tagtype_xmldecl, 'XMLDeclaration: '
calign
.xmldecl:
	mov	rdi, .tagtype_xmldecl
	call	string$to_stdout
	mov	rdi, [r12+xmltag_textstart_ofs]
	mov	rsi, [r12+xmltag_textend_ofs]
	call	.textout
	mov	rdi, .lf
	call	string$to_stdout
	pop	r13 r12 rbx
	epilog
cleartext .lf, 10

falign
.textout:
	jmp	qword [r13*8+.textdispatch]
dalign
.textdispatch:
	dq	.text8, .text16, .text32
calign
.text8:
	sub	rsi, rdi
	add	rdi, rbx
	call	string$from_utf8
	push	rax
	mov	rdi, rax
	call	string$to_stdout
	pop	rdi
	call	heap$free
	ret
calign
.text16:
	sub	rsi, rdi
	shl	rsi, 1
	shl	rdi, 1
	add	rdi, rbx
	call	string$from_utf16
	push	rax
	mov	rdi, rax
	call	string$to_stdout
	pop	rdi
	call	heap$free
	ret
calign
.text32:
	sub	rsi, rdi
	shl	rsi, 2
	shl	rdi, 2
	add	rdi, rbx
	call	string$from_utf32
	push	rax
	mov	rdi, rax
	call	string$to_stdout
	pop	rdi
	call	heap$free
	ret
	
end if


if used xmlparser$new | used xmlparser$new_string | used xmlparser$init | used xmlparser$init_string | defined include_everything

; defines for the character width (as passed to xmlparser$new)
xmlparser_utf8	= 0
xmlparser_utf16	= 1
xmlparser_utf32 = 2

; defines for flags (as passed to xmlparser$new)
xmlparser_ignorewhite	= 1
xmlparser_condensewhite	= 2

; defines for the return of xmlparser$next and xmlparser$prev
xmlparser_noerror				= 0
xmlparser_endofdocument				= 1
xmlparser_unterminatedcdatasection		= 2
xmlparser_unterminatedxmldeclaration		= 3
xmlparser_unterminateddoctypedeclaration	= 4
xmlparser_unterminatedcomment			= 5
xmlparser_malformedelement			= 6
xmlparser_unterminatedattributevalue		= 7
xmlparser_unterminatedelement			= 8
xmlparser_unterminatedprocessinginstruction	= 9
xmlparser_tagattributeoverflow			= 10
xmlparser_badqname				= 11
xmlparser_prefixnotbound			= 12
xmlparser_duplicateattribute			= 13

; xmlparser object itself:
xmlparser_base_ofs		= 0		; pointer to xml
xmlparser_size_ofs		= 8		; size in characters (not bytes)
xmlparser_pos_ofs		= 16		; our current position (in characters not bytes)
xmlparser_end_ofs		= 24		; our end (might be size, or less if whitespace trunc, etc)
xmlparser_flags_ofs 		= 32		; dword flags
xmlparser_width_ofs		= 36		; dword character width

xmlparser_size = 40

end if


if used xmlparser$new | defined include_everything
	; four arguments: rdi == ptr to xml, rsi == length (in characters) of same, edx == character width, ecx == flags
	; returns new xmlparser object in rax
falign
xmlparser$new:
	prolog	xmlparser$new
	push	rdi rsi rdx rcx
	mov	edi, xmlparser_size
	call	heap$alloc
	xor	r8d, r8d
	pop	rcx rdx rsi rdi
	mov	[rax+xmlparser_base_ofs], rdi
	mov	[rax+xmlparser_size_ofs], rsi
	mov	[rax+xmlparser_pos_ofs], r8
	mov	[rax+xmlparser_end_ofs], rsi
	mov	[rax+xmlparser_flags_ofs], ecx
	mov	[rax+xmlparser_width_ofs], edx
	epilog

end if


if used xmlparser$new_string | defined include_everything
	; two arguments: rdi == string (native library string), esi == flags
	; returns new xmlparser object in rax
falign
xmlparser$new_string:
	prolog	xmlparser$new_string
	push	rdi rsi
	mov	edi, xmlparser_size
	call	heap$alloc
	xor	r8d, r8d
	pop	rsi rdi
	mov	rcx, [rdi]
	lea	rdx, [rdi+8]
	mov	[rax+xmlparser_base_ofs], rdx
	mov	[rax+xmlparser_size_ofs], rcx
	mov	[rax+xmlparser_pos_ofs], r8
	mov	[rax+xmlparser_end_ofs], rcx
	mov	[rax+xmlparser_flags_ofs], esi
if string_bits = 32
	mov	dword [rax+xmlparser_width_ofs], xmlparser_utf32
else
	mov	dword [rax+xmlparser_width_ofs], xmlparser_utf16
end if
	epilog

end if


if used xmlparser$init | defined include_everything
	; five arguments: rdi == xmlparser object to init, rsi == ptr to xml, rdx == length (in chars) of same, ecx == char width, r8d == flags
falign
xmlparser$init:
	prolog	xmlparser$init
	mov	rax, rdi
	xor	r9d, r9d
	mov	[rdi+xmlparser_base_ofs], rsi
	mov	[rdi+xmlparser_size_ofs], rdx
	mov	[rdi+xmlparser_pos_ofs], r9
	mov	[rdi+xmlparser_end_ofs], rdx
	mov	[rdi+xmlparser_flags_ofs], r8d
	mov	[rdi+xmlparser_width_ofs], ecx
	epilog

end if


if used xmlparser$init_string | defined include_everything
	; three arguments: rdi == xmlparser object, rsi == string (native library string), edx == flags
falign
xmlparser$init_string:
	prolog	xmlparser$init_string
	lea	rcx, [rsi+8]
	mov	r8, [rsi]
	xor	r9d, r9d
	mov	rax, rdi
	mov	[rdi+xmlparser_base_ofs], rcx
	mov	[rdi+xmlparser_size_ofs], r8
	mov	[rdi+xmlparser_pos_ofs], r9
	mov	[rdi+xmlparser_end_ofs], r8
	mov	[rdi+xmlparser_flags_ofs], edx
if string_bits = 32
	mov	dword [rdi+xmlparser_width_ofs], xmlparser_utf32
else
	mov	dword [rdi+xmlparser_width_ofs], xmlparser_utf16
end if
	epilog

end if


if used xmlparser$errortext | defined include_everything
	; single argument in edi: one of the xmlparser_ numeric return values
	; returns a static string (not heap$alloc'd)
falign
xmlparser$errortext:
	prolog	xmlparser$errortext
	mov	rax, [rdi*8+.dispatch]
	epilog
cleartext .e0, 'No Error'
cleartext .e1, 'End of Document'
cleartext .e2, 'Unterminated CDATA Section'
cleartext .e3, 'Unterminated XML Declaration'
cleartext .e4, 'Unterminated DOCTYPE Declaration'
cleartext .e5, 'Unterminated Comment'
cleartext .e6, 'Malformed Element'
cleartext .e7, 'Unterminated Attribute Value'
cleartext .e8, 'Unterminated Element'
cleartext .e9, 'Unterminated Processing Instruction'
cleartext .e10, 'Tag Attribute Count Overflow'
cleartext .e11, 'Bad QName'
cleartext .e12, 'Prefix Not Bound'
cleartext .e13, 'Duplicate Attribute'
dalign
.dispatch:
	dq	.e0, .e1, .e2, .e3, .e4, .e5, .e6, .e7, .e8, .e9, .e10, .e11, .e12, .e13

end if


if used xmlparser$next | defined include_everything
	; two arguments: rdi == xmlparser object, rsi == xmltag object (we'll call reset on it first)
	; returns one of the above xmlparser_ values in eax
falign
xmlparser$next:
	prolog	xmlparser$next
	mov	eax, [rdi+xmlparser_width_ofs]
	mov	ecx, [rdi+xmlparser_flags_ofs]
	push	rbx rdi
	mov	rbx, rsi
	mov	rsi, [rdi+xmlparser_pos_ofs]
	mov	r10, [rdi+xmlparser_end_ofs]
	mov	rdi, [rdi+xmlparser_base_ofs]
	; copy the base and width to the tag itself:
	sub	r10, rsi
	mov	[rbx+xmltag_base_ofs], rdi
	mov	dword [rbx+xmltag_width_ofs], eax
	jmp	qword [rax*8+.dispatch]
dalign
.dispatch:
	dq	.utf8, .utf16, .utf32
falign
.utf8:
	test	r10, r10
	jz	.utf8_endofdoc
	test	ecx, xmlparser_ignorewhite
	jz	.utf8_noskipwhite
	; otherwise, skip whitespace and check for eod
calign
.utf8_skipwhite:
	movzx	ecx, byte [rdi+rsi]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf8_noskipwhite
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf8_noskipwhite
	; otherwise, we hit a 32, 9, 10, or 13
	add	rsi, 1
	sub	r10, 1
	jnz	.utf8_skipwhite
	; fallthrough to end of document
calign
.utf8_endofdoc:
	; save our position
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_endofdocument
	epilog
calign
.utf8_noskipwhite:
	; reset our xmltag (no need to call xmltag$reset, inline here is fine)
	xor	ecx, ecx
	mov	edx, -1
	movzx	eax, byte [rdi+rsi]
	mov	[rbx+xmltag_textstart_ofs], rcx
	mov	[rbx+xmltag_textend_ofs], rcx
	mov	[rbx+xmltag_textcolon_ofs], rdx	; writes 0 into empty too
	cmp	eax, '<'
	mov	[rbx+xmltag_nodetype_ofs], rcx	; writes over attrcount too
	mov	[rbx+xmltag_realstart_ofs], rsi
	jne	.utf8_textnode
	cmp	r10, 6
	jb	.utf8_notcdata
	cmp	dword [rdi+rsi], '<?xm'
	je	.utf8_maybexmldecl
.utf8_notxmldecl:
	cmp	dword [rdi+rsi], '<!DO'
	je	.utf8_maybedoctype
.utf8_notdoctype:
	cmp	dword [rdi+rsi], '<![C'
	je	.utf8_maybecdata
.utf8_notcdata:
	cmp	r10, 2
	jb	.utf8_element
	cmp	word [rdi+rsi], '<?'
	je	.utf8_pi
	cmp	r10, 4
	jb	.utf8_element
	cmp	dword [rdi+rsi], '<!--'
	je	.utf8_comment
.utf8_element:
	test	r10, r10
	jz	.utf8_malformed
	; otehrwise, increment our position and search for a > or />
	add	rsi, 1
	sub	r10, 1
	jz	.utf8_malformed
	mov	[rbx+xmltag_textstart_ofs], rsi
calign
.utf8_elementscan:
	movzx	ecx, byte [rdi+rsi]
	cmp	ecx, '>'
	je	.utf8_elementscan_done
	cmp	ecx, 32
	je	.utf8_elementscan_done
	jb	.utf8_elementscan_spacecheck
	cmp	ecx, ':'
	je	.utf8_elementscan_colon
	cmp	ecx, '/'
	je	.utf8_elementscan_forwardslash
	add	rsi, 1
	sub	r10, 1
	jnz	.utf8_elementscan
	; fallthrough to malformed
calign
.utf8_malformed:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_malformedelement
	epilog
calign
.utf8_elementscan_done:
	cmp	rsi, [rbx+xmltag_textstart_ofs]
	je	.utf8_malformed
	test	r10, r10	; sanity only
	jz	.utf8_malformed
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_element
calign
.utf8_attrscan:
	movzx	ecx, byte [rdi+rsi]
	mov	edx, ecx
	cmp	ecx, 32
	ja	.utf8_attrscan_notwhitespace
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf8_attrscan_notwhitespace
	; otherwise, it was whitespace, keep going
	add	rsi, 1
	sub	r10, 1
	jnz	.utf8_attrscan
	jmp	.utf8_malformed
calign
.utf8_attrscan_notwhitespace:
	mov	ecx, edx
	cmp	edx, '>'
	je	.utf8_attrscan_alldone
	cmp	r10, 1
	je	.utf8_malformed
	cmp	word [rdi+rsi], '/>'
	je	.utf8_attrscan_empty
	; rsi sitting on attribute name start
	mov	r9d, -1		; use this for our colon pos if any
	mov	r11, rsi	; save our attribute name start
calign
.utf8_attrnamescan:
	mov	rdx, rsi
	cmp	ecx, '='
	je	.utf8_attrnamescan_done
	cmp	ecx, '>'
	je	.utf8_attrnamescan_done
	sub	rdx, r11
	cmp	ecx, 32
	ja	.utf8_attrnamescan_notws
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jnz	.utf8_attrnamescan_done
	add	ecx, 1
calign
.utf8_attrnamescan_notws:
	; otherwise, not whitespace
	mov	r8d, r9d
	cmp	ecx, ':'
	cmove	r8d, edx
	cmp	r9d, -1
	cmove	r9d, r8d
	add	rsi, 1
	sub	r10, 1
	jz	.utf8_malformed
	cmp	r10, 1
	je	.utf8_malformed
	movzx	ecx, byte [rdi+rsi]
	jmp	.utf8_attrnamescan
calign
.utf8_attrnamescan_done:
	mov	eax, [rbx+xmltag_attrcount_ofs]
	cmp	rsi, r11
	je	.utf8_malformed
	; so our attribute name end is rsi, start is r11, colon pos if any is r9d
	cmp	eax, xmltag_maxattr
	jae	.utf8_attroverflow
	imul	eax, eax, xmltagattr_size
	lea	rdx, [rbx+rax+xmltag_attributes_ofs]
	mov	[rdx+xmltagattr_namestart_ofs], r11
	mov	[rdx+xmltagattr_nameend_ofs], rsi
	mov	[rdx+xmltagattr_namecolon_ofs], r9
calign
.utf8_attrname_wsafter:
	movzx	ecx, byte [rdi+rsi]
	cmp	ecx, 32
	ja	.utf8_attrname_wsafter_done
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf8_attrname_wsafter_done
	add	rsi, 1
	sub	r10, 1
	jnz	.utf8_attrname_wsafter
	jmp	.utf8_malformed
calign
.utf8_attrname_wsafter_done:
	cmp	byte [rdi+rsi], '='
	jne	.utf8_malformed
	add	rsi, 1
	sub	r10, 1
	jz	.utf8_malformed
calign
.utf8_attrvalue_wsbefore:
	movzx	ecx, byte [rdi+rsi]
	cmp	ecx, 32
	ja	.utf8_attrvalue_wsbefore_done
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf8_attrvalue_wsbefore_done
	add	rsi, 1
	sub	r10, 1
	jnz	.utf8_attrvalue_wsbefore
	jmp	.utf8_malformed
calign
.utf8_attrvalue_wsbefore_done:
	movzx	r9d, byte [rdi+rsi]	; save our delimiter quote
	add	rsi, 1
	sub	r10, 1
	jz	.utf8_malformed
	mov	r11, rsi		; save our attribute value start position
	cmp	r9d, '"'
	je	.utf8_attrvalue_scan
	cmp	r9d, 0x27		; single quote
	jne	.utf8_malformed
calign
.utf8_attrvalue_scan:
	movzx	ecx, byte [rdi+rsi]
	add	rsi, 1
	sub	r10, 1
	cmp	ecx, r9d
	je	.utf8_attrvalue_scandone
	test	r10, r10
	jz	.utf8_attrvalue_scan_unterminated
	cmp	ecx, '<'
	jne	.utf8_attrvalue_scan
	; otherwise, unterminated attribute value
.utf8_attrvalue_scan_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedattributevalue
	epilog
calign
.utf8_attrvalue_scandone:
	; close quote found, but we may be at the end of the document:
	lea	rax, [rsi-1]
	test	r10, r10
	jz	.utf8_malformed
	; otherwise, our tag pointer in rdx is still valid:
	mov	[rdx+xmltagattr_valuestart_ofs], r11
	mov	[rdx+xmltagattr_valueend_ofs], rax
	; add one to our attrcount
	add	dword [rbx+xmltag_attrcount_ofs], 1
	; keep going:
	jmp	.utf8_attrscan
calign
.utf8_attroverflow:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_tagattributeoverflow
	epilog
calign
.utf8_attrscan_alldone:
	; char at rdi+rsi is '>'
	add	rsi, 1
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_attrscan_empty:
	add	rsi, 2
	mov	dword [rbx+xmltag_empty_ofs], 1
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_elementscan_spacecheck:
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jnz	.utf8_elementscan_done
	; not whitespace
	add	rsi, 1
	sub	r10, 1
	jnz	.utf8_elementscan
	jmp	.utf8_malformed
calign
.utf8_elementscan_colon:
	mov	rdx, rsi
	add	rsi, 1
	sub	rdx, [rbx+xmltag_textstart_ofs]
	sub	r10, 1
	jz	.utf8_malformed
	cmp	dword [rbx+xmltag_textcolon_ofs], -1
	jne	.utf8_elementscan
	mov	dword [rbx+xmltag_textcolon_ofs], edx
	jmp	.utf8_elementscan
calign
.utf8_elementscan_forwardslash:
	cmp	r10, 2
	jb	.utf8_malformed
	cmp	byte [rdi+rsi+1], '>'
	je	.utf8_elementscan_done
	add	rsi, 1
	sub	r10, 1
	jmp	.utf8_elementscan
calign
.utf8_textnode:
	mov	[rbx+xmltag_textstart_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_textnode
	mov	rax, [rsp]		
calign
.utf8_textnode_search:
	cmp	byte [rdi+rsi], '<'
	je	.utf8_textnode_searchdone
	add	rsi, 1
	sub	r10, 1
	jnz	.utf8_textnode_search
	; otherwise, we hit the end of the document
calign
.utf8_textnode_searchdone:
	; save our position
	mov	[rax+xmlparser_pos_ofs], rsi
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	r9, [rbx+xmltag_textstart_ofs]
	mov	r11, rsi
	cmp	dword [rax+xmlparser_flags_ofs], xmlparser_ignorewhite + xmlparser_condensewhite
	je	.utf8_textnode_condense_head
	; otherwise, done and dusted
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_textnode_condense_head:
	movzx	ecx, byte [rdi+r9]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf8_textnode_condense_tail
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf8_textnode_condense_tail
	; otherwise, whitespace
	add	r9, 1
	cmp	r9, rsi
	jb	.utf8_textnode_condense_head
	; otherwise, r9 == the end, all whitespace text tag?
	mov	[rbx+xmltag_textstart_ofs], rsi
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_textnode_condense_tail:
	lea	rax, [r11-1]
	cmp	r11, r9
	je	.utf8_textnode_condense_done
	movzx	ecx, byte [rdi+rax]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf8_textnode_condense_done
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf8_textnode_condense_done
	; otherwise whitespace
	mov	r11, rax
	jmp	.utf8_textnode_condense_tail
calign
.utf8_textnode_condense_done:
	mov	[rbx+xmltag_textstart_ofs], r9
	mov	[rbx+xmltag_textend_ofs], r11
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_maybexmldecl:
	cmp	word [rdi+rsi+4], 'l '
	jne	.utf8_notxmldecl
	add	rsi, 6
	sub	r10, 6
	jz	.utf8_xmldecl_unterminated
	mov	[rbx+xmltag_textstart_ofs], rsi
	; scan for our ?>
calign
.utf8_xmldecl_scan:
	cmp	r10, 2
	jb	.utf8_xmldecl_unterminated
	cmp	word [rdi+rsi], '?>'
	je	.utf8_xmldecl_scandone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 2
	jae	.utf8_xmldecl_scan
	; fallthrough to unterminated
calign
.utf8_xmldecl_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedxmldeclaration
	epilog
calign
.utf8_xmldecl_scandone:
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_xmldeclaration
	add	rsi, 2
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_maybedoctype:
	cmp	r10, 9
	jb	.utf8_notcdata			; 9 bytes required for cdata also
	cmp	dword [rdi+rsi+4], 'CTYP'
	jne	.utf8_notdoctype
	cmp	byte [rdi+rsi+8], 'E'
	jne	.utf8_notdoctype
	add	rsi, 9
	sub	r10, 9
	jz	.utf8_endofdoc
	mov	[rbx+xmltag_textstart_ofs], rsi
	xor	edx, edx			; nesting depth
calign
.utf8_doctype_scan:
	test	r10, r10
	jz	.utf8_doctype_unterminated
	movzx	ecx, byte [rdi+rsi]
	lea	eax, [edx+1]
	add	rsi, 1
	sub	r10, 1
	cmp	ecx, '<'
	cmove	edx, eax
	je	.utf8_doctype_scan
	cmp	ecx, '>'
	jne	.utf8_doctype_scan
	test	edx, edx
	jz	.utf8_doctype_done
	sub	edx, 1
	jmp	.utf8_doctype_scan
calign
.utf8_doctype_done:
	lea	rdx, [rsi-1]
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_doctypedeclaration
	mov	[rbx+xmltag_textend_ofs], rdx
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_doctype_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminateddoctypedeclaration
	epilog
calign
.utf8_maybecdata:
	cmp	r10, 9
	jb	.utf8_notcdata
	cmp	dword [rdi+rsi+4], 'DATA'
	jne	.utf8_notcdata
	cmp	byte [rdi+rsi+8], '['
	jne	.utf8_notcdata
	add	rsi, 9
	sub	r10, 9
	jz	.utf8_cdata_unterminated
	mov	[rbx+xmltag_textstart_ofs], rsi
calign
.utf8_cdata_scan:
	cmp	r10, 3
	jb	.utf8_cdata_unterminated
	cmp	word [rdi+rsi], ']]'
	je	.utf8_cdata_maybedone
	add	rsi, 1
	sub	r10, 1
	jmp	.utf8_cdata_scan
calign
.utf8_cdata_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedcdatasection
	epilog
calign
.utf8_cdata_maybedone:
	lea	rcx, [r10-1]
	lea	rdx, [rsi+1]
	cmp	byte [rdi+rsi+2], '>'
	cmovne	rsi, rdx
	cmovne	r10, rcx
	jne	.utf8_cdata_scan
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 3
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_cdata
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_pi:
	add	rsi, 2
	sub	r10, 2
	mov	[rbx+xmltag_textstart_ofs], rsi
	cmp	r10, 2
	jb	.utf8_pi_unterminated
calign
.utf8_pi_scan:
	cmp	word [rdi+rsi], '?>'
	je	.utf8_pi_scandone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 2
	jae	.utf8_pi_scan
	; fallthrough to unterminated
calign
.utf8_pi_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedprocessinginstruction
	epilog
calign
.utf8_pi_scandone:
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 2
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_processinginstruction
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf8_comment:
	add	rsi, 4
	sub	r10, 4
	mov	[rbx+xmltag_textstart_ofs], rsi
	cmp	r10, 3
	jb	.utf8_comment_unterminated
calign
.utf8_comment_scan:
	cmp	word [rdi+rsi], '--'
	je	.utf8_comment_maybedone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 3
	jae	.utf8_comment_scan
	; fallthrough to unterminated
calign
.utf8_comment_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedcomment
	epilog
calign
.utf8_comment_maybedone:
	lea	rax, [rsi+1]
	cmp	byte [rdi+rsi+2], '>'
	cmovne	rsi, rax
	jne	.utf8_comment_scan
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 3
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_comment
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog


falign
.utf16:
	test	r10, r10
	jz	.utf16_endofdoc
	test	ecx, xmlparser_ignorewhite
	jz	.utf16_noskipwhite
	; otherwise, skip whitespace and check for eod
calign
.utf16_skipwhite:
	movzx	ecx, word [rdi+rsi*2]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf16_noskipwhite
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf16_noskipwhite
	; otherwise, we hit a 32, 9, 10, or 13
	add	rsi, 1
	sub	r10, 1
	jnz	.utf16_skipwhite
	; fallthrough to end of document
calign
.utf16_endofdoc:
	; save our position
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_endofdocument
	epilog
dalign
.qxmldecl:
	dw	'<', '?', 'x', 'm'
.qdoctype:
	dw	'<', '!', 'D', 'O'
.qcdata:
	dw	'<', '!', '[', 'C'
.qcomment:
	dw	'<', '!', '-', '-'
calign
.utf16_noskipwhite:
	; reset our xmltag (no need to call xmltag$reset, inline here is fine)
	xor	ecx, ecx
	mov	edx, -1
	movzx	eax, word [rdi+rsi*2]
	mov	[rbx+xmltag_textstart_ofs], rcx
	mov	[rbx+xmltag_textend_ofs], rcx
	mov	[rbx+xmltag_textcolon_ofs], rdx	; writes 0 into empty too
	cmp	eax, '<'
	mov	[rbx+xmltag_nodetype_ofs], rcx	; writes over attrcount too
	mov	[rbx+xmltag_realstart_ofs], rsi
	jne	.utf16_textnode
	cmp	r10, 6
	jb	.utf16_notcdata
	mov	r8, [rdi+rsi*2]
	cmp	r8, [.qxmldecl]
	je	.utf16_maybexmldecl
.utf16_notxmldecl:
	cmp	r8, [.qdoctype]
	je	.utf16_maybedoctype
.utf16_notdoctype:
	cmp	r8, [.qcdata]
	je	.utf16_maybecdata
.utf16_notcdata:
	cmp	r10, 2
	jb	.utf16_element
	cmp	r8d, 0x3f003c		; '<?'
	je	.utf16_pi
	cmp	r10, 4
	jb	.utf16_element
	cmp	r8, [.qcomment]
	je	.utf16_comment
.utf16_element:
	test	r10, r10
	jz	.utf16_malformed
	; otehrwise, increment our position and search for a > or />
	add	rsi, 1
	sub	r10, 1
	jz	.utf16_malformed
	mov	[rbx+xmltag_textstart_ofs], rsi
calign
.utf16_elementscan:
	movzx	ecx, word [rdi+rsi*2]
	cmp	ecx, '>'
	je	.utf16_elementscan_done
	cmp	ecx, 32
	je	.utf16_elementscan_done
	jb	.utf16_elementscan_spacecheck
	cmp	ecx, ':'
	je	.utf16_elementscan_colon
	cmp	ecx, '/'
	je	.utf16_elementscan_forwardslash
	add	rsi, 1
	sub	r10, 1
	jnz	.utf16_elementscan
	; fallthrough to malformed
calign
.utf16_malformed:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_malformedelement
	epilog
calign
.utf16_elementscan_done:
	cmp	rsi, [rbx+xmltag_textstart_ofs]
	je	.utf16_malformed
	test	r10, r10	; sanity only
	jz	.utf16_malformed
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_element
calign
.utf16_attrscan:
	movzx	ecx, word [rdi+rsi*2]
	mov	edx, ecx
	cmp	ecx, 32
	ja	.utf16_attrscan_notwhitespace
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf16_attrscan_notwhitespace
	; otherwise, it was whitespace, keep going
	add	rsi, 1
	sub	r10, 1
	jnz	.utf16_attrscan
	jmp	.utf16_malformed
calign
.utf16_attrscan_notwhitespace:
	mov	ecx, edx
	cmp	edx, '>'
	je	.utf16_attrscan_alldone
	cmp	r10, 1
	je	.utf16_malformed
	cmp	dword [rdi+rsi*2], 0x3e002f	; '/>'
	je	.utf16_attrscan_empty
	; rsi sitting on attribute name start
	mov	r9d, -1		; use this for our colon pos if any
	mov	r11, rsi	; save our attribute name start
calign
.utf16_attrnamescan:
	mov	rdx, rsi
	cmp	ecx, '='
	je	.utf16_attrnamescan_done
	cmp	ecx, '>'
	je	.utf16_attrnamescan_done
	sub	rdx, r11
	cmp	ecx, 32
	ja	.utf16_attrnamescan_notws
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jnz	.utf16_attrnamescan_done
	add	ecx, 1
calign
.utf16_attrnamescan_notws:
	; otherwise, not whitespace
	mov	r8d, r9d
	cmp	ecx, ':'
	cmove	r8d, edx
	cmp	r9d, -1
	cmove	r9d, r8d
	add	rsi, 1
	sub	r10, 1
	jz	.utf16_malformed
	cmp	r10, 1
	je	.utf16_malformed
	movzx	ecx, word [rdi+rsi*2]
	jmp	.utf16_attrnamescan
calign
.utf16_attrnamescan_done:
	mov	eax, [rbx+xmltag_attrcount_ofs]
	cmp	rsi, r11
	je	.utf16_malformed
	; so our attribute name end is rsi, start is r11, colon pos if any is r9d
	cmp	eax, xmltag_maxattr
	jae	.utf16_attroverflow
	imul	eax, eax, xmltagattr_size
	lea	rdx, [rbx+rax+xmltag_attributes_ofs]
	mov	[rdx+xmltagattr_namestart_ofs], r11
	mov	[rdx+xmltagattr_nameend_ofs], rsi
	mov	[rdx+xmltagattr_namecolon_ofs], r9
calign
.utf16_attrname_wsafter:
	movzx	ecx, word [rdi+rsi*2]
	cmp	ecx, 32
	ja	.utf16_attrname_wsafter_done
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf16_attrname_wsafter_done
	add	rsi, 1
	sub	r10, 1
	jnz	.utf16_attrname_wsafter
	jmp	.utf16_malformed
calign
.utf16_attrname_wsafter_done:
	cmp	word [rdi+rsi*2], '='
	jne	.utf16_malformed
	add	rsi, 1
	sub	r10, 1
	jz	.utf16_malformed
calign
.utf16_attrvalue_wsbefore:
	movzx	ecx, word [rdi+rsi*2]
	cmp	ecx, 32
	ja	.utf16_attrvalue_wsbefore_done
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf16_attrvalue_wsbefore_done
	add	rsi, 1
	sub	r10, 1
	jnz	.utf16_attrvalue_wsbefore
	jmp	.utf16_malformed
calign
.utf16_attrvalue_wsbefore_done:
	movzx	r9d, word [rdi+rsi*2]	; save our delimiter quote
	add	rsi, 1
	sub	r10, 1
	jz	.utf16_malformed
	mov	r11, rsi		; save our attribute value start position
	cmp	r9d, '"'
	je	.utf16_attrvalue_scan
	cmp	r9d, 0x27		; single quote
	jne	.utf16_malformed
calign
.utf16_attrvalue_scan:
	movzx	ecx, word [rdi+rsi*2]
	add	rsi, 1
	sub	r10, 1
	cmp	ecx, r9d
	je	.utf16_attrvalue_scandone
	test	r10, r10
	jz	.utf16_attrvalue_scan_unterminated
	cmp	ecx, '<'
	jne	.utf16_attrvalue_scan
.utf16_attrvalue_scan_unterminated:
	; otherwise, unterminated attribute value
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedattributevalue
	epilog
calign
.utf16_attrvalue_scandone:
	; close quote found, but we may be at the end of the document:
	lea	rax, [rsi-1]
	test	r10, r10
	jz	.utf16_malformed
	; otherwise, our tag pointer in rdx is still valid:
	mov	[rdx+xmltagattr_valuestart_ofs], r11
	mov	[rdx+xmltagattr_valueend_ofs], rax
	; add one to our attrcount
	add	dword [rbx+xmltag_attrcount_ofs], 1
	; keep going:
	jmp	.utf16_attrscan
calign
.utf16_attroverflow:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_tagattributeoverflow
	epilog
calign
.utf16_attrscan_alldone:
	; char at rdi+rsi is '>'
	add	rsi, 1
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf16_attrscan_empty:
	add	rsi, 2
	mov	dword [rbx+xmltag_empty_ofs], 1
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf16_elementscan_spacecheck:
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jnz	.utf16_elementscan_done
	; not whitespace
	add	rsi, 1
	sub	r10, 1
	jnz	.utf16_elementscan
	jmp	.utf16_malformed
calign
.utf16_elementscan_colon:
	mov	rdx, rsi
	add	rsi, 1
	sub	rdx, [rbx+xmltag_textstart_ofs]
	sub	r10, 1
	jz	.utf16_malformed
	cmp	dword [rbx+xmltag_textcolon_ofs], -1
	jne	.utf16_elementscan
	mov	dword [rbx+xmltag_textcolon_ofs], edx
	jmp	.utf16_elementscan
calign
.utf16_elementscan_forwardslash:
	cmp	r10, 2
	jb	.utf16_malformed
	cmp	word [rdi+rsi*2+2], '>'
	je	.utf16_elementscan_done
	add	rsi, 1
	sub	r10, 1
	jmp	.utf16_elementscan
calign
.utf16_textnode:
	mov	[rbx+xmltag_textstart_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_textnode
	mov	rax, [rsp]		
calign
.utf16_textnode_search:
	cmp	word [rdi+rsi*2], '<'
	je	.utf16_textnode_searchdone
	add	rsi, 1
	sub	r10, 1
	jnz	.utf16_textnode_search
	; otherwise, we hit the end of the document
calign
.utf16_textnode_searchdone:
	; save our position
	mov	[rax+xmlparser_pos_ofs], rsi
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	r9, [rbx+xmltag_textstart_ofs]
	mov	r11, rsi
	cmp	dword [rax+xmlparser_flags_ofs], xmlparser_ignorewhite + xmlparser_condensewhite
	je	.utf16_textnode_condense_head
	; otherwise, done and dusted
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
calign
.utf16_textnode_condense_head:
	movzx	ecx, word [rdi+r9*2]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf16_textnode_condense_tail
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf16_textnode_condense_tail
	; otherwise, whitespace
	add	r9, 1
	cmp	r9, rsi
	jb	.utf16_textnode_condense_head
	; otherwise, r9 == the end, all whitespace text tag?
	mov	[rbx+xmltag_textstart_ofs], rsi
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
calign
.utf16_textnode_condense_tail:
	lea	rax, [r11-1]
	cmp	r11, r9
	je	.utf16_textnode_condense_done
	movzx	ecx, word [rdi+rax*2]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf16_textnode_condense_done
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf16_textnode_condense_done
	; otherwise whitespace
	mov	r11, rax
	jmp	.utf16_textnode_condense_tail
calign
.utf16_textnode_condense_done:
	mov	[rbx+xmltag_textstart_ofs], r9
	mov	[rbx+xmltag_textend_ofs], r11
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
calign
.utf16_maybexmldecl:
	cmp	dword [rdi+rsi*2+8], 0x20006c	; 'l '
	jne	.utf16_notxmldecl
	add	rsi, 6
	sub	r10, 6
	jz	.utf16_xmldecl_unterminated
	mov	[rbx+xmltag_textstart_ofs], rsi
	; scan for our ?>
calign
.utf16_xmldecl_scan:
	cmp	r10, 2
	jb	.utf16_xmldecl_unterminated
	cmp	dword [rdi+rsi*2], 0x3e003f	; '?>'
	je	.utf16_xmldecl_scandone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 2
	jae	.utf16_xmldecl_scan
	; fallthrough to unterminated
calign
.utf16_xmldecl_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedxmldeclaration
	epilog
calign
.utf16_xmldecl_scandone:
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_xmldeclaration
	add	rsi, 2
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
dalign
.qctype:
	dw	'C', 'T', 'Y', 'P'
calign
.utf16_maybedoctype:
	mov	rdx, [rdi+rsi*2+8]
	cmp	r10, 9
	jb	.utf16_notcdata			; 9 bytes required for cdata also
	cmp	rdx, [.qctype]			; CTYP
	jne	.utf16_notdoctype
	cmp	word [rdi+rsi*2+16], 'E'
	jne	.utf16_notdoctype
	add	rsi, 9
	sub	r10, 9
	jz	.utf16_endofdoc
	mov	[rbx+xmltag_textstart_ofs], rsi
	xor	edx, edx			; nesting depth
calign
.utf16_doctype_scan:
	test	r10, r10
	jz	.utf16_doctype_unterminated
	movzx	ecx, word [rdi+rsi*2]
	lea	eax, [edx+1]
	add	rsi, 1
	sub	r10, 1
	cmp	ecx, '<'
	cmove	edx, eax
	je	.utf16_doctype_scan
	cmp	ecx, '>'
	jne	.utf16_doctype_scan
	test	edx, edx
	jz	.utf16_doctype_done
	sub	edx, 1
	jmp	.utf16_doctype_scan
calign
.utf16_doctype_done:
	lea	rdx, [rsi-1]
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_doctypedeclaration
	mov	[rbx+xmltag_textend_ofs], rdx
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf16_doctype_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminateddoctypedeclaration
	epilog
dalign
.qdata:
	dw	'D', 'A', 'T', 'A'
calign
.utf16_maybecdata:
	mov	rdx, [rdi+rsi*2+8]
	cmp	r10, 9
	jb	.utf16_notcdata
	cmp	rdx, [.qdata]
	jne	.utf16_notcdata
	cmp	word [rdi+rsi*2+16], '['
	jne	.utf16_notcdata
	add	rsi, 9
	sub	r10, 9
	jz	.utf16_cdata_unterminated
	mov	[rbx+xmltag_textstart_ofs], rsi
calign
.utf16_cdata_scan:
	cmp	r10, 3
	jb	.utf16_cdata_unterminated
	cmp	dword [rdi+rsi*2], 0x5d005d	; ']]'
	je	.utf16_cdata_maybedone
	add	rsi, 1
	sub	r10, 1
	jmp	.utf16_cdata_scan
calign
.utf16_cdata_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedcdatasection
	epilog
calign
.utf16_cdata_maybedone:
	lea	rcx, [r10-1]
	lea	rdx, [rsi+1]
	cmp	word [rdi+rsi*2+4], '>'
	cmovne	rsi, rdx
	cmovne	r10, rcx
	jne	.utf16_cdata_scan
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 3
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_cdata
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf16_pi:
	add	rsi, 2
	sub	r10, 2
	mov	[rbx+xmltag_textstart_ofs], rsi
	cmp	r10, 2
	jb	.utf16_pi_unterminated
calign
.utf16_pi_scan:
	cmp	dword [rdi+rsi*2], 0x3e003f	; '?>'
	je	.utf16_pi_scandone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 2
	jae	.utf16_pi_scan
	; fallthrough to unterminated
calign
.utf16_pi_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedprocessinginstruction
	epilog
calign
.utf16_pi_scandone:
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 2
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_processinginstruction
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf16_comment:
	add	rsi, 4
	sub	r10, 4
	mov	[rbx+xmltag_textstart_ofs], rsi
	cmp	r10, 3
	jb	.utf16_comment_unterminated
calign
.utf16_comment_scan:
	cmp	dword [rdi+rsi*2], 0x2d002d	; '--'
	je	.utf16_comment_maybedone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 3
	jae	.utf16_comment_scan
	; fallthrough to unterminated
calign
.utf16_comment_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedcomment
	epilog
calign
.utf16_comment_maybedone:
	lea	rax, [rsi+1]
	cmp	word [rdi+rsi*2+4], '>'
	cmovne	rsi, rax
	jne	.utf16_comment_scan
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 3
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_comment
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog

falign
.utf32:
	test	r10, r10
	jz	.utf32_endofdoc
	test	ecx, xmlparser_ignorewhite
	jz	.utf32_noskipwhite
	; otherwise, skip whitespace and check for eod
calign
.utf32_skipwhite:
	mov	ecx, [rdi+rsi*4]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf32_noskipwhite
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf32_noskipwhite
	; otherwise, we hit a 32, 9, 10, or 13
	add	rsi, 1
	sub	r10, 1
	jnz	.utf32_skipwhite
	; fallthrough to end of document
calign
.utf32_endofdoc:
	; save our position
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_endofdocument
	epilog
dalign
.dqxmldecl:
	dd	'<', '?', 'x', 'm'
.dqdoctype:
	dd	'<', '!', 'D', 'O'
.dqcdata:
	dd	'<', '!', '[', 'C'
.dqcomment:
	dd	'<', '!', '-', '-'
.qltquestion:
	dd	'<', '?'
calign
.utf32_noskipwhite:
	; reset our xmltag (no need to call xmltag$reset, inline here is fine)
	xor	ecx, ecx
	mov	edx, -1
	mov	eax, [rdi+rsi*4]
	mov	[rbx+xmltag_textstart_ofs], rcx
	mov	[rbx+xmltag_textend_ofs], rcx
	mov	[rbx+xmltag_textcolon_ofs], rdx	; writes 0 into empty too
	cmp	eax, '<'
	mov	[rbx+xmltag_nodetype_ofs], rcx	; writes over attrcount too
	mov	[rbx+xmltag_realstart_ofs], rsi
	jne	.utf32_textnode
	cmp	r10, 6
	jb	.utf32_notcdata
	mov	r8, [rdi+rsi*4]
	mov	r9, [rdi+rsi*4+8]
	cmp	r8, [.dqxmldecl]
	je	.utf32_maybexmldecl
.utf32_notxmldecl:
	cmp	r8, [.dqdoctype]
	je	.utf32_maybedoctype
.utf32_notdoctype:
	cmp	r8, [.dqcdata]
	je	.utf32_maybecdata
.utf32_notcdata:
	cmp	r10, 2
	jb	.utf32_element
	cmp	r8, [.qltquestion]	; '<?'
	je	.utf32_pi
	cmp	r10, 4
	jb	.utf32_element
	cmp	r8, [.dqcomment]
	je	.utf32_maybecomment
.utf32_element:
	test	r10, r10
	jz	.utf32_malformed
	; otehrwise, increment our position and search for a > or />
	add	rsi, 1
	sub	r10, 1
	jz	.utf32_malformed
	mov	[rbx+xmltag_textstart_ofs], rsi
calign
.utf32_elementscan:
	mov	ecx, dword [rdi+rsi*4]
	cmp	ecx, '>'
	je	.utf32_elementscan_done
	cmp	ecx, 32
	je	.utf32_elementscan_done
	jb	.utf32_elementscan_spacecheck
	cmp	ecx, ':'
	je	.utf32_elementscan_colon
	cmp	ecx, '/'
	je	.utf32_elementscan_forwardslash
	add	rsi, 1
	sub	r10, 1
	jnz	.utf32_elementscan
	; fallthrough to malformed
calign
.utf32_malformed:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_malformedelement
	epilog
calign
.utf32_elementscan_done:
	cmp	rsi, [rbx+xmltag_textstart_ofs]
	je	.utf32_malformed
	test	r10, r10	; sanity only
	jz	.utf32_malformed
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_element
calign
.utf32_attrscan:
	mov	ecx, dword [rdi+rsi*4]
	mov	edx, ecx
	cmp	ecx, 32
	ja	.utf32_attrscan_notwhitespace
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf32_attrscan_notwhitespace
	; otherwise, it was whitespace, keep going
	add	rsi, 1
	sub	r10, 1
	jnz	.utf32_attrscan
	jmp	.utf32_malformed
dalign
.qslashgt:
	dd	'/', '>'
calign
.utf32_attrscan_notwhitespace:
	mov	ecx, edx
	mov	r8, [.qslashgt]
	cmp	edx, '>'
	je	.utf32_attrscan_alldone
	cmp	r10, 1
	je	.utf32_malformed
	cmp	qword [rdi+rsi*4], r8		; '/>'
	je	.utf32_attrscan_empty
	; rsi sitting on attribute name start
	mov	r9d, -1		; use this for our colon pos if any
	mov	r11, rsi	; save our attribute name start
calign
.utf32_attrnamescan:
	mov	rdx, rsi
	cmp	ecx, '='
	je	.utf32_attrnamescan_done
	cmp	ecx, '>'
	je	.utf32_attrnamescan_done
	sub	rdx, r11
	cmp	ecx, 32
	ja	.utf32_attrnamescan_notws
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jnz	.utf32_attrnamescan_done
	add	ecx, 1
calign
.utf32_attrnamescan_notws:
	; otherwise, not whitespace
	mov	r8d, r9d
	cmp	ecx, ':'
	cmove	r8d, edx
	cmp	r9d, -1
	cmove	r9d, r8d
	add	rsi, 1
	sub	r10, 1
	jz	.utf32_malformed
	cmp	r10, 1
	je	.utf32_malformed
	mov	ecx, dword [rdi+rsi*4]
	jmp	.utf32_attrnamescan
calign
.utf32_attrnamescan_done:
	mov	eax, [rbx+xmltag_attrcount_ofs]
	cmp	rsi, r11
	je	.utf32_malformed
	; so our attribute name end is rsi, start is r11, colon pos if any is r9d
	cmp	eax, xmltag_maxattr
	jae	.utf32_attroverflow
	imul	eax, eax, xmltagattr_size
	lea	rdx, [rbx+rax+xmltag_attributes_ofs]
	mov	[rdx+xmltagattr_namestart_ofs], r11
	mov	[rdx+xmltagattr_nameend_ofs], rsi
	mov	[rdx+xmltagattr_namecolon_ofs], r9
calign
.utf32_attrname_wsafter:
	mov	ecx, dword [rdi+rsi*4]
	cmp	ecx, 32
	ja	.utf32_attrname_wsafter_done
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf32_attrname_wsafter_done
	add	rsi, 1
	sub	r10, 1
	jnz	.utf32_attrname_wsafter
	jmp	.utf32_malformed
calign
.utf32_attrname_wsafter_done:
	cmp	dword [rdi+rsi*4], '='
	jne	.utf32_malformed
	add	rsi, 1
	sub	r10, 1
	jz	.utf32_malformed
calign
.utf32_attrvalue_wsbefore:
	mov	ecx, dword [rdi+rsi*4]
	cmp	ecx, 32
	ja	.utf32_attrvalue_wsbefore_done
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf32_attrvalue_wsbefore_done
	add	rsi, 1
	sub	r10, 1
	jnz	.utf32_attrvalue_wsbefore
	jmp	.utf32_malformed
calign
.utf32_attrvalue_wsbefore_done:
	mov	r9d, dword [rdi+rsi*4]	; save our delimiter quote
	add	rsi, 1
	sub	r10, 1
	jz	.utf32_malformed
	mov	r11, rsi		; save our attribute value start position
	cmp	r9d, '"'
	je	.utf32_attrvalue_scan
	cmp	r9d, 0x27		; single quote
	jne	.utf32_malformed
calign
.utf32_attrvalue_scan:
	mov	ecx, dword [rdi+rsi*4]
	add	rsi, 1
	sub	r10, 1
	cmp	ecx, r9d
	je	.utf32_attrvalue_scandone
	test	r10, r10
	jz	.utf32_attrvalue_scan_unterminated
	cmp	ecx, '<'
	jne	.utf32_attrvalue_scan
.utf32_attrvalue_scan_unterminated:
	; otherwise, unterminated attribute value
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedattributevalue
	epilog
calign
.utf32_attrvalue_scandone:
	; close quote found, but we may be at the end of the document:
	lea	rax, [rsi-1]
	test	r10, r10
	jz	.utf32_malformed
	; otherwise, our tag pointer in rdx is still valid:
	mov	[rdx+xmltagattr_valuestart_ofs], r11
	mov	[rdx+xmltagattr_valueend_ofs], rax
	; add one to our attrcount
	add	dword [rbx+xmltag_attrcount_ofs], 1
	; keep going:
	jmp	.utf32_attrscan
calign
.utf32_attroverflow:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_tagattributeoverflow
	epilog
calign
.utf32_attrscan_alldone:
	; char at rdi+rsi is '>'
	add	rsi, 1
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf32_attrscan_empty:
	add	rsi, 2
	mov	dword [rbx+xmltag_empty_ofs], 1
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf32_elementscan_spacecheck:
	mov	r8d, 1
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jnz	.utf32_elementscan_done
	; not whitespace
	add	rsi, 1
	sub	r10, 1
	jnz	.utf32_elementscan
	jmp	.utf32_malformed
calign
.utf32_elementscan_colon:
	mov	rdx, rsi
	add	rsi, 1
	sub	rdx, [rbx+xmltag_textstart_ofs]
	sub	r10, 1
	jz	.utf32_malformed
	cmp	dword [rbx+xmltag_textcolon_ofs], -1
	jne	.utf32_elementscan
	mov	dword [rbx+xmltag_textcolon_ofs], edx
	jmp	.utf32_elementscan
calign
.utf32_elementscan_forwardslash:
	cmp	r10, 2
	jb	.utf32_malformed
	cmp	dword [rdi+rsi*4+4], '>'
	je	.utf32_elementscan_done
	add	rsi, 1
	sub	r10, 1
	jmp	.utf32_elementscan
calign
.utf32_textnode:
	mov	[rbx+xmltag_textstart_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_textnode
	mov	rax, [rsp]		
calign
.utf32_textnode_search:
	cmp	dword [rdi+rsi*4], '<'
	je	.utf32_textnode_searchdone
	add	rsi, 1
	sub	r10, 1
	jnz	.utf32_textnode_search
	; otherwise, we hit the end of the document
calign
.utf32_textnode_searchdone:
	; save our position
	mov	[rax+xmlparser_pos_ofs], rsi
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	r9, [rbx+xmltag_textstart_ofs]
	mov	r11, rsi
	cmp	dword [rax+xmlparser_flags_ofs], xmlparser_ignorewhite + xmlparser_condensewhite
	je	.utf32_textnode_condense_head
	; otherwise, done and dusted
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
calign
.utf32_textnode_condense_head:
	mov	ecx, dword [rdi+r9*4]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf32_textnode_condense_tail
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf32_textnode_condense_tail
	; otherwise, whitespace
	add	r9, 1
	cmp	r9, rsi
	jb	.utf32_textnode_condense_head
	; otherwise, r9 == the end, all whitespace text tag?
	mov	[rbx+xmltag_textstart_ofs], rsi
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
calign
.utf32_textnode_condense_tail:
	lea	rax, [r11-1]
	cmp	r11, r9
	je	.utf32_textnode_condense_done
	mov	ecx, dword [rdi+rax*4]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf32_textnode_condense_done
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf32_textnode_condense_done
	; otherwise whitespace
	mov	r11, rax
	jmp	.utf32_textnode_condense_tail
calign
.utf32_textnode_condense_done:
	mov	[rbx+xmltag_textstart_ofs], r9
	mov	[rbx+xmltag_textend_ofs], r11
	pop	rdi rbx
	mov	eax, xmlparser_noerror
	epilog
dalign
.qlspace:
	dd	'l', ' '
.qquestiongt:
	dd	'?', '>'
calign
.utf32_maybexmldecl:
	mov	rdx, [rdi+rsi*4+16]
	cmp	r9, [.dqxmldecl+8]
	jne	.utf32_notxmldecl
	cmp	rdx, [.qlspace]
	jne	.utf32_notxmldecl
	mov	r9, [.qquestiongt]
	add	rsi, 6
	sub	r10, 6
	jz	.utf32_xmldecl_unterminated
	mov	[rbx+xmltag_textstart_ofs], rsi
	; scan for our ?>
calign
.utf32_xmldecl_scan:
	cmp	r10, 2
	jb	.utf32_xmldecl_unterminated
	cmp	qword [rdi+rsi*4], r9		; '?>'
	je	.utf32_xmldecl_scandone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 2
	jae	.utf32_xmldecl_scan
	; fallthrough to unterminated
calign
.utf32_xmldecl_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedxmldeclaration
	epilog
calign
.utf32_xmldecl_scandone:
	mov	[rbx+xmltag_textend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_xmldeclaration
	add	rsi, 2
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
dalign
.dqctype:
	dd	'C', 'T', 'Y', 'P'
calign
.utf32_maybedoctype:
	cmp	r9, [.dqdoctype+8]
	jne	.utf32_notdoctype
	mov	rdx, [rdi+rsi*4+16]
	mov	rax, [rdi+rsi*4+24]
	cmp	r10, 9
	jb	.utf32_notcdata			; 9 bytes required for cdata also
	cmp	rdx, [.dqctype]			; CT
	jne	.utf32_notdoctype
	cmp	rax, [.dqctype+8]
	jne	.utf32_notdoctype
	cmp	dword [rdi+rsi*4+32], 'E'
	jne	.utf32_notdoctype
	add	rsi, 9
	sub	r10, 9
	jz	.utf32_endofdoc
	mov	[rbx+xmltag_textstart_ofs], rsi
	xor	edx, edx			; nesting depth
calign
.utf32_doctype_scan:
	test	r10, r10
	jz	.utf32_doctype_unterminated
	mov	ecx, dword [rdi+rsi*4]
	lea	eax, [edx+1]
	add	rsi, 1
	sub	r10, 1
	cmp	ecx, '<'
	cmove	edx, eax
	je	.utf32_doctype_scan
	cmp	ecx, '>'
	jne	.utf32_doctype_scan
	test	edx, edx
	jz	.utf32_doctype_done
	sub	edx, 1
	jmp	.utf32_doctype_scan
calign
.utf32_doctype_done:
	lea	rdx, [rsi-1]
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_doctypedeclaration
	mov	[rbx+xmltag_textend_ofs], rdx
	mov	[rbx+xmltag_realend_ofs], rsi
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf32_doctype_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminateddoctypedeclaration
	epilog
dalign
.dqdata:
	dd	'D', 'A', 'T', 'A'
.qbrbr:
	dd	']', ']'
calign
.utf32_maybecdata:
	cmp	r9, [.dqcdata+8]
	jne	.utf32_notcdata
	mov	rdx, [rdi+rsi*4+16]
	mov	rax, [rdi+rsi*4+24]
	cmp	r10, 9
	jb	.utf32_notcdata
	cmp	rdx, [.dqdata]
	jne	.utf32_notcdata
	cmp	rax, [.dqdata+8]
	jne	.utf32_notcdata
	cmp	dword [rdi+rsi*4+32], '['
	jne	.utf32_notcdata
	add	rsi, 9
	sub	r10, 9
	jz	.utf32_cdata_unterminated
	mov	[rbx+xmltag_textstart_ofs], rsi
	mov	r8, [.qbrbr]
calign
.utf32_cdata_scan:
	cmp	r10, 3
	jb	.utf32_cdata_unterminated
	cmp	qword [rdi+rsi*4], r8		; ']]'
	je	.utf32_cdata_maybedone
	add	rsi, 1
	sub	r10, 1
	jmp	.utf32_cdata_scan
calign
.utf32_cdata_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedcdatasection
	epilog
calign
.utf32_cdata_maybedone:
	lea	rcx, [r10-1]
	lea	rdx, [rsi+1]
	cmp	dword [rdi+rsi*4+8], '>'
	cmovne	rsi, rdx
	cmovne	r10, rcx
	jne	.utf32_cdata_scan
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 3
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_cdata
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
calign
.utf32_pi:
	mov	rax, [.qquestiongt]
	add	rsi, 2
	sub	r10, 2
	mov	[rbx+xmltag_textstart_ofs], rsi
	cmp	r10, 2
	jb	.utf32_pi_unterminated
calign
.utf32_pi_scan:
	cmp	qword [rdi+rsi*4], rax		; '?>'
	je	.utf32_pi_scandone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 2
	jae	.utf32_pi_scan
	; fallthrough to unterminated
calign
.utf32_pi_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedprocessinginstruction
	epilog
calign
.utf32_pi_scandone:
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 2
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_processinginstruction
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog
dalign
.qdashdash:
	dd	'-', '-'
calign
.utf32_maybecomment:
	cmp	r9, [.dqcomment+8]
	jne	.utf32_element
	add	rsi, 4
	sub	r10, 4
	mov	[rbx+xmltag_textstart_ofs], rsi
	mov	rax, [.qdashdash]
	cmp	r10, 3
	jb	.utf32_comment_unterminated
calign
.utf32_comment_scan:
	cmp	qword [rdi+rsi*4], rax		; '--'
	je	.utf32_comment_maybedone
	add	rsi, 1
	sub	r10, 1
	cmp	r10, 3
	jae	.utf32_comment_scan
	; fallthrough to unterminated
calign
.utf32_comment_unterminated:
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_unterminatedcomment
	epilog
calign
.utf32_comment_maybedone:
	lea	rax, [rsi+1]
	cmp	dword [rdi+rsi*4+8], '>'
	cmovne	rsi, rax
	jne	.utf32_comment_scan
	mov	[rbx+xmltag_textend_ofs], rsi
	add	rsi, 3
	mov	[rbx+xmltag_realend_ofs], rsi
	mov	dword [rbx+xmltag_nodetype_ofs], xmltag_comment
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_noerror
	epilog

end if


if used xmlparser$skipws | defined include_everything
	; single argument: rdi == xmlparser object
	; depending on flags, moves position forward past whitespace
	; (see below where/why it is called)
falign
xmlparser$skipws:
	prolog	xmlparser$skipws
	mov	eax, [rdi+xmlparser_width_ofs]
	mov	ecx, [rdi+xmlparser_flags_ofs]
	push	rbx rdi
	mov	rbx, rsi
	mov	rsi, [rdi+xmlparser_pos_ofs]
	mov	r10, [rdi+xmlparser_end_ofs]
	mov	rdi, [rdi+xmlparser_base_ofs]
	; copy the base and width to the tag itself:
	sub	r10, rsi
	mov	[rbx+xmltag_base_ofs], rdi
	mov	dword [rbx+xmltag_width_ofs], eax
	jmp	qword [rax*8+.dispatch]
dalign
.dispatch:
	dq	.utf8, .utf16, .utf32
falign
.utf8:
	test	r10, r10
	jz	.utf8_done
	test	ecx, xmlparser_ignorewhite
	jz	.utf8_done
	; otherwise, skip whitespace and check for eod
calign
.utf8_skipwhite:
	movzx	ecx, byte [rdi+rsi]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf8_done
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf8_done
	; otherwise, we hit a 32, 9, 10, or 13
	add	rsi, 1
	sub	r10, 1
	jnz	.utf8_skipwhite
	; fallthrough to end of document
calign
.utf8_done:
	; save our position
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	epilog
falign
.utf16:
	test	r10, r10
	jz	.utf16_done
	test	ecx, xmlparser_ignorewhite
	jz	.utf16_done
	; otherwise, skip whitespace and check for eod
calign
.utf16_skipwhite:
	movzx	ecx, word [rdi+rsi*2]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf16_done
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf16_done
	; otherwise, we hit a 32, 9, 10, or 13
	add	rsi, 1
	sub	r10, 1
	jnz	.utf16_skipwhite
	; fallthrough to end of document
calign
.utf16_done:
	; save our position
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	epilog
falign
.utf32:
	test	r10, r10
	jz	.utf32_done
	test	ecx, xmlparser_ignorewhite
	jz	.utf32_done
	; otherwise, skip whitespace and check for eod
calign
.utf32_skipwhite:
	mov	ecx, [rdi+rsi*4]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf32_done
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf32_done
	; otherwise, we hit a 32, 9, 10, or 13
	add	rsi, 1
	sub	r10, 1
	jnz	.utf32_skipwhite
	; fallthrough to end of document
calign
.utf32_done:
	; save our position
	pop	rdi rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	epilog

end if


if used xmlparser$prev | defined include_everything
	; two arguments: rdi == xmlparser object, rsi == xmltag object (we'll call reset on it first)
	; returns one of the above xmlparser_ values in eax
	; (if we are at the beginning, xmlparser_endofdocument will get returned)
falign
xmlparser$prev:
	prolog	xmlparser$prev
	mov	eax, [rdi+xmlparser_width_ofs]
	mov	ecx, [rdi+xmlparser_flags_ofs]
	push	rbx r12 rdi
	mov	rbx, rsi
	mov	rsi, [rdi+xmlparser_pos_ofs]
	mov	r10, [rdi+xmlparser_end_ofs]
	mov	rdi, [rdi+xmlparser_base_ofs]
	; store the _starting position_
	mov	r12, rsi			; for the unpleasant case of <elem>></elem>
	; copy the base and width to the tag itself:
	sub	r10, rsi
	mov	[rbx+xmltag_base_ofs], rdi
	mov	dword [rbx+xmltag_width_ofs], eax
	jmp	qword [rax*8+.dispatch]
dalign
.dispatch:
	dq	.utf8, .utf16, .utf32
falign
.utf8:
	test	rsi, rsi
	jz	.utf8_endofdoc
	test	ecx, xmlparser_ignorewhite
	jz	.utf8_noskipwhite
	; otherwise, skip whitespace and check for eod
calign
.utf8_skipwhite:
	; we know rsi >0
	movzx	ecx, byte [rdi+rsi-1]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf8_noskipwhite
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf8_noskipwhite
	; otherwise, byte before rsi is whitespace, go back one
	sub	rsi, 1
	jnz	.utf8_skipwhite
	; fallthrough to end of document
calign
.utf8_endofdoc:
	; save our position:
	pop	rdi r12 rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_endofdocument
	epilog
calign
.utf8_noskipwhite:
	; char at rsi-1 is the one we're interested in
	; two possibilities:
	; 1) it is a >, check for cdata close, comment close, or PI close , else parse/scan tag backward
	; 2) it is not, in which case it is text, go back until we find a <, move forward til we find its >, move forward one and let xmltag$next do the rest
	cmp	byte [rdi+rsi-1], '>'
	je	.utf8_parsetag
	; otherwise, it is a text node, travel backward until we find a <, then call xmltag$next twice and return
	sub	rsi, 1
	jz	.utf8_textscan_done
calign
.utf8_textscan:
	cmp	byte [rdi+rsi-1], '<'
	je	.utf8_textscan_nodefound
	sub	rsi, 1
	jnz	.utf8_textscan
.utf8_textscan_done:
	; if we made it to here as a fallthrough from above, we went backward and did _not_ find a tag open (which means text at pos 0...)
	; xmlparser$next will move the pos to the end (providing no error)
	; so we need to preserve our position so we can set it again
	xchg	rsi, rbx
	mov	rdi, [rsp]
	mov	[rdi+xmlparser_pos_ofs], rbx
	call	xmlparser$next
	pop	rdi
	; so now rbx contains our previous pos... if that returned an error, leave them
	cmp	eax, xmlparser_noerror
	jne	.utf8_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
.utf8_textscan_return:
	pop	r12 rbx
	; leave the return value alone
	epilog
calign
.utf8_textscan_nodefound:
	; we found the <, but we are interested in the text tag _after_ it
	; the byte at rsi-1 is the < character, rbx is the xmltag itself
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	rsi, rbx
	call	xmlparser$next	; skip the tag itself
	mov	rdi, [rsp]
	mov	rsi, rbx
	; this position is the one we need to preserve
	mov	rbx, [rdi+xmlparser_pos_ofs]
	call	xmlparser$next	; retrieve the text tag
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf8_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
.utf8_parsetag_error_add:
	add	rsi, 1
	; fallthrough
calign
.utf8_parsetag_error:
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	; return in eax is okay
	pop	rdi
	pop	r12 rbx
	epilog
calign
.utf8_parsetag:
	; the byte at rsi-1 is a >, but since we > is allowed unescaped
	; in attributes, PI, CDATA, text, etc we have to determine which
	; kind of tag it is in order to correctly walk backward

	; Note the unpleasant case where a text node's last character is >
	; to deal with that, we walk backward til we find the unescaped
	; <, then compare the results of that tagparse's _end position_
	; to our starting position (and if they are !=, we call next again)
	mov	eax, xmlparser_unterminatedelement
	cmp	rsi, 1		; an error condition no doubt
	je	.utf8_parsetag_error
	cmp	byte [rdi+rsi-2], '?'
	je	.utf8_parsetag_pi
	cmp	byte [rdi+rsi-2], '-'
	je	.utf8_parsetag_comment
	cmp	byte [rdi+rsi-2], ']'
	je	.utf8_parsetag_cdata
	; otherwise, normal tag it is, search backward for the opener (or, unpleasant case of text >)
	sub	rsi, 1
	jz	.utf8_parsetag_error_add
calign
.utf8_tagscan:
	cmp	byte [rdi+rsi-1], '<'
	je	.utf8_tagscan_nodefound
	sub	rsi, 1
	jnz	.utf8_tagscan
	; if we fell through to here, no opening tag was found (cough)
	mov	eax, xmlparser_unterminatedelement
	add	rsi, 1
	jmp	.utf8_parsetag_error
calign
.utf8_tagscan_nodefound:
	; the byte at rsi-1 is the < character, rbx is the xmltag itself
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	rsi, rbx
	; this position is the one we need to preserve
	mov	rbx, [rdi+xmlparser_pos_ofs]
	push	rsi
	call	xmlparser$next	; skip the tag itself
	pop	rsi

	; re: case where it was actually a text node whose last character was a >, we have to check the pos now
	; and compare against our _starting_ position, if they are not the same, we need to call xmlparser$next again
	mov	rdi, [rsp]
	cmp	r12, [rdi+xmlparser_pos_ofs]
	jne	.utf8_tagscan_nodefound_textcase
.utf8_tagscan_return:
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf8_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
calign
.utf8_tagscan_nodefound_textcase:
	; since xmlparser$next only consumes whitespace _before_ it consumes, we need to possibly skip whitespace
	; and check them again
	push	rdi rsi
	call	xmlparser$skipws
	pop	rsi rdi
	mov	eax, xmlparser_noerror
	cmp	r12, [rdi+xmlparser_pos_ofs]
	je	.utf8_tagscan_return		; return if we are now equal
	; otherwise, rdi/rsi already setup for next call to get the text value
	call	xmlparser$next
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf8_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
calign
.utf8_parsetag_pi:
.utf8_parsetag_pi_scan:
	; so the word at rsi-2 == '?>', we need to search backward til we find the <? and return that
	cmp	word [rdi+rsi-1], '<?'
	je	.utf8_parsetag_startfound
	sub	rsi, 1
	jnz	.utf8_parsetag_pi_scan
	; if we fell through to here, no opening tag was found (cough)
	mov	eax, xmlparser_unterminatedprocessinginstruction
	add	rsi, 1
	jmp	.utf8_parsetag_error
calign
.utf8_parsetag_startfound:
	; word at rsi-1 is our tag start, we need to preserve this position
	sub	rsi, 1
	mov	rdi, [rsp]
	mov	[rdi+xmlparser_pos_ofs], rsi
	xchg	rsi, rbx
	call	xmlparser$next
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf8_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog

calign
.utf8_parsetag_comment:
	; so the word at rsi-2 == '->' ... make sure the byte at rsi-3 is also a -, text if not (utf8_tagscan)
	cmp	rsi, 2
	je	.utf8_tagscan
	cmp	byte [rdi+rsi-3], '-'
	jne	.utf8_tagscan
	; otherwise, search backward for <!--
calign
.utf8_parsetag_comment_scan:
	cmp	dword [rdi+rsi-1], '<!--'
	je	.utf8_parsetag_startfound
	sub	rsi, 1
	jnz	.utf8_parsetag_comment_scan
	; if we fell through to here, no opening tag was found (cough)
	mov	eax, xmlparser_unterminatedcomment
	add	rsi, 1
	jmp	.utf8_parsetag_error
calign
.utf8_parsetag_cdata:
	; so the word at rsi-2 == ']>' ... make sure the byte at rsi-3 is also a ], text if not (utf8_tagscan)
	cmp	rsi, 2
	je	.utf8_tagscan
	cmp	byte [rdi+rsi-3], ']'
	jne	.utf8_tagscan
	; otherwise, search backward for <![C DATA [
calign
.utf8_parsetag_cdata_scan:
	cmp	dword [rdi+rsi-1], '<![C'
	je	.utf8_parsetag_maybecdata
.utf8_parsetag_cdata_next:
	sub	rsi, 1
	jnz	.utf8_parsetag_cdata_scan
	; if we fell through to here, no opening tag was found (cough)
	mov	eax, xmlparser_unterminatedcdatasection
	add	rsi, 1
	jmp	.utf8_parsetag_error
calign
.utf8_parsetag_maybecdata:
	cmp	dword [rdi+rsi+3], 'DATA'
	jne	.utf8_parsetag_cdata_next
	cmp	byte [rdi+rsi+7], '['
	jne	.utf8_parsetag_cdata_next
	jmp	.utf8_parsetag_startfound


calign
.utf16:
	test	rsi, rsi
	jz	.utf16_endofdoc
	test	ecx, xmlparser_ignorewhite
	jz	.utf16_noskipwhite
	; otherwise, skip whitespace and check for eod
calign
.utf16_skipwhite:
	; we know rsi >0
	movzx	ecx, word [rdi+rsi*2-2]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf16_noskipwhite
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf16_noskipwhite
	; otherwise, byte before rsi is whitespace, go back one
	sub	rsi, 1
	jnz	.utf16_skipwhite
	; fallthrough to end of document
calign
.utf16_endofdoc:
	; save our position:
	pop	rdi r12 rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_endofdocument
	epilog
calign
.utf16_noskipwhite:
	; char at rsi-1 is the one we're interested in
	; two possibilities:
	; 1) it is a >, check for cdata close, comment close, or PI close , else parse/scan tag backward
	; 2) it is not, in which case it is text, go back until we find a <, move forward til we find its >, move forward one and let xmltag$next do the rest
	cmp	word [rdi+rsi*2-2], '>'
	je	.utf16_parsetag
	; otherwise, it is a text node, travel backward until we find a <, then call xmltag$next twice and return
	sub	rsi, 1
	jz	.utf16_textscan_done
calign
.utf16_textscan:
	cmp	word [rdi+rsi*2-2], '<'
	je	.utf16_textscan_nodefound
	sub	rsi, 1
	jnz	.utf16_textscan
.utf16_textscan_done:
	; if we made it to here as a fallthrough from above, we went backward and did _not_ find a tag open (which means text at pos 0...)
	; xmlparser$next will move the pos to the end (providing no error)
	; so we need to preserve our position so we can set it again
	xchg	rsi, rbx
	mov	rdi, [rsp]
	mov	[rdi+xmlparser_pos_ofs], rbx
	call	xmlparser$next
	pop	rdi
	; so now rbx contains our previous pos... if that returned an error, leave them
	cmp	eax, xmlparser_noerror
	jne	.utf16_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
.utf16_textscan_return:
	pop	r12 rbx
	; leave the return value alone
	epilog
calign
.utf16_textscan_nodefound:
	; we found the <, but we are interested in the text tag _after_ it
	; the word at rsi-1 is the < character, rbx is the xmltag itself
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	rsi, rbx
	call	xmlparser$next	; skip the tag itself
	mov	rdi, [rsp]
	mov	rsi, rbx
	; this position is the one we need to preserve
	mov	rbx, [rdi+xmlparser_pos_ofs]
	call	xmlparser$next	; retrieve the text tag
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf16_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
.utf16_parsetag_error_add:
	add	rsi, 1
	; fallthrough
calign
.utf16_parsetag_error:
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	rsi, rbx
	call	xmlparser$next
	pop	rdi
	pop	r12 rbx
	epilog
calign
.utf16_parsetag:
	; the word at rsi-1 is a >, but since we > is allowed unescaped
	; in attributes, PI, CDATA, text, etc we have to determine which
	; kind of tag it is in order to correctly walk backward

	; Note the unpleasant case where a text node's last character is >
	; to deal with that, we walk backward til we find the unescaped
	; <, then compare the results of that tagparse's _end position_
	; to our starting position (and if they are !=, we call next again)

	cmp	rsi, 1		; an error condition no doubt
	je	.utf16_parsetag_error
	mov	r8d, dword [.utf16_pistart]
	mov	r9, qword [.utf16_commentstart]
	cmp	word [rdi+rsi*2-4], '?'
	je	.utf16_parsetag_pi
	cmp	word [rdi+rsi*2-4], '-'
	je	.utf16_parsetag_comment
	cmp	word [rdi+rsi*2-4], ']'
	je	.utf16_parsetag_cdata
	; otherwise, normal tag it is, search backward for the opener (or, unpleasant case of text >)
	sub	rsi, 1
	jz	.utf16_parsetag_error_add
calign
.utf16_tagscan:
	cmp	word [rdi+rsi*2-2], '<'
	je	.utf16_tagscan_nodefound
	sub	rsi, 1
	jnz	.utf16_tagscan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf16_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
dalign
.utf16_pistart:
	dw	'<', '?'
.utf16_commentstart:
	dw	'<', '!', '-', '-'
.utf16_cdatastart:
	dw	'<', '!', '[', 'C'
.utf16_cdatamid:
	dw	'D', 'A', 'T', 'A'
calign
.utf16_tagscan_nodefound:
	; the word at rsi-1 is the < character, rbx is the xmltag itself
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	rsi, rbx
	; this position is the one we need to preserve
	mov	rbx, [rdi+xmlparser_pos_ofs]
	push	rsi
	call	xmlparser$next	; skip the tag itself
	pop	rsi

	; re: case where it was actually a text node whose last character was a >, we have to check the pos now
	; and compare against our _starting_ position, if they are not the same, we need to call xmlparser$next again
	mov	rdi, [rsp]
	cmp	r12, [rdi+xmlparser_pos_ofs]
	jne	.utf16_tagscan_nodefound_textcase
.utf16_tagscan_return:
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf16_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
calign
.utf16_tagscan_nodefound_textcase:
	; since xmlparser$next only consumes whitespace _before_ it consumes, we need to possibly skip whitespace
	; and check them again
	push	rdi rsi
	call	xmlparser$skipws
	pop	rsi rdi
	mov	eax, xmlparser_noerror
	cmp	r12, [rdi+xmlparser_pos_ofs]
	je	.utf16_tagscan_return		; return if we are now equal
	; otherwise, rdi/rsi already setup for next call to get the text value
	call	xmlparser$next
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf16_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
calign
.utf16_parsetag_pi:
.utf16_parsetag_pi_scan:
	; so the word at rsi-4 == '?>', we need to search backward til we find the <? and return that
	; r8d was preloaded with the UTF16 dword <? for our search
	cmp	dword [rdi+rsi*2-2], r8d
	je	.utf16_parsetag_startfound
	sub	rsi, 1
	jnz	.utf16_parsetag_pi_scan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf16_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
calign
.utf16_parsetag_startfound:
	; dword at rsi-1 is our tag start, we  need to preserve this position
	sub	rsi, 1
	mov	rdi, [rsp]
	mov	[rdi+xmlparser_pos_ofs], rsi
	xchg	rsi, rbx
	call	xmlparser$next
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf16_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
calign
.utf16_parsetag_comment:
	; so the dword at rsi-4 == '->' ... make sure the word at rsi-6 is also a -, text if not (utf16_tagscan)
	cmp	rsi, 2
	je	.utf16_tagscan
	cmp	word [rdi+rsi*2-6], '-'
	jne	.utf16_tagscan
	; otherwise, search bakcward for <!--, preloaded into r9
calign
.utf16_parsetag_comment_scan:
	cmp	qword [rdi+rsi*2-2], r9
	je	.utf16_parsetag_startfound
	sub	rsi, 1
	jnz	.utf16_parsetag_comment_scan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf16_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
calign
.utf16_parsetag_cdata:
	; so the dword at rsi-4 == ']>' ... make sure the word at rsi-6 is also a ], text if not (utf16_tagscan)
	cmp	rsi, 2
	je	.utf16_tagscan
	cmp	word [rdi+rsi*2-6], ']'
	jne	.utf16_tagscan
	mov	r8, qword [.utf16_cdatastart]
	mov	r9, qword [.utf16_cdatamid]
	; otherwise, search backward for <![C DATA [
calign
.utf16_parsetag_cdata_scan:
	cmp	qword [rdi+rsi*2-2], r8
	je	.utf16_parsetag_maybecdata
.utf16_parsetag_cdata_next:
	sub	rsi, 1
	jnz	.utf16_parsetag_cdata_scan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf16_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
calign
.utf16_parsetag_maybecdata:
	cmp	qword [rdi+rsi*2+6], r9
	jne	.utf16_parsetag_cdata_next
	cmp	word [rdi+rsi*2+14], '['
	jne	.utf16_parsetag_cdata_next
	jmp	.utf16_parsetag_startfound


calign
.utf32:
	test	rsi, rsi
	jz	.utf32_endofdoc
	test	ecx, xmlparser_ignorewhite
	jz	.utf32_noskipwhite
	; otherwise, skip whitespace and check for eod
calign
.utf32_skipwhite:
	; we know rsi >0
	mov	ecx, dword [rdi+rsi*4-4]
	mov	r8d, 1
	cmp	ecx, 32
	ja	.utf32_noskipwhite
	sub	ecx, 1
	shl	r8d, cl
	test	r8d, 2147488512
	jz	.utf32_noskipwhite
	; otherwise, byte before rsi is whitespace, go back one
	sub	rsi, 1
	jnz	.utf32_skipwhite
	; fallthrough to end of document
calign
.utf32_endofdoc:
	; save our position:
	pop	rdi r12 rbx
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	eax, xmlparser_endofdocument
	epilog
calign
.utf32_noskipwhite:
	; char at rsi-1 is the one we're interested in
	; two possibilities:
	; 1) it is a >, check for cdata close, comment close, or PI close , else parse/scan tag backward
	; 2) it is not, in which case it is text, go back until we find a <, move forward til we find its >, move forward one and let xmltag$next do the rest
	cmp	dword [rdi+rsi*4-4], '>'
	je	.utf32_parsetag
	; otherwise, it is a text node, travel backward until we find a <, then call xmltag$next twice and return
	sub	rsi, 1
	jz	.utf32_textscan_done
calign
.utf32_textscan:
	cmp	dword [rdi+rsi*4-4], '<'
	je	.utf32_textscan_done
	sub	rsi, 1
	jnz	.utf32_textscan
.utf32_textscan_done:
	; if we made it to here as a fallthrough from above, we went backward and did _not_ find a tag open (which means text at pos 0...)
	; xmlparser$next will move the pos to the end (providing no error)
	; so we need to preserve our position so we can set it again
	xchg	rsi, rbx
	mov	rdi, [rsp]
	mov	[rdi+xmlparser_pos_ofs], rbx
	call	xmlparser$next
	pop	rdi
	; so now rbx contains our previous pos... if that returned an error, leave them
	cmp	eax, xmlparser_noerror
	jne	.utf32_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
.utf32_textscan_return:
	pop	r12 rbx
	; leave the return value alone
	epilog
calign
.utf32_textscan_nodefound:
	; we found the <, but we are interested in the text tag _after_ it
	; the character at rsi-1 is the <, rbx is the xmltag itself
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	rsi, rbx
	call	xmlparser$next	; skip the tag itself
	mov	rdi, [rsp]
	mov	rsi, rbx
	; this position is the one we need to preserve
	mov	rbx, [rdi+xmlparser_pos_ofs]
	call	xmlparser$next	; retrieve the text tag
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf32_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
.utf32_parsetag_error_add:
	add	rsi, 1
	; fallthrough
calign
.utf32_parsetag_error:
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	rsi, rbx
	call	xmlparser$next
	pop	rdi
	pop	r12 rbx
	epilog
calign
.utf32_parsetag:
	; the character at rsi-1 is a >, but since we > is allowed unescaped
	; in attributes, PI, CDATA, text, etc we have to determine which
	; kind of tag it is in order to correctly walk backward

	; Note the unpleasant case where a text node's last character is >
	; to deal with that, we walk backward til we find the unescaped
	; <, then compare the results of that tagparse's _end position_
	; to our starting position (and if they are !=, we call next again)

	cmp	rsi, 1		; an error condition no doubt
	je	.utf32_parsetag_error
	mov	r8, qword [.utf32_pistart]
	cmp	dword [rdi+rsi*4-8], '?'
	je	.utf32_parsetag_pi
	cmp	dword [rdi+rsi*4-8], '-'
	je	.utf32_parsetag_comment
	cmp	dword [rdi+rsi*4-8], ']'
	je	.utf32_parsetag_cdata
	; otherwise, normal tag it is, search backward for the opener (or, unpleasant case of text >)
	sub	rsi, 1
	jz	.utf32_parsetag_error_add
calign
.utf32_tagscan:
	cmp	dword [rdi+rsi*4-4], '<'
	je	.utf32_tagscan_nodefound
	sub	rsi, 1
	jnz	.utf32_tagscan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf32_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
dalign
.utf32_pistart:
	dd	'<', '?'
.utf32_commentstart:
	dd	'<', '!'
.utf32_commentmid:
	dd	'-', '-'
.utf32_cdatastart:
	dd	'<', '!'
.utf32_cdatamid1:
	dd	'[', 'C'
.utf32_cdatamid2:
	dd	'D', 'A'
.utf32_cdatamid3:
	dd	'T', 'A'
calign
.utf32_tagscan_nodefound:
	; the char at rsi-1 is the < character, rbx is the xmltag itself
	mov	rdi, [rsp]
	sub	rsi, 1
	mov	[rdi+xmlparser_pos_ofs], rsi
	mov	rsi, rbx
	; this position is the one we need to preserve
	mov	rbx, [rdi+xmlparser_pos_ofs]
	push	rsi
	call	xmlparser$next	; skip the tag itself
	pop	rsi

	; re: case where it was actually a text node whose last character was a >, we have to check the pos now
	; and compare against our _starting_ position, if they are not the same, we need to call xmlparser$next again
	mov	rdi, [rsp]
	cmp	r12, [rdi+xmlparser_pos_ofs]
	jne	.utf32_tagscan_nodefound_textcase
.utf32_tagscan_return:
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf32_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
calign
.utf32_tagscan_nodefound_textcase:
	; since xmlparser$next only consumes whitespace _before_ it consumes, we need to possibly skip whitespace
	; and check them again
	push	rdi rsi
	call	xmlparser$skipws
	pop	rsi rdi
	mov	eax, xmlparser_noerror
	cmp	r12, [rdi+xmlparser_pos_ofs]
	je	.utf32_tagscan_return		; return if we are now equal
	; otherwise, rdi/rsi already setup for next call to get the text value
	call	xmlparser$next
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf32_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
calign
.utf32_parsetag_pi:
.utf32_parsetag_pi_scan:
	; so the qword at rsi-2 == '?>', we need to search backward til we find the <? and return that
	cmp	qword [rdi+rsi*4-4], r8
	je	.utf32_parsetag_startfound
	sub	rsi, 1
	jnz	.utf32_parsetag_pi_scan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf32_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
calign
.utf32_parsetag_startfound:
	; qword at rsi-1 is our tag start, we need to preserve this position
	sub	rsi, 1
	mov	rdi, [rsp]
	mov	[rdi+xmlparser_pos_ofs], rsi
	xchg	rsi, rbx
	call	xmlparser$next
	pop	rdi
	cmp	eax, xmlparser_noerror
	jne	.utf32_textscan_return
	mov	[rdi+xmlparser_pos_ofs], rbx
	pop	r12 rbx
	epilog
calign
.utf32_parsetag_comment:
	; so the qword at rsi-2 == '->' ... make sure the dword at rsi-12 is also a -, text if not (utf32_tagscan)
	cmp	rsi, 2
	je	.utf32_tagscan
	cmp	dword [rdi+rsi*4-12], '-'
	jne	.utf32_tagscan
	; otherwise, search backward for <!--
	mov	r8, qword [.utf32_commentstart]
	mov	r9, qword [.utf32_commentmid]
calign
.utf32_parsetag_comment_scan:
	cmp	qword [rdi+rsi*4-4], r8
	je	.utf32_parsetag_maybecomment
	sub	rsi, 1
	jnz	.utf32_parsetag_comment_scan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf32_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
calign
.utf32_parsetag_maybecomment:
	cmp	qword [rdi+rsi*4+4], r9
	je	.utf32_parsetag_startfound
	sub	rsi, 1
	jnz	.utf32_parsetag_comment_scan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf32_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
calign
.utf32_parsetag_cdata:
	; so the qword at rsi-2 == ']>' ... make sure the dword at rsi-12 is also a ], text if not (utf32_tagscan)
	cmp	rsi, 2
	je	.utf32_tagscan
	cmp	dword [rdi+rsi*4-12], ']'
	jne	.utf32_tagscan
	; otherwise, search backward for <![C DATA [
	mov	r8, qword [.utf32_cdatastart]
	mov	r9, qword [.utf32_cdatamid1]
	mov	r10, qword [.utf32_cdatamid2]
	mov	r11, qword [.utf32_cdatamid3]
calign
.utf32_parsetag_cdata_scan:
	cmp	qword [rdi+rsi*4-4], r8
	je	.utf32_parsetag_maybecdata
.utf32_parsetag_cdata_next:
	sub	rsi, 1
	jnz	.utf32_parsetag_cdata_scan
	; if we fell through to here, no opening tag was found (cough)
	add	rsi, 1
	jmp	.utf32_parsetag_error	; TODO: this won't return an error, it will return a text node, think about this some more
calign
.utf32_parsetag_maybecdata:
	cmp	qword [rdi+rsi*4+4], r9
	jne	.utf32_parsetag_cdata_next
	cmp	qword [rdi+rsi*4+12], r10
	jne	.utf32_parsetag_cdata_next
	cmp	qword [rdi+rsi*4+20], r11
	jne	.utf32_parsetag_cdata_next
	cmp	dword [rdi+rsi*4+28], '['
	jne	.utf32_parsetag_cdata_next
	jmp	.utf32_parsetag_startfound
	
	
end if


if used xmlparser$destroy | defined include_everything
	; placeholder function only, just calls heap$free
	; single argument in rdi: an xmlparser object
falign
xmlparser$destroy:
	prolog	xmlparser$destroy
	call	heap$free
	epilog

end if
