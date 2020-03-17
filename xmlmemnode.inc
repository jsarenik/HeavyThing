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
	; xmlmemnode: an XML node representation that is all in-memory.
	;
	; What this means is that if given an xmlparser object as a construction
	; method, that it will scan the entirety of the document, convert all
	; tag names, attribute names and values, text, cdata, etc into native
	; strings, construct child object lists of nested nodes, the whole
	; shooting match.
	;
	; The reason it is called xmlmemnode is so that it is plain to see that
	; it literally loads the entirety into memory (rather than other methods
	; I have that use lazy on-demand based DOM traversal).
	;
	; This design was _not_ made for memory efficiency (read: this uses a lot
	; of memory to do its work). The way the library's default memory manager
	; works is bin-based allocations, minimum size of 64 bytes per alloc,
	; combine that with UTF32 native strings and 8 byte length prefaces, and
	; our node size minimums, well, you get the idea. I don't work in memory
	; constrained environments very often, so this still suits me just fine.
	; YMMV.
	;
	; Depending on your specific needs, you may find the "pull parser" used
	; herein (xmlparser.inc) will be much friendlier memory-wise (it uses
	; none aside from its state and a single tag space). Searching this file
	; for xmlparser$next reveals how the parser state works.
	;
	; Also note: this is not an exact DOM spec model, but is close enough to
	; see the resemblance. :-)
	;



	; first things first: namespaces (also memory based, so we include in here)

if used xmlmemns$new | used xmlmemns$new_nocopy | defined include_everything

xmlmemns_prefix_ofs = 0
xmlmemns_uri_ofs = 8

xmlmemns_size = 16

end if

if used xmlmemns$new | defined include_everything

	; two arguments: rdi == string prefix, rsi == string uri
	; returns a new xmlmemns object in rax
	; NOTE: copies both strings, see xmlmemns$new_nocopy for the alternative
falign
xmlmemns$new:
	prolog	xmlmemns$new
	push	rbx rdi rsi
	mov	edi, xmlmemns_size
	call	heap$alloc
	mov	rbx, rax
	pop	rdi
	call	string$copy
	mov	[rbx+xmlmemns_uri_ofs], rax
	pop	rdi
	call	string$copy
	mov	[rbx+xmlmemns_prefix_ofs], rax
	mov	rax, rbx
	pop	rbx
	epilog

end if


if used xmlmemns$new_nocopy | defined include_everything
	; two arguments: rdi == string prefix, rsi == string uri
	; returns a new xmlmemns object in rax
	; NOTE: assumes ownership of both strings
falign
xmlmemns$new_nocopy:
	prolog	xmlmemns$new_nocopy
	push	rdi rsi
	mov	edi, xmlmemns_size
	call	heap$alloc
	pop	rsi rdi
	mov	[rax+xmlmemns_prefix_ofs], rdi
	mov	[rax+xmlmemns_uri_ofs], rsi
	epilog

end if


if used xmlmemns$destroy | defined include_everything
	; single argument in rdi: an xmlmemns object
falign
xmlmemns$destroy:
	prolog	xmlmemns$destroy
	push	rdi
	push	qword [rdi+xmlmemns_prefix_ofs]
	mov	rdi, [rdi+xmlmemns_uri_ofs]
	call	heap$free
	pop	rdi
	call	heap$free
	pop	rdi
	call	heap$free
	epilog

end if


if used xmlmemns$has_prefix | defined include_everything
	; single argument in rdi: an xmlmemns object
	; returns bool as to whether the prefix string's length is >0
falign
xmlmemns$has_prefix:
	prolog	xmlmemns$has_prefix
	mov	rsi, [rdi+xmlmemns_prefix_ofs]
	mov	eax, 1
	xor	edx, edx
	cmp	qword [rsi], 0
	cmove	eax, edx
	epilog

end if


if used xmlmemns$equals | defined include_everything
	; two arguments: rdi == an xmlmemns object, rsi == another xmlmemns object
	; returns bool as to whether their URIs are the same
falign
xmlmemns$equals:
	prolog	xmlmemns$equals
	mov	rdi, [rdi+xmlmemns_uri_ofs]
	mov	rsi, [rsi+xmlmemns_uri_ofs]
	call	string$equals
	epilog

end if



if used xmlmemnode$new_type | used xmlmemnode$new_string | used xmlmemnode$new_parser | defined include_everything


; first up: possible xmlmemnode types (specified as flags so multiple type compares are easier):
xmlmemnode_type_unknown = 0x0001
xmlmemnode_type_attribute = 0x0002
xmlmemnode_type_text = 0x0004
xmlmemnode_type_cdata = 0x0008
xmlmemnode_type_comment = 0x00010
xmlmemnode_type_pi = 0x0020
xmlmemnode_type_element = 0x0040
xmlmemnode_type_document = 0x0080
xmlmemnode_type_document_fragment = 0x0100
xmlmemnode_type_doctype = 0x0200

; flags that may also be included:
xmlmemnode_flag_name_owner = 0x0400
xmlmemnode_flag_ns_owner = 0x0800

; next up: offsets specific to xmlmemnode objects themselves, noting here that sizes may vary depending on the kind
; of node it is
xmlmemnode_parent_ofs = 0
xmlmemnode_name_ofs = 8
xmlmemnode_ns_ofs = 16
xmlmemnode_flags_ofs = 24		; one of the xmlmemnode_type_* defined above, or xmlmemnode_flag_* above
; all node types will have the above fields

; unioned on purpose (their use is dependent upon what kind of node it is)
xmlmemnode_names_ofs = 32		; document and document_fragment nodes contain element/attribute name caches
xmlmemnode_attributes_ofs = 32
xmlmemnode_value_ofs = 32

; element nodes contain all of the above fields as well as the below two:
xmlmemnode_namespaces_ofs = 40
xmlmemnode_children_ofs = 48

; so an xmlmemnode with a type of element, document, or document_fragment is 56
; all others are 40, and since our HeavyThing library's allocator granularity
; is 64 bytes, it makes little difference to the amount of memory a node will use
; but we distinguish them anyway

; note of course that a given node will use considerably more than this for its other
; various bits (string names, vectors for children, attributes, etc etc)

xmlmemnode_element_size = 56
xmlmemnode_size = 40


end if


if used xmlmemnode$new_type | defined include_everything
	; two arguments: edi == xmlmemnode_type_* value, rsi == pointer to parent
falign
xmlmemnode$new_type:
	prolog	xmlmemnode$new_type
	mov	edx, edi
	mov	edi, xmlmemnode_size
	mov	ecx, xmlmemnode_element_size
	test	edx, xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	cmovnz	edi, ecx
	push	rdx rsi
	call	heap$alloc_clear
	pop	rsi rdi
	mov	[rax+xmlmemnode_parent_ofs], rsi
	mov	[rax+xmlmemnode_flags_ofs], edi
	test	edi, xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jnz	.withnames
	epilog
.withnames:
	push	rax
	xor	edi, edi
	call	stringmap$new
	mov	rdi, rax
	pop	rax
	mov	[rax+xmlmemnode_names_ofs], rdi
	epilog

end if


if used xmlmemnode$new_string | defined include_everything
	; single argument in rdi: a string to parse
	; returns either a document node or a document_fragment node in rax (or null on error)
	; Note: this constructs a stack-based xmlparser, then calls xmlmemnode$new_parser
falign
xmlmemnode$new_string:
	prolog	xmlmemnode$new_string
	sub	rsp, xmlparser_size
	mov	rsi, rdi
	mov	rdi, rsp
	mov	edx, xmlparser_ignorewhite or xmlparser_condensewhite		; TODO: make these ht_defaults.inc settings
	call	xmlparser$init_string
	mov	rdi, rsp
	call	xmlmemnode$new_parser
	add	rsp, xmlparser_size
	epilog

end if


if used xmlmemnode$new_parser | defined include_everything
	; single argument in rdi: an xmlparser object
	; returns either a document node or a document_fragment node in rax (or null on error)
	; in the event rax is null, extended error information (such as an error return from xmlparser) will be in edx
falign
xmlmemnode$new_parser:
	prolog	xmlmemnode$new_parser
	push	rbx r12 r13 r14 r15
	mov	rbx, rdi
	sub	rsp, xmltag_size
	mov	rdi, rsp
	call	xmltag$reset
	; we'll use r12 as "this", r13 as "p", r14d as the status return from the xmlparser
	xor	r12d, r12d
	xor	r13d, r13d
calign
.outer:
	mov	rdi, rbx
	mov	rsi, rsp
	call	xmlparser$next

if defined xmlmemnode_debug
	; debug
	mov	rdi, rsp
	push	rax
	call	xmltag$debug
	pop	rax
	; end debug
end if

	mov	ecx, [rsp+xmltag_nodetype_ofs]
	mov	r14d, eax
	cmp	eax, xmlparser_noerror
	jne	.bailout
	jmp	qword [rcx*8+.dispatch]
dalign
.dispatch:
	dq	.bailout, .element, .textnode, .cdata, .pi, .comment, .doctype, .xmldecl
falign
.findnamespace:
	; local function that gets called during new element and element close
	; on entry, rdi == node
	; r15 is still a valid copy of our tag's text, and the one that will be passed to
	; setqname_nocopy (on element create, else it is checked on close)
	; we will return -1 on badqname, -2 on prefixnotbound, otherwise, either 0 or an xmlmemns pointer
	; first up, find the colon (ignoring the xmltag's idea since that is pre-escapign/conversion)
	push	rbx r13 r14
	mov	rbx, rdi			; hangon to the node
	mov	rdi, r15
	mov	esi, ':'
	call	string$indexof_charcode
	cmp	rax, 0
	jg	.findnamespace_prefix
	je	.findnamespace_badqname
	; no colon in the tag text, so no prefix ... so we have to search for an empty (default) prefix in our
	; namespace scope chain
	; walk the node up in rbx looking for it
calign
.findnamespace_finddefault_outer:
	mov	r13, [rbx+xmlmemnode_namespaces_ofs]
	test	r13, r13
	jz	.findnamespace_finddefault_nextouter
	cmp	qword [r13+vector_count_ofs], 0
	je	.findnamespace_finddefault_nextouter
	xor	r14d, r14d
calign
.findnamespace_finddefault_inner:
	mov	rdi, r13
	mov	esi, r14d
	add	r14d, 1
	call	vector$at
	mov	rdi, [rax+xmlmemns_prefix_ofs]
	cmp	qword [rdi], 0
	je	.findnamespace_founddefault
	cmp	r14d, [r13+vector_count_ofs]
	jb	.findnamespace_finddefault_inner
.findnamespace_finddefault_nextouter:
	mov	rbx, [rbx+xmlmemnode_parent_ofs]
	test	rbx, rbx
	jnz	.findnamespace_finddefault_outer
	; otherwise, no default namespace was found
	pop	r14 r13 rbx
	xor	eax, eax
	ret
calign
.findnamespace_badqname:
	pop	r14 r13 rbx
	mov	rax, -1
	ret
calign
.findnamespace_founddefault:
	; xmlmemns object in rax is our return
	pop	r14 r13 rbx
	ret
calign
.findnamespace_prefix:
	; split our prefix and tag text on rax
	mov	r13, rax
	mov	rdi, r15
	xor	esi, esi
	mov	rdx, rax
	call	string$substring
	mov	r14, rax		; temporarily hangon to our prefix
	mov	rdi, r15
	lea	rsi, [r13+1]
	mov	rdx, [r15]
	call	string$substring
	mov	rdi, r15
	mov	r15, rax
	call	heap$free
	; make sure the remaining name is not empty
	cmp	qword [r15], 0
	je	.findnamespace_badqname
	; so we are done with the tag text since we are only looking for the namespace
	; preserve r15 and use it for our prefix search
	push	r15
	mov	r15, r14
calign
.findnamespace_prefix_outer:
	mov	r13, [rbx+xmlmemnode_namespaces_ofs]
	test	r13, r13
	jz	.findnamespace_prefix_nextouter
	cmp	qword [r13+vector_count_ofs], 0
	je	.findnamespace_prefix_nextouter
	xor	r14d, r14d
calign
.findnamespace_prefix_inner:
	mov	rdi, r13
	mov	esi, r14d
	add	r14d, 1
	call	vector$at
	mov	rdi, [rax+xmlmemns_prefix_ofs]
	mov	rsi, r15
	push	rax
	call	string$equals
	pop	rdx
	test	eax, eax
	jnz	.findnamespace_foundprefix
	cmp	r14d, [r13+vector_count_ofs]
	jb	.findnamespace_prefix_inner
.findnamespace_prefix_nextouter:
	mov	rbx, [rbx+xmlmemnode_parent_ofs]
	test	rbx, rbx
	jnz	.findnamespace_prefix_outer
	; otherwise, no default namespace was found, get rid of our prefix string
	mov	rdi, r15
	call	heap$free
	pop	r15 r14 r13 rbx
	mov	rax, -2		; prefix not bound
	ret
calign
.findnamespace_foundprefix:
	; xmlmemns object in rdx is our return, but we have to get rid of the prefix string in r15
	mov	r14, rdx
	mov	rdi, r15
	call	heap$free
	mov	rax, r14
	pop	r15 r14 r13 rbx
	ret
falign
.copyattrsandns:
	; local function that gets called during new element construction
	; on entry, rdi == node, rsi == xmltag
	; our job is to iterate through the attributes (if any, if none, just return)
	; paying attention to xmlns entries, and if they aren't namespace attributes
	; then add them as normal attribute nodes to the node passed in rdi
	; if we puke on something, we'll set r14d to our xmlparser_ error code
	cmp	dword [rsi+xmltag_attrcount_ofs], 0
	je	.copyattrsandns_ret
	push	rbx r12 r13 r14 r15
	mov	rbx, rdi
	mov	r12, rsi
	xor	r13d, r13d
calign
.copyattrsandns_loop:
	mov	rdi, r12
	mov	esi, r13d
	call	xmltag$getattr
	mov	r14, rax
	mov	rdi, r12
	mov	rsi, [rax+xmltagattr_namestart_ofs]
	mov	rdx, [rax+xmltagattr_nameend_ofs]
	call	xmltag$unescape
	mov	r15, rax
	cmp	qword [rax], 5
	jb	.copyattrsandns_loop_normalattr
	mov	rdi, rax
	mov	rsi, .xmlns
	call	string$starts_with
	test	eax, eax
	jz	.copyattrsandns_loop_normalattr
	cmp	qword [r15], 5
	je	.copyattrsandns_loop_defaultns
	mov	rdi, r15
	mov	esi, 5
	call	string$charat
	cmp	eax, ':'
	jne	.copyattrsandns_loop_normalattr
	cmp	qword [r15], 6
	je	.copyattrsandns_loop_badqname
	; otherwise, prefix is from 6 ->, attribute value is the URI
	mov	rdi, r15
	mov	esi, 6
	mov	rdx, -1
	call	string$substr
	mov	rdi, r15
	mov	r15, rax
	call	heap$free
	; so now, get the attribute value out of the tag (unescaped)
.copyattrsandns_loop_xmlns_doit:
	mov	rdi, r12
	mov	rsi, [r14+xmltagattr_valuestart_ofs]
	mov	rdx, [r14+xmltagattr_valueend_ofs]
	call	xmltag$unescape
	mov	rdi, r15		; prefix
	mov	rsi, rax		; uri
	call	xmlmemns$new_nocopy
	; now, we only want to add this prefix/uri namespace if it isn't already in here
	; more than one prefix can reference the same URI, so we don't care if there are multiples
	; if the same prefix already exists, overwrite its URI
	mov	r15, rax		; hangon to our new xmlmemns object
	; we are done with r14 as well
	; so, we know that the node in rbx is an element node (cuz element type construction is the only place we get called from)
	; if the xmlmemnode_namespaces_ofs is null/empty, just add it
	cmp	qword [rbx+xmlmemnode_namespaces_ofs], 0
	je	.copyattrsandns_loop_xmlns_new
	; otherwise, there are already namespaces here, so we have to determine whether there is a prefix mash
	; thanks to markup like this: <elem xmlns:pre="one" xmlns:pre="two" />
	; we know for certain (thanks to use being in parse stage) that there
	; is at least one namespace here
	; scan for a prefix match
	mov	r14, [rbx+xmlmemnode_namespaces_ofs]
	push	r13
	xor	r13d, r13d
calign
.copyattrsandns_loop_xmlns_prefixsearch:
	mov	rdi, r14
	mov	esi, r13d
	call	vector$at
	mov	rdi, [rax+xmlmemns_prefix_ofs]
	mov	rsi, [r15+xmlmemns_prefix_ofs]
	call	string$equals
	test	eax, eax
	jnz	.copyattrsandns_loop_prefixmash
	; otherwise, no match
	add	r13d, 1
	cmp	r13d, [r14+vector_count_ofs]
	jb	.copyattrsandns_loop_xmlns_prefixsearch
	; if we fell through, no prefix match occurred, so go ahead and add our xmlns:
	pop	r13
	mov	rdi, r14
	mov	rsi, r15
	call	vector$push_back
	; all done, go to the next attribute
	add	r13d, 1
	cmp	r13d, [r12+xmltag_attrcount_ofs]
	jb	.copyattrsandns_loop
	pop	r15 r14 r13 r12 rbx
	ret
calign
.copyattrsandns_loop_normalattr:
	; so when we get here, r14 is the xmltagattr object, r15 is the unescaped attribute name that is _not_ an xmlns
	; we can go ahead and unescape the attribute value:
	mov	rdi, r12
	mov	rsi, [r14+xmltagattr_valuestart_ofs]
	mov	rdx, [r14+xmltagattr_valueend_ofs]
	call	xmltag$unescape
	mov	r14, rax
	; so now, r14 is the unescaped attribute value, r15 is the unescaped attribute name
	; go ahead and create an attribute node, with its parent set to rbx
	mov	edi, xmlmemnode_type_attribute
	mov	rsi, rbx
	call	xmlmemnode$new_type
	; set its text value so we can reuse r14 to hangon to the node
	mov	rdi, rax
	mov	rsi, r14
	mov	r14, rax
	call	xmlmemnode$settextvalue_nocopy
	; so now, r14 is our new attribute node with its value already set, r15 is the unescaped attribute name
	; find the namespace (if any) for this attributeName so we can set its QName
	mov	rdi, r15
	mov	esi, ':'
	call	string$indexof_charcode
	xor	edx, edx
	cmp	rax, 0
	jl	.copyattrsandns_loop_attr_ns_found
	je	.copyattrsandns_loop_attr_badqname
	; otherwise, get our prefix, set our attribute name in r15 to its unprefixed remainder
	push	rax
	mov	rdi, r15
	xor	esi, esi
	mov	rdx, rax
	call	string$substring
	mov	rdi, r15
	mov	rsi, [rsp]
	mov	rdx, [r15]
	add	rsi, 1
	mov	[rsp], rax
	call	string$substring
	mov	rdi, r15
	mov	r15, rax
	call	heap$free
	; when we get here, r14 is our attribute node with its textvalue already set
	; so now, [rsp] is the prefix string, and r15 is the non-prefixed attribute name
	; now, starting at our node in rbx, find our prefix in the namespace scope
	; (scope == rbx, its parent, its parent, etc. etc.)
	push	rbx r13 r14
calign
.copyattrsandns_loop_attr_prefixsearch_outer:
	mov	r14, [rbx+xmlmemnode_namespaces_ofs]
	xor	r13d, r13d
	test	r14, r14
	jz	.copyattrsandns_loop_attr_prefixsearch_nextscope
	; otherwise, there are namespaces here, search for our prefix in [rsp+24]
calign
.copyattrsandns_loop_attr_prefixsearch_nsloop:
	mov	rdi, r14
	mov	esi, r13d
	call	vector$at
	; xmlmemns object in rax, get its prefix
	mov	rdi, [rsp+24]
	mov	rsi, [rax+xmlmemns_prefix_ofs]
	push	rax
	call	string$equals
	pop	rdx
	test	eax, eax
	jnz	.copyattrsandns_loop_attr_ns_found_stackmod
	add	r13d, 1
	cmp	r13d, dword [r14+vector_count_ofs]
	jb	.copyattrsandns_loop_attr_prefixsearch_nsloop
.copyattrsandns_loop_attr_prefixsearch_nextscope:
	mov	rbx, [rbx+xmlmemnode_parent_ofs]
	test	rbx, rbx
	jnz	.copyattrsandns_loop_attr_prefixsearch_outer
	; otherwise, we went all the way up and didn't find the prefix in scope
	; fail with a prefixnotbound error
	pop	r14 r13 rbx
	pop	rdi
	call	heap$free		; the failed prefix string
	mov	rdi, r14
	call	xmlmemnode$destroy
	mov	rdi, r15		; the rest of the attr name
	call	heap$free
	pop	r15 r14 r13 r12 rbx
	mov	r14d, xmlparser_prefixnotbound
	ret
calign
.copyattrsandns_loop_attr_ns_found_stackmod:
	; xmlmemns object pointer sitting in rdx is the one we were after
	; but we have to cleanup our stack and free the prefix string
	pop	r14 r13 rbx
	mov	rdi, [rsp]		; the prefix string
	mov	[rsp], rdx
	call	heap$free
	pop	rdx
	; fallthrough to attr_ns_found:
calign
.copyattrsandns_loop_attr_ns_found:
	; ns sitting in rdx for our call to setqname
	; attribute node is in r14, r15 is our attribute name sans the prefix

	; hangon to our ns in the newly created attribute node:
	mov	[r14+xmlmemnode_ns_ofs], rdx
	; rdx is still okay, but we have to hangon to its value for our dup checking
	push	rbx r13 r14
	mov	r13, rdx
	xor	r14d, r14d
	sub	rsp, 8			; need room to store one more var
	; so now, well-formed xml isn't allowed to contain duplicate attribute names
	; Soooooo, first up, make sure our node in rbx actually has an attribute list
	mov	rax, [rbx+xmlmemnode_attributes_ofs]
	test	rax, rax
	jnz	.copyattrsandns_loop_attr_attrsokay
	call	vector$new
	mov	[rbx+xmlmemnode_attributes_ofs], rax
.copyattrsandns_loop_attr_attrsokay:
	mov	rbx, rax
	; so now, we have to iterate through each attribute and check for duplicate
	cmp	qword [rax+vector_count_ofs], 0
	je	.copyattrsandns_loop_attr_notdup
calign
.copyattrsandns_loop_attr_duploop:
	; get the attribute at position r14d
	mov	rdi, rbx
	mov	esi, r14d
	call	vector$at
	; do a qname match check against it w/ r15 (the non-prefixed name) and the xmlmemns object in r13
	; the xmlmemns object might be zero
	; hangon to the attribute node:
	mov	[rsp], rax
	; compare the names alone first, then check namespaces
	mov	rdi, r15
	mov	rsi, [rax+xmlmemnode_name_ofs]
	call	string$equals
	test	eax, eax
	jz	.copyattrsandns_loop_attr_dupnext
	; localname matches this attribute node's name, see if their namespaces are the same
	mov	rax, [rsp]
	cmp	r13, [rax+xmlmemnode_ns_ofs]		; pointer-exact match before string compare of URI
	je	.copyattrsandns_loop_attr_duplicate
	test	r13, r13
	jnz	.copyattrsandns_loop_attr_dup_withns
	; otherwise, our localname is without an ns, which means the node we are comparing to
	; has a namespace...
	; special case here where we are supposed to check against the "real" default namespace:
	mov	rdx, [rax+xmlmemnode_ns_ofs]
	mov	rdi, [rdx+xmlmemns_uri_ofs]
	mov	rsi, .defaultnamespace
	call	string$equals
	test	eax, eax
	jnz	.copyattrsandns_loop_attr_duplicate
	; otherwise, fallthrough to dupnext:
calign
.copyattrsandns_loop_attr_dupnext:
	add	r14d, 1
	cmp	r14d, [rbx+vector_count_ofs]
	jb	.copyattrsandns_loop_attr_duploop
	; if we made it to the end, go ahead and fallthrough to notdup:
calign
.copyattrsandns_loop_attr_notdup:
	add	rsp, 8
	; add the attribute to the attributes vector in rbx
	pop	r14 r13
	mov	rdi, rbx
	mov	rsi, r14
	call	vector$push_back
	pop	rbx
	; set the attribute's parent to the node in rbx
	mov	[r14+xmlmemnode_parent_ofs], rbx
	; set its qname to r15
	mov	rdi, r14
	mov	rsi, r15
	mov	rdx, [r14+xmlmemnode_ns_ofs]
	call	xmlmemnode$setqname_nocopy
	; all done, go to the next attribute
	add	r13d, 1
	cmp	r13d, [r12+xmltag_attrcount_ofs]
	jb	.copyattrsandns_loop
	pop	r15 r14 r13 r12 rbx
	ret
cleartext .defaultnamespace, 'http://www.w3.org/XML/1998/namespace'
calign
.copyattrsandns_loop_attr_dup_withns:
	; so, r13 (an xmlmemns object) is nonzero, our comparison node at [rsp] may or may not have one
	cmp	qword [rax+xmlmemnode_ns_ofs], 0
	je	.copyattrsandns_loop_attr_dup_checkdefault
	; otherwise, both r13 and the comparison node have xmlmemns objects that are not the same pointer
	mov	rdx, [rax+xmlmemnode_ns_ofs]
	mov	rdi, [r13+xmlmemns_uri_ofs]
	mov	rsi, [rdx+xmlmemns_uri_ofs]
	call	string$equals
	test	eax, eax
	jz	.copyattrsandns_loop_attr_notdup
	jmp	.copyattrsandns_loop_attr_duplicate
calign
.copyattrsandns_loop_attr_dup_checkdefault:
	mov	rdi, [r13+xmlmemns_uri_ofs]
	mov	rsi, .defaultnamespace
	call	string$equals
	test	eax, eax
	jz	.copyattrsandns_loop_attr_notdup
	; otherwise, fallthrough to copyattrsandns_loop_attr_duplicate
calign
.copyattrsandns_loop_attr_duplicate:
	add	rsp, 8
	pop	r14 r13 rbx
	mov	rdi, r14
	call	xmlmemnode$destroy
	; since we didn't yet setqname on the attribute node, we also have to free the string in r15
	mov	rdi, r15
	call	heap$free
	pop	r15 r14 r13 r12 rbx
	mov	r14d, xmlparser_duplicateattribute
	ret
	
calign
.copyattrsandns_loop_attr_badqname:
	; r14 node has to be destroyed
	mov	rdi, r14
	call	xmlmemnode$destroy
	; fallthrough to copyattrsandns_loop_badqname
calign
.copyattrsandns_loop_badqname:
	; when we get here, r15 == a bad qname, something like "xmlns:" or similar
	mov	rdi, r15
	call	heap$free
	pop	r15 r14 r13 r12 rbx
	mov	r14d, xmlparser_badqname
	ret
	
calign
.copyattrsandns_loop_prefixmash:
	; so we found a matching prefix to overwrite, its at offset r13d
	mov	rdi, r14
	mov	esi, r13d
	call	vector$at
	mov	rsi, [r15+xmlmemns_uri_ofs]
	mov	rdi, [rax+xmlmemns_uri_ofs]
	mov	[rax+xmlmemns_uri_ofs], rsi
	mov	[r15+xmlmemns_uri_ofs], rdi	; swapped
	pop	r13
	; now we can destroy our new xmlmemns:
	mov	rdi, r15
	call	xmlmemns$destroy
	; all done, go to the next attribute
	add	r13d, 1
	cmp	r13d, [r12+xmltag_attrcount_ofs]
	jb	.copyattrsandns_loop
	pop	r15 r14 r13 r12 rbx
	ret
calign
.copyattrsandns_loop_xmlns_new:
	; we want a vector with precisely one item in it
	mov	rdi, r15
	call	vector$new_fromunsigned
	mov	[rbx+xmlmemnode_namespaces_ofs], rax

	; all done, go to the next attribute
	add	r13d, 1
	cmp	r13d, [r12+xmltag_attrcount_ofs]
	jb	.copyattrsandns_loop
	pop	r15 r14 r13 r12 rbx
.copyattrsandns_ret:
	ret

calign
.copyattrsandns_loop_defaultns:
	; r15 == "xmlns" and nothing more
	mov	rdi, r15
	call	heap$free
	call	string$new
	mov	r15, rax
	jmp	.copyattrsandns_loop_xmlns_doit
cleartext .xmlns, 'xmlns'

calign
.element:
	; so if we got here and there is no r12, we need to promote ourselves to a document fragment
	; (no doctype or ?xmldecl exists)
	test	r12, r12
	jnz	.element_nopromote
	mov	edi, xmlmemnode_type_document_fragment
	xor	esi, esi
	call	xmlmemnode$new_type
	mov	r12, rax
	mov	r13, rax
.element_nopromote:
	; get the tag's text:
	mov	rdi, rsp
	call	xmltag$text
	mov	r15, rax
	; xmlparser would have errored out if it was a null tag name, so check the first character
if string_bits = 32
	cmp	dword [r15+8], '/'
else
	cmp	word [r15+8], '/'
end if
	je	.element_closing
	; otherwise, create a new element node
	mov	edi, xmlmemnode_type_element
	xor	esi, esi
	call	xmlmemnode$new_type
	push	rax
	mov	rdi, r13
	mov	rsi, rax
	call	xmlmemnode$appendchild
	mov	rax, [rsp]
	cmp	dword [rsp+xmltag_empty_ofs+8], 0
	cmove	r13, rax
	mov	rdi, rax
	lea	rsi, [rsp+8]			; the xmltag object
	call	.copyattrsandns			; do this first because our tag name might reference an attributed xmlns
	mov	rdi, [rsp]
	call	.findnamespace
	pop	rdi
	mov	ecx, xmlparser_badqname
	mov	r8d, xmlparser_prefixnotbound
	cmp	rax, -1
	cmove	r14d, ecx
	je	.bailout
	cmp	rax, -2
	cmove	r14d, r8d
	je	.bailout
	mov	rsi, r15
	mov	rdx, rax
	call	xmlmemnode$setqname_nocopy
	jmp	.outer
calign
.element_closing:
	mov	rdi, r15
	mov	esi, 1
	mov	rdx, -1
	call	string$substr
	mov	rdi, r15
	mov	r15, rax
	call	heap$free
	mov	rdi, r13
	call	.findnamespace
	mov	ecx, xmlparser_badqname
	mov	r8d, xmlparser_prefixnotbound
	cmp	rax, -1
	cmove	r14d, ecx
	je	.bailout
	cmp	rax, -2
	cmove	r14d, r8d
	je	.bailout
	mov	ecx, xmlparser_unterminatedelement
	cmp	rax, [r13+xmlmemnode_ns_ofs]
	cmovne	r14d, ecx
	jne	.bailout
	mov	rdi, [r13+xmlmemnode_name_ofs]
	mov	rsi, r15
	call	string$equals
	mov	ecx, xmlparser_unterminatedelement
	test	eax, eax
	cmovz	r14d, ecx
	jz	.bailout
	; otherwise, if r13 != r12, set r13 = its parent
	mov	rax, [r13+xmlmemnode_parent_ofs]
	cmp	r13, r12
	cmovne	r13, rax
	mov	rdi, r15
	call	heap$free
	jmp	.outer
calign
.textnode:
	mov	edi, xmlmemnode_type_text
.textnode_doit:
	xor	esi, esi
	call	xmlmemnode$new_type
	test	r12, r12
	cmovz	r12, rax
	mov	r15, rax
	; get the tag's text:
	mov	rdi, rsp
	call	xmltag$text
	mov	rdi, r15
	mov	rsi, rax
	call	xmlmemnode$settextvalue_nocopy
	cmp	r15, r12
	je	.outer
	mov	rdi, r13
	mov	rsi, r15
	call	xmlmemnode$appendchild
	jmp	.outer
calign
.cdata:
	mov	edi, xmlmemnode_type_cdata
	jmp	.textnode_doit
calign
.comment:
	mov	edi, xmlmemnode_type_comment
	jmp	.textnode_doit
calign
.pi:
	mov	rdi, rsp
	call	xmltag$text
	mov	r15, rax
	mov	rdi, rax
	mov	esi, ' '
	call	string$indexof_charcode
	cmp	rax, 0
	jl	.pi_nameonly
	; otherwise, name and value, skipping whitespace from rax forward
	push	rax
	mov	rdi, r15
	xor	esi, esi
	mov	rdx, rax
	call	string$substring
	mov	rsi, [rsp]
	mov	[rsp], rax			; name portion
	mov	rdi, r15
	call	string$skip_whitespace
	mov	rdi, r15
	mov	rsi, rax
	mov	rdx, -1
	call	string$substr
	push	rax
	; so now, [rsp] == value, [rsp+8] == name, we are done with string in r15
	mov	rdi, r15
	call	heap$free
	mov	edi, xmlmemnode_type_pi
	xor	esi, esi
	call	xmlmemnode$new_type
	test	r12, r12
	cmovz	r12, rax
	mov	r15, rax
	mov	rdi, rax
	pop	rsi
	call	xmlmemnode$settextvalue_nocopy
	mov	rdi, r15
	pop	rsi
	xor	edx, edx	; no NS
	call	xmlmemnode$setqname_nocopy
	cmp	r15, r12
	je	.outer
	mov	rdi, r13
	mov	rsi, r15
	call	xmlmemnode$appendchild
	jmp	.outer
calign
.pi_nameonly:
	; r15 == name, value needs to be string$new
	mov	edi, xmlmemnode_type_pi
	xor	esi, esi
	call	xmlmemnode$new_type
	test	r12, r12
	cmovz	r12, rax
	mov	rdi, rax
	mov	rsi, r15
	xor	edx, edx	; no NS
	mov	r15, rax
	call	xmlmemnode$setqname_nocopy
	call	string$new
	mov	rdi, r15
	mov	rsi, rax
	call	xmlmemnode$settextvalue_nocopy
	cmp	r15, r12
	je	.outer
	mov	rdi, r13
	mov	rsi, r15
	call	xmlmemnode$appendchild
	jmp	.outer

calign
.doctype:
	; this one works similarly to the text, cdata, etc
	; but like .xmldecl, we might promote ourselves to a document
	test	r12, r12
	jnz	.doctype_nopromote
	mov	edi, xmlmemnode_type_document
	xor	esi, esi
	call	xmlmemnode$new_type
	mov	r12, rax
	mov	r13, rax
.doctype_nopromote:
	mov	edi, xmlmemnode_type_doctype
	xor	esi, esi
	call	xmlmemnode$new_type
	mov	r15, rax
	; get the tag's text:
	mov	rdi, rsp
	call	xmltag$text
	mov	rdi, r15
	mov	rsi, rax
	call	xmlmemnode$settextvalue_nocopy
	cmp	r15, r12
	je	.outer
	mov	rdi, r13
	mov	rsi, r15
	call	xmlmemnode$appendchild
	jmp	.outer
calign
.xmldecl:
	; xmldecls are not really processing instructions, but we preserve them as such
	; so that we can spit it back out the same way we got it
	; if we don't yet have a "this", promote ourselves to a document
	test	r12, r12
	jnz	.xmldecl_nopromote
	mov	edi, xmlmemnode_type_document
	xor	esi, esi
	call	xmlmemnode$new_type
	mov	r12, rax
	mov	r13, rax
.xmldecl_nopromote:
	mov	edi, xmlmemnode_type_pi
	xor	esi, esi
	call	xmlmemnode$new_type
	mov	r15, rax
	; get the tag's text
	mov	rdi, rsp
	call	xmltag$text
	mov	rdi, r15
	mov	rsi, rax
	call	xmlmemnode$settextvalue_nocopy
	; set its qname
	mov	rdi, r15
	mov	rsi, .xmldeclstr
	xor	edx, edx		; no NS
	call	xmlmemnode$setqname
	mov	rdi, r13
	mov	rsi, r15
	call	xmlmemnode$appendchild
	jmp	.outer
cleartext .xmldeclstr, 'xml'
.bailout:
	cmp	r14d, xmlparser_noerror
	je	.bailout_checkparent
	cmp	r14d, xmlparser_endofdocument
	je	.bailout_checkparent
	; otherwise, an error has occurred... r12 should be the root of all evil, destroy it:
	test	r12, r12
	jz	.bailout_nodestroy
	mov	rdi, r12
	call	xmlmemnode$destroy
.bailout_nodestroy:
	; our return needs to be null, with edx == r14d
	mov	edx, r14d
	xor	eax, eax
	add	rsp, xmltag_size
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.bailout_checkparent:
	cmp	r12, r13
	jne	.bailout_unterminated
	mov	rax, r12
	xor	edx, edx
	add	rsp, xmltag_size
	pop	r15 r14 r13 r12 rbx
	epilog
calign
.bailout_unterminated:
	test	r12, r12
	jz	.bailout_unterminated_nodestroy
	mov	rdi, r12
	call	xmlmemnode$destroy
.bailout_unterminated_nodestroy:
	mov	edx, xmlparser_unterminatedelement
	xor	eax, eax
	add	rsp, xmltag_size
	pop	r15 r14 r13 r12 rbx
	epilog

end if


if used xmlmemnode$destroy | defined include_everything
	; single argument in rdi: an xmlmemnode to destroy
falign
xmlmemnode$destroy:
	prolog	xmlmemnode$destroy
	mov	ecx, [rdi+xmlmemnode_flags_ofs]
	push	rbx
	mov	rbx, rdi
	test	ecx, xmlmemnode_type_attribute or xmlmemnode_type_text or xmlmemnode_type_cdata or xmlmemnode_type_comment or xmlmemnode_type_pi
	jnz	.namevalonly
	test	ecx, xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jnz	.docorfrag
	; element or unknown (we'll check for unset)
	test	ecx, xmlmemnode_flag_name_owner
	jz	.element_notnameowner
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	test	rdi, rdi
	jz	.element_notnameowner
	call	heap$free
.element_notnameowner:
	mov	rdi, [rbx+xmlmemnode_attributes_ofs]
	test	rdi, rdi
	jz	.element_noattributes
	mov	rsi, xmlmemnode$destroy
	mov	rdx, rbx
	call	vector$clear_arg
	mov	rdi, [rbx+xmlmemnode_attributes_ofs]
	call	vector$destroy
.element_noattributes:
	mov	rdi, [rbx+xmlmemnode_namespaces_ofs]
	test	rdi, rdi
	jz	.element_nonamespaces
	mov	rsi, xmlmemns$destroy
	mov	rdx, rbx
	call	vector$clear_arg
	mov	rdi, [rbx+xmlmemnode_namespaces_ofs]
	call	vector$destroy
.element_nonamespaces:
	mov	rdi, [rbx+xmlmemnode_children_ofs]
	test	rdi, rdi
	jz	.element_nochildren
	mov	rsi, xmlmemnode$destroy
	mov	rdx, rbx
	call	vector$clear_arg
	mov	rdi, [rbx+xmlmemnode_children_ofs]
	call	vector$destroy
.element_nochildren:
	; done, dusted, free our xmlmemnode object in rbx and be done
	mov	rdi, rbx
	pop	rbx
	call	heap$free
	epilog
.namevalonly:
	test	ecx, xmlmemnode_flag_name_owner
	jz	.namevalonly_notnameowner
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	test	rdi, rdi
	jz	.namevalonly_notnameowner
	call	heap$free
.namevalonly_notnameowner:
	mov	rdi, [rbx+xmlmemnode_value_ofs]
	test	rdi, rdi
	jz	.namevalonly_novalue
	call	heap$free
.namevalonly_novalue:
	mov	rdi, rbx
	pop	rbx
	call	heap$free
	epilog
.docorfrag:
	; these should not have a name, just the names stringmap
	mov	rdi, [rdi+xmlmemnode_names_ofs]
	mov	rsi, heap$free
	call	stringmap$clear
	mov	rdi, [rbx+xmlmemnode_names_ofs]
	call	heap$free
	mov	rdi, rbx
	pop	rbx
	call	heap$free
	epilog

end if


if used xmlmemnode$setqname | defined include_everything
	; three arguments: rdi == xmlmemnode, rsi == string name, rdx == xmlmemns pointer (may be zero)
	; makes a copy of the string in rsi and/or does not assume ownership
	; returns a bool as to whether it was okay to do so (invalid node type check)
falign
xmlmemnode$setqname:
	prolog	xmlmemnode$setqname
	mov	ecx, [rdi+xmlmemnode_flags_ofs]
	xor	eax, eax
	test	ecx, xmlmemnode_type_pi
	jnz	.copy
	test	ecx, xmlmemnode_type_element or xmlmemnode_type_attribute
	jz	.ret
	; otherwise, it is an attribute or element node... and these must all be "rooted (says I from Australia)"
	; under a document or document_fragment node, and that contains an xmlmemnode_names_ofs stringmap
	mov	[rdi+xmlmemnode_ns_ofs], rdx
	mov	rcx, rdi
calign
.findroot:
	mov	rax, [rcx+xmlmemnode_parent_ofs]
	test	rax, rax
	cmovnz	rcx, rax
	jnz	.findroot
	; so now, rcx is the topmost node, which must be a document or document_fragment in order for us to cache it
	test	dword [rcx+xmlmemnode_flags_ofs], xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.copy
	; otherwise, it is held in a doc/docfrag, so find our string in rsi in the map, or make a new entry
	mov	rcx, [rcx+xmlmemnode_names_ofs]	; the stringmap we are interested in
	push	rdi rsi rcx
	mov	rdi, rcx
	call	stringmap$find
	test	rax, rax
	jz	.newmapentry
	; otherwise, there is already a matching string:
	mov	rax, [rax+_avlofs_key]		; grab the key string from the stringmap
	pop	rcx rsi rdi
	mov	[rdi+xmlmemnode_name_ofs], rax
	mov	eax, 1
	epilog
.newmapentry:
	mov	rdi, [rsp+8]
	call	string$copy
	mov	[rsp+8], rax
	mov	rdi, [rsp]
	mov	rsi, rax
	xor	edx, edx
	call	stringmap$insert_unique
	pop	rcx rsi rdi
	mov	[rdi+xmlmemnode_name_ofs], rsi
	mov	eax, 1
	epilog
.copy:
	or	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_flag_name_owner
	mov	[rdi+xmlmemnode_ns_ofs], rdx
	push	rdi
	cmp	qword [rdi+xmlmemnode_name_ofs], 0
	jne	.oldone
	mov	rdi, rsi
	call	string$copy
	pop	rdi
	mov	[rdi+xmlmemnode_name_ofs], rax
	mov	eax, 1
.ret:
	epilog
.oldone:
	push	rsi
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_flag_name_owner
	jz	.oldone_nofree
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	call	heap$free
.oldone_nofree:
	pop	rdi
	call	string$copy
	pop	rdi
	mov	[rdi+xmlmemnode_name_ofs], rax
	mov	eax, 1
	epilog

end if


if used xmlmemnode$setqname_nocopy | defined include_everything
	; three arguments: rdi == xmlmemnode, rsi == string name, rdx == xmlmemns pointer (may be zero)
	; assumes ownership of the string in rsi
	; returns a bool as to whether it was okay to do so (invalid node type check)
falign
xmlmemnode$setqname_nocopy:
	prolog	xmlmemnode$setqname_nocopy
	mov	ecx, [rdi+xmlmemnode_flags_ofs]
	xor	eax, eax
	test	ecx, xmlmemnode_type_pi
	jnz	.owned
	test	ecx, xmlmemnode_type_element or xmlmemnode_type_attribute
	jz	.ret
	; otherwise, it is an attribute or element node, and these must all be "rooted (says I from Australia)"
	; under a document or document_fragment node, and that contains an xmlmemnode_names_ofs stringmap
	mov	[rdi+xmlmemnode_ns_ofs], rdx
	mov	rcx, rdi
calign
.findroot:
	mov	rax, [rcx+xmlmemnode_parent_ofs]
	test	rax, rax
	cmovnz	rcx, rax
	jnz	.findroot
	; so now, rcx is the topmost node, which must be a document or document_fragment in order for us to cache it
	test	dword [rcx+xmlmemnode_flags_ofs], xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.owned
	; otherwise, it is held in a doc/docfrag, so find our string in rsi in the map, or make a new entry
	mov	rcx, [rcx+xmlmemnode_names_ofs]	; the stringmap we are interested in
	push	rdi rsi rcx
	mov	rdi, rcx
	call	stringmap$find
	test	rax, rax
	jz	.newmapentry
	; otherwise, there is already a matching string:
	mov	rax, [rax+_avlofs_key]		; grab the key string from the stringmap
	pop	rcx rsi rdi
	mov	[rdi+xmlmemnode_name_ofs], rax
	; free our string since we are to assume ownership fo it
	mov	rdi, rsi
	call	heap$free
	mov	eax, 1
	epilog
.newmapentry:
	mov	rdi, [rsp]
	mov	rsi, [rsp+8]
	xor	edx, edx
	call	stringmap$insert_unique
	pop	rcx rsi rdi
	mov	[rdi+xmlmemnode_name_ofs], rsi
	mov	eax, 1
	epilog
.owned:
	or	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_flag_name_owner
	mov	[rdi+xmlmemnode_ns_ofs], rdx
	cmp	qword [rdi+xmlmemnode_name_ofs], 0
	jne	.oldone
	mov	[rdi+xmlmemnode_name_ofs], rsi
	mov	eax, 1
.ret:
	epilog
.oldone:
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_flag_name_owner
	jz	.oldone_nofree
	push	rdi rsi
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	call	heap$free
	pop	rsi rdi
.oldone_nofree:
	mov	[rdi+xmlmemnode_name_ofs], rsi
	mov	eax, 1
	epilog

end if


if used xmlmemnode$settextvalue | defined include_everything
	; two arguments: rdi == xmlmemnode, rsi == string text value
	; returns a bool as to whether it was okay to do so (invalid node type check)
	; makes a copy of the string in rsi
falign
xmlmemnode$settextvalue:
	prolog	xmlmemnode$settextvalue
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_text or xmlmemnode_type_attribute or xmlmemnode_type_cdata or xmlmemnode_type_comment or xmlmemnode_type_pi or xmlmemnode_type_doctype
	jz	.ret
	push	rdi
	cmp	qword [rdi+xmlmemnode_value_ofs], 0
	jne	.oldone
	mov	rdi, rsi
	call	string$copy
	pop	rdi
	mov	[rdi+xmlmemnode_value_ofs], rax
	mov	eax, 1
.ret:
	epilog
.oldone:
	push	rsi
	mov	rdi, [rdi+xmlmemnode_value_ofs]
	call	heap$free
	pop	rdi
	call	string$copy
	pop	rdi
	mov	[rdi+xmlmemnode_value_ofs], rax
	mov	eax, 1
	epilog

end if


if used xmlmemnode$settextvalue_nocopy | defined include_everything
	; two arguments: rdi == xmlmemnode, rsi == string text value
	; returns a bool as to whether it was okay to do so (invalid node type check)
	; assumes ownership of the string passed (unlike the non _nocopy version which copies it)
falign
xmlmemnode$settextvalue_nocopy:
	prolog	xmlmemnode$settextvalue_nocopy
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_text or xmlmemnode_type_attribute or xmlmemnode_type_cdata or xmlmemnode_type_comment or xmlmemnode_type_pi or xmlmemnode_type_doctype
	jz	.ret
	cmp	qword [rdi+xmlmemnode_value_ofs], 0
	jne	.oldone
	mov	[rdi+xmlmemnode_value_ofs], rsi
	mov	eax, 1
.ret:
	epilog
.oldone:
	push	rdi rsi
	mov	rdi, [rdi+xmlmemnode_value_ofs]
	call	heap$free
	pop	rsi rdi
	mov	[rdi+xmlmemnode_value_ofs], rsi
	mov	eax, 1
	epilog

end if


if used xmlmemnode$appendchild | defined include_everything
	; two arguments: rdi == xmlmemnode, rsi == child xmlmemnode
	; returns bool as to whether it was okay to do so (invalid node type check)
	; also sets parent of the child to rdi
falign
xmlmemnode$appendchild:
	prolog	xmlmemnode$appendchild
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.ret
	mov	[rsi+xmlmemnode_parent_ofs], rdi
	cmp	qword [rdi+xmlmemnode_children_ofs], 0
	je	.newkids
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	call	vector$push_back
	mov	eax, 1
.ret:
	epilog
.newkids:
	push	rdi
	mov	rdi, rsi
	call	vector$new_fromunsigned
	pop	rdi
	mov	[rdi+xmlmemnode_children_ofs], rax
	mov	eax, 1
	epilog

end if


if used xmlmemnode$tostring | defined include_everything
	; single argument in rdi: xmlmemnode
	; returns heap$alloc'd new string
falign
xmlmemnode$tostring:
	prolog	xmlmemnode$tostring
	push	rbx rdi
	call	buffer$new
	pop	rdi
	mov	rbx, rax
	xor	esi, esi
	call	.descend
	mov	rdi, [rbx+buffer_itself_ofs]
	mov	rsi, [rbx+buffer_length_ofs]
if string_bits = 32
	call	string$from_utf32
else
	call	string$from_utf16
end if
	mov	rdi, rbx
	mov	rbx, rax
	call	buffer$destroy
	mov	rax, rbx
	pop	rbx
	epilog
falign
.descend:
	; on entry: rdi is the node we are sorting, esi == indent level
	push	r12 r13
	mov	r12, rdi
	mov	r13d, esi
	mov	ecx, [rdi+xmlmemnode_flags_ofs]
	test	esi, esi
	jz	.noindent
	test	ecx, xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jnz	.noindent
	push	r14
	mov	r14d, r13d
calign
.indent:
	mov	rdi, rbx
	mov	esi, ' '
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	sub	r14d, 1
	jnz	.indent
	pop	r14
.noindent:
	mov	ecx, [r12+xmlmemnode_flags_ofs]
	test	ecx, xmlmemnode_type_text or xmlmemnode_type_attribute
	jnz	.textnode
	test	ecx, xmlmemnode_type_cdata
	jnz	.cdata
if defined xmlmemnode_attributes_separate
	test	ecx, xmlmemnode_type_attribute
	jnz	.attr
end if
	test	ecx, xmlmemnode_type_comment
	jnz	.comment
	test	ecx, xmlmemnode_type_doctype
	jnz	.doctype
	test	ecx, xmlmemnode_type_pi
	jnz	.pi
	test	ecx, xmlmemnode_type_element
	jz	.notelement

	mov	rdi, rbx
	mov	esi, '<'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	cmp	qword [r12+xmlmemnode_ns_ofs], 0
	je	.element_no_namespace
	; otherwise, add its prefix
	mov	rdi, rbx
	mov	rsi, [r12+xmlmemnode_ns_ofs]
	mov	rsi, [rsi+xmlmemns_prefix_ofs]
	cmp	qword [rsi], 0
	je	.element_no_namespace
	call	buffer$append_rawstring
	mov	rdi, rbx
	mov	esi, ':'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
.element_no_namespace:
	mov	rdi, rbx
	mov	rsi, [r12+xmlmemnode_name_ofs]
	call	buffer$append_rawstring
	; iterate through attributes if any
	push	r14 r15
	mov	r14, [r12+xmlmemnode_attributes_ofs]
	xor	r15d, r15d
	test	r14, r14
	jz	.element_no_attributes
	cmp	qword [r14+vector_count_ofs], 0
	je	.element_no_attributes
calign
.element_attribute_loop:
	mov	rdi, rbx
	mov	esi, ' '
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	mov	rdi, r14
	mov	esi, r15d
	call	vector$at
	push	rax		; attribute node
	; see if it has a namespace
	cmp	qword [rax+xmlmemnode_ns_ofs], 0
	je	.element_attribute_no_namespace
	mov	rdi, rbx
	mov	rsi, [rax+xmlmemnode_ns_ofs]
	mov	rsi, [rsi+xmlmemns_prefix_ofs]
	cmp	qword [rsi], 0
	je	.element_attribute_no_namespace
	call	buffer$append_rawstring
	mov	rdi, rbx
	mov	esi, ':'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
.element_attribute_no_namespace:
	mov	rax, [rsp]
	mov	rdi, rbx
	mov	rsi, [rax+xmlmemnode_name_ofs]
	call	buffer$append_rawstring
	mov	rdi, rbx
	mov	rsi, .equalquote
	call	buffer$append_rawstring
	; escape its value if any
	pop	rax
	cmp	qword [rax+xmlmemnode_value_ofs], 0
	je	.element_attribute_no_value
	mov	rdi, [rax+xmlmemnode_value_ofs]
	call	xmltag$escape_string
	push	rax
	mov	rdi, rbx
	mov	rsi, rax
	call	buffer$append_rawstring
	pop	rdi
	call	heap$free
.element_attribute_no_value:
	mov	rdi, rbx
	mov	esi, '"'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	; done with this attribute
	add	r15d, 1
	cmp	r15d, [r14+vector_count_ofs]
	jb	.element_attribute_loop
	; fall through to no attributes

.element_no_attributes:
	; do the same again for namespaces if any
	mov	r14, [r12+xmlmemnode_namespaces_ofs]
	xor	r15d, r15d
	test	r14, r14
	jz	.element_no_namespaces
	cmp	qword [r14+vector_count_ofs], 0
	je	.element_no_namespaces
calign
.element_namespace_loop:
	mov	rdi, rbx
	mov	rsi, .spacexmlns
	call	buffer$append_rawstring
	mov	rdi, r14
	mov	esi, r15d
	call	vector$at
	push	rax
	mov	rsi, [rax+xmlmemns_prefix_ofs]
	cmp	qword [rsi], 0
	je	.element_namespace_noprefix
	mov	rdi, rbx
	mov	esi, ':'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	mov	rax, [rsp]
	mov	rdi, rbx
	mov	rsi, [rax+xmlmemns_prefix_ofs]
	call	buffer$append_rawstring
.element_namespace_noprefix:
	mov	rdi, rbx
	mov	rsi, .equalquote
	call	buffer$append_rawstring
	pop	rax
	mov	rdi, rbx
	mov	rsi, [rax+xmlmemns_uri_ofs]
	call	buffer$append_rawstring
	mov	rdi, rbx
	mov	esi, '"'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	add	r15d, 1
	cmp	r15d, [r14+vector_count_ofs]
	jb	.element_namespace_loop

.element_no_namespaces:
	; if we have no children, spew /> and return
	pop	r15 r14
	mov	rdi, [r12+xmlmemnode_children_ofs]
	test	rdi, rdi
	jz	.element_no_children
	cmp	qword [rdi+vector_count_ofs], 0
	je	.element_no_children
	mov	rdi, rbx
	mov	esi, '>'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	mov	rdi, [r12+xmlmemnode_children_ofs]
	jmp	.with_children
cleartext .spacexmlns, ' xmlns'
calign
.notelement:
	mov	rdi, [r12+xmlmemnode_children_ofs]
	test	rdi, rdi
	jz	.notelement_ret
	cmp	qword [rdi+vector_count_ofs], 0
	je	.notelement_ret
.with_children:
	xor	esi, esi
	call	vector$at
	; rax == first child
	; so here, r12 is our node, r13d is our indent level, rbx is the buffer we are adding to
	push	r13 r14 r15
	mov	rdi, [r12+xmlmemnode_children_ofs]
	xor	r14d, r14d		; our child index
	xor	edx, edx
	lea	esi, [r13d+2]
	mov	r15d, 1			; bool as to whether we should indent children or not

	; if we have an only child that is text or cdata, clear the indent children bool
	cmp	qword [rdi+vector_count_ofs], 1
	ja	.with_children_setindentlevel
	test	dword [rax+xmlmemnode_flags_ofs], xmlmemnode_type_text or xmlmemnode_type_cdata
	cmovnz	r15d, edx
.with_children_setindentlevel:
	; if r15d nonzero and our node is _not_ a document, set r13d = +2
	test	r15d, r15d
	cmovz	r13d, edx		; if !indent children, set next indent level to 0
	jz	.childloop
	test	dword [r12+xmlmemnode_flags_ofs], xmlmemnode_type_document or xmlmemnode_type_document_fragment
	cmovz	r13d, esi
calign
.childloop:
	mov	rdi, [r12+xmlmemnode_children_ofs]
	mov	esi, r14d
	call	vector$at
	test	dword [r12+xmlmemnode_flags_ofs], xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.childloop_notdocument
	test	r15d, r15d
	jz	.childloop_descend
	test	r14d, r14d
	jz	.childloop_descend
	mov	rdi, rbx
	mov	esi, 10
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	jmp	.childloop_descend
calign
.childloop_notdocument:
	test	r15d, r15d
	jz	.childloop_descend
	mov	rdi, rbx
	mov	esi, 10
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
calign
.childloop_descend:
	mov	rdi, [r12+xmlmemnode_children_ofs]
	mov	esi, r14d
	call	vector$at
	mov	rdi, rax
	mov	esi, r13d
	call	.descend
	mov	rdi, [r12+xmlmemnode_children_ofs]
	add	r14d, 1
	cmp	r14d, [rdi+vector_count_ofs]
	jb	.childloop
	; restore the value of the original indentlevel for r13d
	mov	r13, [rsp+16]

	test	r15d, r15d
	jz	.childloop_done_noindent
	mov	rdi, rbx
	mov	esi, 10
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	test	r13d, r13d
	jz	.childloop_done_noindent
calign
.childloop_indent:
	mov	rdi, rbx
	mov	esi, ' '
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	sub	r13d, 1
	jnz	.childloop_indent
calign
.childloop_done_noindent:
	pop	r15 r14 r13
	test	dword [r12+xmlmemnode_flags_ofs], xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jnz	.childloop_done_notelement
	; otherwise, </tag>
	mov	rdi, rbx
	mov	rsi, .closetag
	call	buffer$append_rawstring
	cmp	qword [r12+xmlmemnode_ns_ofs], 0
	je	.childloop_done_element_nons
	mov	rdi, [r12+xmlmemnode_ns_ofs]
	mov	rsi, [rdi+xmlmemns_prefix_ofs]
	cmp	qword [rsi], 0
	je	.childloop_done_element_nons
	mov	rdi, rbx
	call	buffer$append_rawstring
	mov	rdi, rbx
	mov	esi, ':'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
.childloop_done_element_nons:
	mov	rdi, rbx
	mov	rsi, [r12+xmlmemnode_name_ofs]
	call	buffer$append_rawstring
	mov	rdi, rbx
	mov	esi, '>'
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
.childloop_done_notelement:
	pop	r13 r12
	ret
calign
.element_no_children:
	mov	rdi, rbx
	mov	rsi, .emptyclose
	call	buffer$append_rawstring
	pop	r13 r12
	ret
cleartext .closetag, '</'
cleartext .emptyclose, '/>'
calign
.notelement_ret:
	ret
cleartext .equalquote, '="'
.textnode:
	; escape the value and add it straight in
	cmp	qword [r12+xmlmemnode_value_ofs], 0
	je	.textnode_ret
	mov	rdi, [r12+xmlmemnode_value_ofs]
	call	xmltag$escape_string
	push	rax
	mov	rdi, rbx
	mov	rsi, rax
	call	buffer$append_rawstring
	pop	rdi
	call	heap$free
.textnode_ret:
	pop	r13 r12
	ret
.cdata:
	mov	rdi, rbx
	mov	rsi, .cdatahead
	call	buffer$append_rawstring
	cmp	qword [r12+xmlmemnode_value_ofs], 0
	je	.cdata_novalue
	mov	rdi, rbx
	mov	rsi, [r12+xmlmemnode_value_ofs]
	call	buffer$append_rawstring
.cdata_novalue:
	mov	rdi, rbx
	mov	rsi, .cdatatail
	call	buffer$append_rawstring
	pop	r13 r12
	ret
cleartext .cdatahead, '<![CDATA['
cleartext .cdatatail, ']]>'
.comment:
	mov	rdi, rbx
	mov	rsi, .commenthead
	call	buffer$append_rawstring
	cmp	qword [r12+xmlmemnode_value_ofs], 0
	je	.comment_novalue
	mov	rdi, rbx
	mov	rsi, [r12+xmlmemnode_value_ofs]
	call	buffer$append_rawstring
.comment_novalue:
	mov	rdi, rbx
	mov	rsi, .commenttail
	call	buffer$append_rawstring
	pop	r13 r12
	ret
cleartext .commenthead, '<!--'
cleartext .commenttail, '-->'
.doctype:
	mov	rdi, rbx
	mov	rsi, .doctypehead
	call	buffer$append_rawstring
	cmp	qword [r12+xmlmemnode_value_ofs], 0
	je	.doctype_novalue
	mov	rdi, rbx
	mov	rsi, [r12+xmlmemnode_value_ofs]
	call	buffer$append_rawstring
.doctype_novalue:
	mov	rdi, rbx
	mov	rsi, .doctypetail
	call	buffer$append_rawstring
	pop	r13 r12
	ret
cleartext .doctypehead, '<!DOCTYPE '
cleartext .doctypetail, '>'
.pi:
	mov	rdi, rbx
	mov	rsi, .pihead
	call	buffer$append_rawstring
	mov	rdi, rbx
	mov	rsi, [r12+xmlmemnode_name_ofs]
	call	buffer$append_rawstring
	cmp	qword [r12+xmlmemnode_value_ofs], 0
	je	.pi_novalue
	mov	rdi, rbx
	mov	esi, ' '
if string_bits = 32
	call	buffer$append_dword
else
	call	buffer$append_word
end if
	mov	rdi, rbx
	mov	rsi, [r12+xmlmemnode_value_ofs]
	call	buffer$append_rawstring
.pi_novalue:
	mov	rdi, rbx
	mov	rsi, .pitail
	call	buffer$append_rawstring
	pop	r13 r12
	ret
cleartext .pihead, '<?'
cleartext .pitail, '?>'

end if


if used xmlmemnode$foreach_processinginstruction | defined include_everything
	; two arguments: rdi == xmlmemnode object, rsi == function to call
	; function in rsi will get called with rdi == xmlmemnode for every xmlmemnode_type_pi node
falign
xmlmemnode$foreach_processinginstruction:
	prolog	xmlmemnode$foreach_processinginstruction
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.ret
	; otherwise, search by recursive descent
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	mov	rdx, rsi
	mov	rsi, .descend
	test	rdi, rdi
	jz	.ret
	call	vector$foreach_arg
.ret:
	epilog
falign
.descend:
	; rdi == xmlmemnode (child), rsi == function to call
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_pi
	jnz	.descend_call
	; otherwise, if it is not an element or document fragment do nothing
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.descend_nothingtodo
	; otherwise, descend
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	mov	rdx, rsi
	mov	rsi, .descend
	test	rdi, rdi
	jz	.descend_nothingtodo
	call	vector$foreach_arg
	; done, return
.descend_nothingtodo:
	ret
calign
.descend_call:
	call	rsi
	; we know that our node that was in rdi was a PI and thus has no children
	; return:
	ret

end if


if used xmlmemnode$foreach_element | defined include_everything
	; two arguments: rdi == xmlmemnode object, rsi == function to call
	; function in rsi will get called with rdi == xmlmemnode for every xmlmemnode_type_element node
falign
xmlmemnode$foreach_element:
	prolog	xmlmemnode$foreach_element
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.ret
	; otherwise, search by recursive descent
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	mov	rdx, rsi
	mov	rsi, .descend
	test	rdi, rdi
	jz	.ret
	call	vector$foreach_arg
.ret:
	epilog
falign
.descend:
	; rdi == xmlmemnode (child), rsi == function to call
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jnz	.descend_call
	; otherwise, if it is not an element or document fragment do nothing
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.descend_nothingtodo
	; otherwise, descend
.descend_deeper:
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	mov	rdx, rsi
	mov	rsi, .descend
	test	rdi, rdi
	jz	.descend_nothingtodo
	call	vector$foreach_arg
	; done, return
.descend_nothingtodo:
	ret
calign
.descend_call:
	push	rdi rsi
	call	rsi
	pop	rsi rdi
	jmp	.descend_deeper

end if


if used xmlmemnode$foreach_bytagname | defined include_everything
	; three arguments: rdi == xmlmemnode object, rsi == function to call, rdx == tagname
	; function in rsi will get called with rdi == xmlmemnode for every element whose tagname matches
	; NOTE: this function ignores namespaces, see foreach_byqname for ns matching
falign
xmlmemnode$foreach_bytagname:
	prolog	xmlmemnode$foreach_bytagname
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.ret
	; otherwise, search by recursive descent
	; NOTE: we cheat here and use a register that doesn't get hammered by vector$foreach_arg:
	; r15: (because we need to additionally store our tagname)
	push	r15
	mov	r15, rdx
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	mov	rdx, rsi
	mov	rsi, .descend
	test	rdi, rdi
	jz	.skip
	call	vector$foreach_arg
.skip:
	pop	r15
.ret:
	epilog
falign
.descend:
	; rdi == xmlmemnode (child), rsi == function to call, r15 is our tagname to compare
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jnz	.descend_doit
	; otherwise, if it is not an element or document fragment, do nothing
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.descend_nothingtodo
	; otherwise, descend:
.descend_deeper:
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	mov	rdx, rsi
	mov	rsi, .descend
	test	rdi, rdi
	jz	.descend_nothingtodo
	call	vector$foreach_arg
	; done, return
.descend_nothingtodo:
	ret
calign
.descend_doit:
	; function in rsi to be called only if the tagname matches that in r15
	cmp	qword [rdi+xmlmemnode_name_ofs], 0
	je	.descend_nothingtodo
	push	rdi rsi
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	mov	rsi, r15
	call	string$equals
	test	eax, eax
	jz	.descend_doit_nocall
	mov	rdi, [rsp+8]
	mov	rsi, [rsp]
	call	rsi
.descend_doit_nocall:
	pop	rsi rdi
	; go ahead and descend:
	jmp	.descend_deeper


end if


if used xmlmemnode$foreach_byqname | defined include_everything
	; four arguments: rdi == xmlmemnode object, rsi == function to call, rdx == tagname, rcx == namespace URI
	; function in rsi will get called with rdi == xmlmemnode for every element whose tagname and namespace matches
	; NOTE: namespace URI can be null, otherwise we do full namespace matching
falign
xmlmemnode$foreach_byqname:
	prolog	xmlmemnode$foreach_byqname
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.ret
	; otherwise, search by recursive descent
	; NOTE: we cheat here and use a register that doesn't get hammered by vector$foreach_arg
	; r15: (because we need to additionally store our tagname and namespace)
	push	r15
	sub	rsp, 16
	mov	r15, rsp
	mov	[rsp], rdx
	mov	[rsp+8], rcx
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	mov	rdx, rsi
	mov	rsi, .descend
	test	rdi, rdi
	jz	.skip
	call	vector$foreach_arg
.skip:
	add	rsp, 16
	pop	r15
.ret:
	epilog
falign
.descend:
	; rdi == xmlmemnode (child), rsi == function to call, r15 == pointer to rsp with our tagname and namespace URI
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jnz	.descend_doit
	; otherwise, if it is not an element or document fragment, do nothing
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.descend_nothingtodo
	; otherwise, descend
.descend_deeper:
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	mov	rdx, rsi
	mov	rsi, .descend
	test	rdi, rdi
	jz	.descend_nothingtodo
	call	vector$foreach_arg
	; done, return
.descend_nothingtodo:
	ret
calign
.descend_doit:
	; function in rsi to be called only if the tagname matches and the NS URI matches
	cmp	qword [rdi+xmlmemnode_name_ofs], 0
	je	.descend_nothingtodo
	cmp	qword [rdi+xmlmemnode_ns_ofs], 0
	je	.descend_nothingtodo
	push	rdi rsi
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	mov	rsi, [r15]
	call	string$equals
	test	eax, eax
	jz	.descend_doit_nocall
	mov	rdi, [rsp+8]
	mov	rsi, [r15+8]
	mov	rdi, [rdi+xmlmemnode_ns_ofs]
	mov	rdi, [rdi+xmlmemns_uri_ofs]
	call	string$equals
	test	eax, eax
	jz	.descend_doit_nocall
	mov	rdi, [rsp+8]
	mov	rsi, [rsp]
	call	rsi
.descend_doit_nocall:
	pop	rsi rdi
	; go ahead and descend:
	jmp	.descend_deeper

end if


if used xmlmemnode$element_byid | defined include_everything
	; two arguments: rdi == xmlmemnode object, rsi == "id" attribute to match
	; returns an xmlmemnode_type_element node in rax or null if not found
	; (returns the first one that matches if duplicate ids exist)
	; NOTE: this searches for elements with an unbound namespace like:
	; <elem id="xxx" /> where "xxx" is passed to this function
	; (this does deep searching below the node passed in rdi)
falign
xmlmemnode$element_byid:
	prolog	xmlmemnode$element_byid
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.ret
	cmp	qword [rdi+xmlmemnode_children_ofs], 0
	je	.ret
	call	.descend_and_check
.ret:
	epilog
cleartext .idstr, 'id'
falign
.descend_and_check:
	; rdi == xmlmemnode, rsi == attribute name
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element or xmlmemnode_type_document or xmlmemnode_type_document_fragment
	jz	.skipit
	push	rdi rsi
	mov	rsi, .idstr
	call	xmlmemnode$attribute_byname
	pop	rsi rdi
	test	rax, rax
	jnz	.check_attribute_value
.process_children:
	; otherwise, for each one of our children, descend
	cmp	qword [rdi+xmlmemnode_children_ofs], 0
	je	.skipit
	push	rbx r12
	mov	rbx, [rdi+xmlmemnode_children_ofs]
	mov	r12, [rbx+vector_count_ofs]
	mov	rbx, [rbx+vector_array_ofs]
	test	r12, r12
	jz	.norecurse
calign
.loop:
	mov	rdi, [rbx]
	add	rbx, 8
	; rsi still valid and pointing to our id search value
	call	.descend_and_check
	test	rax, rax
	jnz	.norecurse
	sub	r12, 1
	jnz	.loop
.norecurse:
	pop	r12 rbx
.skipit:
	ret
.check_attribute_value:
	; so xmlmemnode$attribute_byname returned us an xmlmemnode_type_attribute for the id
	; check its value for a match in rsi
	push	rdi rsi
	mov	rdi, [rax+xmlmemnode_value_ofs]
	call	string$equals
	pop	rsi rdi
	test	eax, eax
	jz	.process_children
	mov	rax, rdi
	ret

end if


if used xmlmemnode$attribute_byname | defined include_everything
	; two arguments: rdi == xmlmemnode object, rsi == attribute name
	; returns an xmlmemnode_type_attribute node in rax or null if not found
	; NOTE: this function ignores namespaces, see attribute_byqname for ns matching
falign
xmlmemnode$attribute_byname:
	prolog	xmlmemnode$attribute_byname
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jz	.ret
	mov	rcx, [rdi+xmlmemnode_attributes_ofs]
	test	rcx, rcx
	jz	.ret
	cmp	qword [rcx+vector_count_ofs], 0
	je	.ret
	; otherwise, we can walk them:
	push	rbx r12 r13
	mov	rbx, [rcx+vector_array_ofs]
	mov	r12, [rcx+vector_count_ofs]
	mov	r13, rsi
calign
.search:
	mov	rdi, [rbx]
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	mov	rsi, r13
	call	string$equals
	test	eax, eax
	jnz	.found
	add	rbx, 8
	sub	r12d, 1
	jnz	.search
	xor	eax, eax
	pop	r13 r12 rbx
.ret:
	epilog
calign
.found:
	mov	rax, [rbx]
	pop	r13 r12 rbx
	epilog

end if


if used xmlmemnode$attribute_byqname | defined include_everything
	; three arguments: rdi == xmlmemnode object, rsi == attribute name, rdx == namespace URI
	; returns an xmlmemnode_type_attribute node in rax or null if not found
	; NOTE: namespace URI can be null, otherwise we do full namespace matching
falign
xmlmemnode$attribute_byqname:
	prolog	xmlmemnode$attribute_byqname
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jz	.ret
	mov	rcx, [rdi+xmlmemnode_attributes_ofs]
	test	rcx, rcx
	jz	.ret
	cmp	qword [rcx+vector_count_ofs], 0
	je	.ret
	; otherwise, we can walk them:
	push	rbx r12 r13 r14
	mov	rbx, [rcx+vector_array_ofs]
	mov	r12, [rcx+vector_count_ofs]
	mov	r13, rsi
	mov	r14, rdx
calign
.search:
	mov	rdi, [rbx]
	cmp	qword [rdi+xmlmemnode_ns_ofs], 0
	je	.search_nocheck
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	mov	rsi, r13
	call	string$equals
	test	eax, eax
	jnz	.maybefound
.search_nocheck:
	add	rbx, 8
	sub	r12d, 1
	jnz	.search
	xor	eax, eax
	pop	r14 r13 r12 rbx
.ret:
	epilog
calign
.maybefound:
	mov	rdi, [rbx]
	mov	rdi, [rdi+xmlmemnode_ns_ofs]
	mov	rsi, r14
	mov	rdi, [rdi+xmlmemns_uri_ofs]
	call	string$equals
	test	eax, eax
	jz	.search_nocheck
	mov	rax, [rbx]
	pop	r14 r13 r12 rbx
	epilog

end if


if used xmlmemnode$property_byname | defined include_everything
	; two arguments: rdi == xmlmemnode object, rsi == property name
	; returns an xmlmemnode_type_text node in rax or null if not found
	; NOTE: this function ignores namespaces, see property_byqname for ns matching
	; this searches child elements for element tag names (the "property" name)
	; and if a child element has an only child and its only child is text, that text node
	; is returned
falign
xmlmemnode$property_byname:
	prolog	xmlmemnode$property_byname
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jz	.ret
	mov	rcx, [rdi+xmlmemnode_children_ofs]
	test	rcx, rcx
	jz	.ret
	cmp	qword [rcx+vector_count_ofs], 0
	je	.ret
	; otherwise, we can walk them:
	push	rbx r12 r13
	mov	rbx, [rcx+vector_array_ofs]
	mov	r12, [rcx+vector_count_ofs]
	mov	r13, rsi
calign
.search:
	mov	rdi, [rbx]
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jz	.search_next
	; see if its name matches the one we are looking for:
	mov	rsi, r13
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	call	string$equals
	test	eax, eax
	jnz	.found
.search_next:
	add	rbx, 8
	sub	r12, 1
	jnz	.search
	xor	eax, eax
	pop	r13 r12 rbx
.ret:
	epilog
calign
.found:
	; [rbx] is a name-matched xmlmemnode_type_element, make sure it has
	; an only child and that its only child is text, or no deal
	mov	rdi, [rbx]
	cmp	qword [rdi+xmlmemnode_children_ofs], 0
	je	.search_next
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	cmp	qword [rdi+vector_count_ofs], 1
	jne	.search_next
	mov	rsi, [rdi+vector_array_ofs]
	mov	rax, [rsi]
	; make sure its type is text:
	test	dword [rax+xmlmemnode_flags_ofs], xmlmemnode_type_text
	jz	.search_next
	; otherwise, all good
	pop	r13 r12 rbx
	epilog

end if


if used xmlmemnode$property_byqname | defined include_everything
	; three arguments: rdi == xmlmemnode object, rsi == property name, rdx == namespace URI
	; returns an xmlmemnode_type_text node in rax or null if not found
	; NOTE: namespace URI can be null, otherwise we do full namespace matching
	; this searches child elements for element tag names (the "property" name)
	; and if a child element has an only child and its only child is text, that text node
	; is returned
falign
xmlmemnode$property_byqname:
	prolog	xmlmemnode$property_byqname
	xor	eax, eax
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jz	.ret
	mov	rcx, [rdi+xmlmemnode_children_ofs]
	test	rcx, rcx
	jz	.ret
	cmp	qword [rcx+vector_count_ofs], 0
	je	.ret
	; otherwise, we can walk them:
	push	rbx r12 r13 r14
	mov	rbx, [rcx+vector_array_ofs]
	mov	r12, [rcx+vector_count_ofs]
	mov	r13, rsi
	mov	r14, rdx
calign
.search:
	mov	rdi, [rbx]
	test	dword [rdi+xmlmemnode_flags_ofs], xmlmemnode_type_element
	jz	.search_next
	; see if its name matches the one we are looking for:
	mov	rsi, r13
	mov	rdi, [rdi+xmlmemnode_name_ofs]
	call	string$equals
	test	eax, eax
	jnz	.found
.search_next:
	add	rbx, 8
	sub	r12, 1
	jnz	.search
	xor	eax, eax
	pop	r14 r13 r12 rbx
.ret:
	epilog
calign
.found:
	; [rbx] is a name-matched xmlmemnode_type_element, make sure it has
	; an only child and that its only child is text, or no deal
	mov	rdi, [rbx]
	mov	rsi, r14
	mov	rdi, [rdi+xmlmemnode_ns_ofs]
	test	rdi, rdi
	jz	.search_next
	mov	rdi, [rdi+xmlmemns_uri_ofs]
	call	string$equals
	test	eax, eax
	jz	.search_next
	mov	rdi, [rbx]
	cmp	qword [rdi+xmlmemnode_children_ofs], 0
	je	.search_next
	mov	rdi, [rdi+xmlmemnode_children_ofs]
	cmp	qword [rdi+vector_count_ofs], 1
	jne	.search_next
	mov	rsi, [rdi+vector_array_ofs]
	mov	rax, [rsi]
	; make sure its type is text:
	test	dword [rax+xmlmemnode_flags_ofs], xmlmemnode_type_text
	jz	.search_next
	; otherwise, all good
	pop	r14 r13 r12 rbx
	epilog

end if
