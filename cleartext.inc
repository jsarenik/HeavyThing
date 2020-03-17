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
	; cleartext.inc: static string encoding macros
	;
	; v1.22+ update: This now correctly deals with UTF8 input in fasm source
	;    .. Versions prior to this one simply promoted the string (much like
	;    fasm does with a du 'string' declaration) to either 16 bit or 32 bit
	;    characters.
	;
	;    This version validates and correctly interprets UTF8.
	;    NOTE: Requires little-endian byte order (see load ..ch lines below).

macro cleartext name*, [val*] {
common
	local ..str,..sz,..ch,..ci,..cj,..cc,..ign,..dat,..idx,..pad
	virtual at 0
		..str:: db val
		..sz = $
	end virtual
	..cc = 0
	..ign = 0
	; first step: count how many codepoints there are and validate the UTF8
	repeat ..sz
		if ..ign > 0
			..ign = ..ign - 1
		else
			if %+3 <= ..sz
				load ..ch dword from ..str:(%-1)
			else if %+2 <= ..sz
				load ..ch word from ..str:(%-1)
				load ..ci byte from ..str:(%+1)
				..ch = ..ch or (..ci shl 16)
			else if %+1 <= ..sz
				load ..ch word from ..str:(%-1)
			else
				load ..ch byte from ..str:(%-1)
			end if
			..ci = ..ch
			..cj = ..ch
			..ch = (..ch and 0xf0) shr 4
			if ..ch < 8
				; ascii
				..cj = ..ci and 0xff
				; codepoint is in ..cj
				..cc = ..cc + 1
			else if ..ch < 12
				display 'invalid UTF8',10
				err
			else if ..ch < 14
				; w8 or w16
				if %+1 > ..sz
					; not enough bytes left
					display 'invalid UTF8',10
					err
				end if
				..ch = ..ci
				if (..cj shr 8) and 0xc0 <> 0x80
					display 'invalid UTF8',10
					err
				end if
				..cj = ((..ch and 0xff) shl 6) and 0x7c0
				..ch = (..ch shr 8) and 0x3f
				..cj = ..cj or ..ch
				if ..cj < 0x80
					display 'invalid UTF8',10
					err
				end if
				; codepoint is in ..cj
				..cc = ..cc + 1
				..ign = 1
			else if ..ch = 14
				; w16
				if %+2 > ..sz
					; not enough bytes left
					display 'invalid UTF8',10
					err
				end if
				..ch = ..ci
				if (..cj shr 8) and 0xc0 <> 0x80
					display 'invalid UTF8',10
					err
				end if
				if (..cj shr 16) and 0xc0 <> 0x80
					display 'invalid UTF8',10
					err
				end if
				..cj = ((..ch and 0xff) shl 12) and 0xf000
				..ci = (((..ch shr 8) and 0xff) shl 6) and 0xfc0
				..cj = ..cj or ..ci
				..ch = (..ch shr 16) and 0x3f
				..cj = ..cj or ..ch
				if ..cj < 0x800
					display 'invalid UTF8',10
					err
				end if
				; codepoint is in ..cj
				..cc = ..cc + 1
				..ign = 2
			else
				; w32
				if %+3 > ..sz
					; not enough bytes left
					display 'invalid UTF8',10
					err
				end if
				..ch = ..ci
				if ..ch and 0x08 > 0
					display 'invalid UTF8',10
					err
				end if
				if (..ch shr 8) and 0xc0 <> 0x80
					display 'invalid UTF8',10
					err
				end if
				if (..ch shr 16) and 0xc0 <> 0x80
					display 'invalid UTF8',10
					err
				end if
				if (..ch shr 24) and 0xc0 <> 0x80
					display 'invalid UTF8',10
					err
				end if
				..cj = ((..ch and 0xff) shl 18) and 0x1c0000
				..ci = (((..ch shr 8) and 0xff) shl 12) and 0x3f000
				..cj = ..cj or ..ci
				..ci = (((..ch shr 16) and 0xff) shl 6) and 0xfc0
				..cj = ..cj or ..ci
				..ci = (..ch shr 24) and 0x3f
				..cj = ..cj or ..ci
				if ..cj < 0x10000
					display 'invalid UTF8',10
					err
				end if
if string_bits = 32
				..cc = ..cc + 1
else
				..cc = ..cc + 2
end if
				..ign = 3
			end if
		end if
	end repeat
	; our actual string storage
	dalign
	name:
		dq	..cc
	..dat:
if string_bits = 32
		db	(..cc shl 2) dup 0
else
		db	(..cc shl 1) dup 0
end if
	; and repeat again, only without the validation
	..ign = 0
	..idx = 0
	repeat ..sz
		if ..ign > 0
			..ign = ..ign - 1
		else
			if %+3 <= ..sz
				load ..ch dword from ..str:(%-1)
			else if %+2 <= ..sz
				load ..ch word from ..str:(%-1)
				load ..ci byte from ..str:(%+1)
				..ch = ..ch or (..ci shl 16)
			else if %+1 <= ..sz
				load ..ch word from ..str:(%-1)
			else
				load ..ch byte from ..str:(%-1)
			end if
			..ci = ..ch
			..cj = ..ch
			..ch = (..ch and 0xf0) shr 4
			if ..ch < 8
				; ascii
				..cj = ..ci and 0xff
				; codepoint is in ..cj
if string_bits = 32
				store dword ..cj at ..dat+..idx
				..idx = ..idx + 4
else
				store word ..cj at ..dat+..idx
				..idx = ..idx + 2
end if
			else if ..ch < 14
				; w8 or w16
				..ch = ..ci
				..cj = ((..ch and 0xff) shl 6) and 0x7c0
				..ch = (..ch shr 8) and 0x3f
				..cj = ..cj or ..ch
				; codepoint is in ..cj
if string_bits = 32
				store dword ..cj at ..dat+..idx
				..idx = ..idx + 4
else
				store word ..cj at ..dat+..idx
				..idx = ..idx + 2
end if
				..ign = 1
			else if ..ch = 14
				; w16
				..ch = ..ci
				..cj = ((..ch and 0xff) shl 12) and 0xf000
				..ci = (((..ch shr 8) and 0xff) shl 6) and 0xfc0
				..cj = ..cj or ..ci
				..ch = (..ch shr 16) and 0x3f
				..cj = ..cj or ..ch
				; codepoint is in ..cj
if string_bits = 32
				store dword ..cj at ..dat+..idx
				..idx = ..idx + 4
else
				store word ..cj at ..dat+..idx
				..idx = ..idx + 2
end if
				..ign = 2
			else
				; w32
				..ch = ..ci
				..cj = ((..ch and 0xff) shl 18) and 0x1c0000
				..ci = (((..ch shr 8) and 0xff) shl 12) and 0x3f000
				..cj = ..cj or ..ci
				..ci = (((..ch shr 16) and 0xff) shl 6) and 0xfc0
				..cj = ..cj or ..ci
				..ci = (..ch shr 24) and 0x3f
				..cj = ..cj or ..ci
if string_bits = 32
				store dword ..cj at ..dat+..idx
				..idx = ..idx + 4
else
				..ci = (((..cj - 0x10000) shr 10) and 0x3ff) + 0xd800
				; first in ci
				store word ..ci at ..dat+..idx
				..idx = ..idx + 2
				..cj = ((..cj - 0x10000) and 0x3ff) + 0xdc00
				; second in ..cj
				store word ..cj at ..dat+..idx
				..idx = ..idx + 2
end if
				..ign = 3
			end if
		end if
	end repeat
}
