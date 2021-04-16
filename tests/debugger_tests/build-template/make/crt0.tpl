;;;
;;; Copyright 2006, 2007, 2008, 2009 by Brian Dominy <brian@oddchange.com>
;;; ported to Vectrex, 2013 by Frank Buss <fb@frank-buss.de>
;;;
;;; This file is part of GCC.
;;;
;;; GCC is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 3, or (at your option)
;;; any later version.
;;;
;;; GCC is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.

;;; You should have received a copy of the GNU General Public License
;;; along with GCC; see the file COPYING3.  If not see
;;; <http://www.gnu.org/licenses/>.

	; Declare external for main()
	.module	crt0.tpl
	.area .text
	.globl _main

#define __STACK_TOP 0xcbea

	; Declare all linker sections, and combine them into a single bank
	.bank prog
	.area .text  (BANK=prog)
	.area .ctors (BANK=prog)

	.bank ram(BASE=0xc880,SIZE=0x26a,FSFX=_ram)
	.area .data  (BANK=ram)
	.area .bss   (BANK=ram)

   ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;;
	;;; cartridge init block
	;;;
   ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.area	.text
	.ascii "g GCE GAME_YEAR"		; cartrige id and year
	.byte 0x80						; string end
	.word GAME_MUSIC				; address to music1 in BIOS

	.byte 0xf8, 0x50, 0x40, -0x80	; height, width, rel y, rel x
	.ascii "GAME_TITLE_1"			; game title
	.byte 0x80						; string end

	.byte 0xf8, 0x50, 0x20, -0x80	; height, width, rel y, rel x
	.ascii "GAME_TITLE_2"			; game title
	.byte 0x80			; string end

	.byte 0xf8, 0x50, 0x00, -0x80	; height, width, rel y, rel x
	.ascii "GAME_TITLE_3"			; game title
	.byte 0x80						; string end

	.byte 0							; header end

   ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	;;;
	;;; __start : Entry point to the program
	;;;
   ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.area	.text
	.globl __start
__start:

_crt0_init_data:
	ldu		#s_.text
	leau	l_.text,u
	leau	l_.ctors,u			; skip constructors
	ldy		#s_.data
	ldx		#l_.data
	beq		_crt0_init_objects
_crt0_copy_data:
	lda		,u+
	sta		,y+
	leax	-1,x
	bne		_crt0_copy_data
_crt0_init_objects:
	ldu		#s_.ctors			; load first init function
	ldx 	#l_.ctors 			; init table size, usually 1 entry (2 bytes) to static init code
	beq		_crt0_init_bss
_crt0_call_ctor:
	ldy		,u++ 				; load curr init func
	pshs	x
	jsr		,y					; run it
	puls	x
	leax	-2,x
	bne 	_crt0_call_ctor
_crt0_init_bss:
	ldy		#s_.bss
	ldx		#l_.bss
	beq		_crt0_startup
_crt0_zero_bss:
	clr		,y+
	leax	-1,x
	bne		_crt0_zero_bss
_crt0_startup:
	jsr		_main
	tstb
	ble		_crt0_restart
	clr		0xcbfe;	cold reset
_crt0_restart:
	jmp 	0xf000;	rum



#music:
#        .word   0xfee8
#        .word   0xfeb6
#        .byte   0x0, 0x80
#        .byte   0x0, 0x80

	.end __start

