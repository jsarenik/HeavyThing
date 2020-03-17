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
	; roomlist.inc: a tui_panel descendent that deals with displaying listed
	; rooms (and updating the datagrid in realtime)
	;


dalign
roomlist$vtable:
	dq      roomlist$cleanup, tui_panel$clone, tui_panel$draw, tui_object$redraw, tui_object$updatedisplaylist, tui_object$sizechanged
	dq      tui_object$timer, tui_object$layoutchanged, tui_object$move, tui_object$setfocus, tui_object$gotfocus, tui_object$lostfocus
	dq      tui_object$keyevent, tui_object$domodal, tui_object$endmodal, tui_object$exit, tui_object$calcbounds, tui_object$calcchildbounds
	dq      tui_panel$appendchild, tui_object$appendbastard, tui_panel$prependchild, tui_panel$contains, tui_panel$getchildindex
	dq      tui_panel$removechild, tui_object$removebastard, tui_object$removeallchildren, tui_object$removeallbastards
	dq      tui_object$getobjectsunderpoint, tui_object$flatten, tui_object$firekeyevent, tui_object$ontab, tui_object$onshifttab
	dq      tui_object$setcursor, tui_object$showcursor, tui_object$hidecursor, tui_object$click, tui_object$clicked


dalign
roomlistdg$vtable:
        dq      tui_object$cleanup, tui_datagrid$clone, tui_object$draw, tui_object$redraw, tui_object$updatedisplaylist, tui_object$sizechanged
        dq      tui_object$timer, tui_object$layoutchanged, tui_object$move, tui_object$setfocus, tui_object$gotfocus, tui_object$lostfocus
        dq      tui_object$keyevent, tui_object$domodal, tui_object$endmodal, tui_object$exit, tui_object$calcbounds, tui_object$calcchildbounds
        dq      tui_object$appendchild, tui_object$appendbastard, tui_object$prependchild, tui_object$contains, tui_object$getchildindex
        dq      tui_object$removechild, tui_object$removebastard, tui_object$removeallchildren, tui_object$removeallbastards
        dq      tui_object$getobjectsunderpoint, tui_object$flatten, tui_object$firekeyevent, tui_object$ontab, tui_object$onshifttab
        dq      tui_object$setcursor, tui_object$showcursor, tui_object$hidecursor, tui_object$click, tui_object$clicked
        ; our itemselected:
        dq      roomlistdg$roomselected


globals
{
	roomlists	dq	0
	roomlist_json	dq	0
}


	; no arguments, gets called whenever the room list changes (e.g. joins/leaves/new/destroy)
falign
roomlists_update:
	prolog	roomlists_update
	; construct the json representation of our roomlist first
	; construct the JSON we need for tui_datagrid
	push	rbx r12
	call	string$new
	mov	rdi, rax
	call	json$newarray_nocopy
	mov	rbx, rax
	; the JSON array in rbx needs populated with json objects for each public room
	; but first, we want the room list sorted by user count descending
	xor	edi, edi
	call	unsignedmap$new
	mov	r12, rax
	; now iterate through the chatrooms stringmap and insert into that
	mov	rdi, [chatrooms]
	mov	rsi, .chatroom_sort
	mov	rdx, rax
	call	unsignedmap$foreach_arg
	; now, if we reverse iterate through the map in r12, we can construct json objects
	; for each entry
	mov	rdi, r12
	mov	rsi, .tojson
	mov	rdx, rbx
	call	unsignedmap$reverse_foreach_arg
	; now we can clear the map in r12
	mov	rdi, r12
	xor	esi, esi
	call	unsignedmap$clear
	mov	rdi, r12
	call	heap$free

	; so now, we can walk through all roomlists
	mov	rdi, [roomlists]
	test	rdi, rdi
	jz	.noroomlists
	mov	rsi, .setdata
	mov	rdx, rbx
	call	unsignedmap$foreach_arg
.noroomlists:
	mov	rdi, [roomlist_json]
	test	rdi, rdi
	jz	.noprev
	call	json$destroy
.noprev:
	mov	[roomlist_json], rbx
	pop	r12 rbx
.nothingtodo:
	epilog
cleartext .roomstr, 'room'
cleartext .usersstr, 'users'

	; rdi == tui_datagrid object, rsi == undefined, rdx == json array object
falign
.setdata:
	mov	rsi, rdx
	call	tui_datagrid$nvsetdata_notowner
	ret
	
	; rdi == user count (unsigned), rsi == new string name of room, rdx == destination json array
falign
.tojson:
	push	rdi rsi rdx
	; [rsp] == destination json array
	; [rsp+8] == new string name of the room
	; [rsp+16] == user count (unsigned)
	call	string$new
	mov	rdi, rax
	call	json$newobject_nocopy
	push	rax
	; [rsp] == new unnamed json object to hold our goods
	; [rsp+8] == destination json array
	; [rsp+16] == new string name of the room
	; [rsp+24] == user count (unsigned)
	; we need two values added to that object: room, users
	mov	rdi, .roomstr
	mov	rsi, [rsp+16]
	call	json$newvalue
	mov	rdi, [rsp]
	mov	rsi, rax
	call	json$appendchild
	mov	rdi, [rsp+16]
	call	heap$free
	mov	rdi, [rsp+24]
	mov	esi, 10
	call	string$from_unsigned
	mov	[rsp+24], rax
	mov	rdi, .usersstr
	mov	rsi, rax
	call	json$newvalue
	mov	rdi, [rsp]
	mov	rsi, rax
	call	json$appendchild
	mov	rdi, [rsp+24]
	call	heap$free
	mov	rdi, [rsp+8]
	mov	rsi, [rsp]
	call	json$appendchild
	add	rsp, 32
	ret

	; rdi == chatroom name, rsi == chatroom object rsi == rdx == our destination unsigned map
falign
.chatroom_sort:
if string_bits = 32
	cmp	dword [rdi+8], 0x27
else
	cmp	word [rdi+8], 0x27
end if
	je	.chatroom_sort_done
	mov	rsi, [rsi+chatroom_users_ofs]
	mov	rsi, [rsi+_avlofs_right]	; # of users in the room
	push	rsi rdx
	call	string$copy
	pop	rdi rsi
	mov	rdx, rax
	call	unsignedmap$insert
.chatroom_sort_done:
	ret

	; single argument in rdi: the screen object we will belong to
	; returns a new roomlist tui_panel object in rax
falign
roomlist$new:
	prolog	roomlist$new
	push	rdi
	mov	edi, 40
	mov	esi, 15
	mov	rdx, .title
	ansi_colors ecx, 'black', 'cyan'
	ansi_colors r8d, 'black', 'cyan'
	call	tui_panel$new_ii
	push	rax
	mov	qword [rax], roomlist$vtable
	
	; and into that, a single datagrid
	mov	edi, 38
	mov	esi, 13
	ansi_colors edx, 'black', 'lightgray'
	ansi_colors ecx, 'lightgray', 'black'
	ansi_colors r8d, 'lightgray', 'blue'
	call	tui_datagrid$new_ii
	mov	rdi, [rsp+8]
	mov	qword [rax], roomlistdg$vtable
	mov	[rax+tui_dguser_ofs], rdi

	mov	rdi, [rsp]
	mov	rsi, rax
	mov	rdx, [rdi]
	push	rax
	call	qword [rdx+tui_vappendchild]
	
	mov	rdi, [rsp]
	mov	rsi, .roomlabel
	movq	xmm0, [_math_onehundred]
	mov	edx, tui_textalign_left
	mov	rcx, .roomstr
	call	tui_datagrid$nvaddproperty_d
	
	mov	rdi, [rsp]
	mov	rsi, .countlabel
	mov	edx, 5
	mov	ecx, tui_textalign_right
	mov	r8, .countstr
	call	tui_datagrid$nvaddproperty_i

	cmp	qword [roomlists], 0
	jne	.roomlists_okay
	xor	edi, edi
	call	unsignedmap$new
	mov	[roomlists], rax
.roomlists_okay:
	cmp	qword [roomlist_json], 0
	je	.firstrun
	mov	rdi, [rsp]
	mov	rsi, [roomlist_json]
	call	tui_datagrid$nvsetdata_notowner

	mov	rdi, [roomlists]
	pop	rsi
	xor	edx, edx
	call	unsignedmap$insert_unique

	pop	rax rdi
	epilog
.firstrun:
	mov	rdi, [roomlists]
	pop	rsi
	xor	edx, edx
	call	unsignedmap$insert_unique

	call	roomlists_update

	pop	rax rdi
	epilog
	
cleartext .title, 'Realtime Public Room List'
cleartext .roomlabel, 'Room'
cleartext .roomstr, 'room'
cleartext .countlabel, 'Users'
cleartext .countstr, 'users'


; single argument in rdi: the roomlist panel object to cleanup
falign
roomlist$cleanup:
	prolog	roomlist$cleanup
	push	rdi
	mov	rsi, [rdi+tui_panel_guts_ofs]
	mov	rdx, [rsi+tui_children_ofs]
	mov	rsi, [rdx+_list_first_ofs]
	mov	rsi, [rsi]
	mov	rdi, [roomlists]
	call	unsignedmap$erase
	; let tui_panel$cleanup do the rest:
	pop	rdi
	call	tui_panel$cleanup

	epilog


	; rdi == the tui_datagrid object, rsi == the json object that was selected
falign
roomlistdg$roomselected:
	prolog	roomlistdg$roomselected
	push	rbx
	mov	rbx, rdi

	mov	rdi, rsi
	mov	rsi, .roomstr
	call	json$getvaluebyname

	mov	rdi, [rbx+tui_dguser_ofs]	; our screen object
	mov	rsi, [rax+json_value_ofs]
	mov	edx, 1
	xor	ecx, ecx
	call	screen$chatpanel_byname
	; that should have returned us the chatpanel we were after, set input focus to that

	push	rax
	mov	rdi, [rbx+tui_dguser_ofs]
	mov	rdi, [rdi+screen_focus_ofs]
	test	rdi, rdi
	jz	.nofocus
	mov	rsi, [rdi]
	call	qword [rsi+tui_vlostfocus]
.nofocus:
	pop	rax
	mov	rdi, [rbx+tui_dguser_ofs]
	mov	[rdi+screen_focus_ofs], rax

	; and call its getfocus method
	mov	rdi, rax
	mov	rsi, [rax]
	call	qword [rsi+tui_vgotfocus]
	
	; get rid of our modal dialog as well
	mov	rdi, [rbx+tui_dguser_ofs]
	push	rdi
	mov	qword [rdi+screen_modal_ofs], 0
	mov	rsi, [rdi]
	call	qword [rsi+tui_vremoveallbastards]
	pop	rdi
	call	screen$showhidecursor

	pop	rbx
	epilog
cleartext .roomstr, 'room'
