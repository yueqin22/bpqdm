
    .ref 	_c_int00
    .global code_start
	.global _MovePrgFrFlashToRam

    .sect "codestart"

code_start:
    LB main_init

	.text
main_init:
    SETC 	OBJMODE
    EALLOW
    MOVZ 	DP, #7029h>>6
    MOV 	@7029h, #0068h
    EDIS

	MOV		ACC,	#00H
	MOVL 	XAR5,	#0000H
	MOVL 	XAR4,	#(400H-1)
	RPT		@AR4
	|| MOV	*XAR5++,	ACC

	MOVL 	XAR5,	#0400H
	MOVL 	XAR4,	#(400H-1)
	RPT		@AR4
	|| MOV	*XAR5++,	ACC

	MOVL 	XAR5,	#8000H
	MOVL 	XAR4,	#(1000H-1)
	RPT		@AR4
	|| MOV	*XAR5++,	ACC

	MOVL 	XAR5,	#9000H
	MOVL 	XAR4,	#(1000H-1)
	RPT		@AR4
	|| MOV	*XAR5++,	ACC

	LB _c_int00

_MovePrgFrFlashToRam:
	MOVL XAR7, ACC
	MOVL XAR6, *-SP[4]
	RPT	 @AR4
	|| PREAD *XAR6++, *XAR7
	LRETR

	.end
