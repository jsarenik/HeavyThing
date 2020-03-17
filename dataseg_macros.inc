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
	; dataseg.inc: globals macro to define a global writeable data segment
	;
	; to use: globals
	; {
	; 	someglobal dq 0
	; }
	; anywhere throughout your code, and it will all end up inside our single datasegment
	;
	; Many thanks to Tomasz Grysztar (author of fasm) for adding the "if used"
	; conditional modifications to this!
	;

.gvar_list equ
.gvar_condition equ

macro globals {
        local   z
        match condition, .gvar_condition \{
                local i,e
                .gvar_list equ .gvar_list,i,z,e
                macro i \\{ if condition \\}
                macro e \\{ end if \\}
        \}
        match , .gvar_condition \{
                .gvar_list equ .gvar_list,z
        \}
        macro   z
}

macro globalVars {
        match var_list,.gvar_list \{
                irp instr,var_list \\{
                        instr
                \\}
        \}
}

macro if condition& {
        .gvar_if equ
        match =used var, condition \{
                .gvar_condition equ condition
                restore .gvar_if
                .gvar_if equ +
        \}
        if condition
}

macro end directive {
        end directive
        match =if, directive \{
                match +, .gvar_if \\{
                        restore .gvar_condition
                \\}
                restore .gvar_if
        \}
}

