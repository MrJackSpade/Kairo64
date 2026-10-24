;; #LAYOUT# STD *       #TAKE
;; #LAYOUT# X16 BASIC_0 #TAKE-OFFSET 2000
;; #LAYOUT# *   BASIC_0 #TAKE
;; #LAYOUT# *   *       #IGNORE

; This file is under the MIT license, it contains code released by Microsoft Corporation.
; See LICENSE for more information.

;
; Math package - round and move FAC1 to FAC2
;
; This is identical to the original Microsoft implementation where it was named MOVAF.
;
; Output:
; - .A - FAC1 exponent
;
; See also:
; - https://github.com/microsoft/BASIC-M6502/blob/7460af2c03ae19c0e60ff327489229d2005b9357/m6502.asm#L5540C1-L5548C12
; - [CM64] Computes Mapping the Commodore 64 - page 115
; - https://www.c64-wiki.com/wiki/BASIC-ROM
; - https://www.c64-wiki.com/wiki/Floating_point_arithmetic
;

mov_r_FAC1_FAC2:

	; First round FAC1
	jsr round_FAC1

	; FALLTROUGH to mov_FAC1_FAC2
