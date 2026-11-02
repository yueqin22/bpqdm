
	   .def     _swap
	   .def     _abs
	   .def     _absl
       .def 	_qsin
       .def 	_qatan
       .def 	_qsqrt

	   .global  _swap
	   .global  _abs
	   .global  _absl
       .global  _qsin
       .global  _qatan
       .global  _qsqrt

       .text
_abs:	MOV     ACC,AL<<16
        ABS   	ACC
        MOV     AL,AH
		LRETR

_absl:	ABS   	ACC
		LRETR

_swap:	FLIP	AL
		LRETR

K5      .set 0x6480
K4      .set 0x52FF
K3      .set 0xAACC
K2      .set 0x45B8
K1      .set 0x7338

_qsin:	PUSH	ST0
		PUSH	ST1

        SETC    SXM,OVM
        MOV     ACC,AL<<16
        CLRC    TC
        ABSTC   ACC

        LSL     ACC,#1
        ABS     ACC
        SFR     ACC,#1

        MOVL    XT,ACC

        MPY     ACC,T,#K1

        ADD     ACC,#K2<<14

        QMPYL   ACC,XT,ACC
        ADD     AH,#K3

        QMPYL   ACC,XT,ACC
        ADD     ACC,#K4<<7

        QMPYL   ACC,XT,ACC
        ADD     ACC,#K5<<13
        QMPYL   ACC,XT,ACC

        LSL     ACC,#6
        ABS     ACC
        NEGTC   ACC
        MOV     AL,AH

		POP		ST1
		POP		ST0
        LRETR

A1      .set    0x5179
A2      .set    0x6C97
A3      .set    0xBCFA
A4      .set    0x462F
A5      .set    -2401

_qatan:     PUSH	ST0
			PUSH	ST1

			SETC    SXM,OVM
            MOV     AR4,AH
            ABS     ACC
            LSR64   ACC:P,#1
            MOV     AR5,AL

            CSB     ACC
            LSL64   ACC:P,T

            MOV     PH,AH
            TCLR    T,#4

            MOV     ACC,#1<<14
            LSLL    ACC,T

            RPT     #15
            || SUBCU ACC,PH

            MOV     ACC,AL<<16
            ABS     ACC
            MOV     AR5,AH,NTC
            MOV     TL,#0
            MOV     T,AR5

            MPY     ACC,@T,#A5

            ADD     ACC,#A4<<15

            QMPYL   ACC,XT,@ACC
            ADD     ACC, #A3<<15

            QMPYL   ACC,XT,@ACC
            ADD     ACC, #A2<<8

            QMPYL   ACC,XT,@ACC
            ADD     ACC, #A1<<14

            QMPYL   ACC,XT,@ACC

            SFR     ACC,#14
            MOVL    P,ACC
            NEG     ACC
            ADD     ACC,#4000h
            MOVL    P,ACC,NTC
            MOVL    ACC,P

            TBIT    AR4,#15
            NEGTC   ACC

			POP		ST1
			POP		ST0
            LRETR

B0      .set    06a48h
B1      .set    05d1dh
B2      .set    0a9edh
B3      .set    046d6h
B4      .set    0bb54h
B5      .set    00e5ah
SQRT2   .set    05a82h

_qsqrt:		PUSH	ST0
			PUSH	ST1

            SETC    SXM
            MPY     P,T,#0
            LSR64   ACC:P,#1

            CSB     ACC
            LSL64   ACC:P,T

            TBIT    @T,#0
            MOV     AR4,T
            MOVL    XT,ACC
            MOV     AH,AR4
            LSR     AH,#1
            MOV     AR4,AH

            MPY     ACC,T,#B5
            ADD     ACC,#B4<<15

            QMPYL   ACC,XT,@ACC
            ADD     ACC, #B3<<15

            QMPYL   ACC,XT,@ACC
            ADD     ACC, #B2<<14

            QMPYL   ACC,XT,@ACC
            ADD     ACC, #B1<<13

            QMPYL   ACC,XT,@ACC
            ADD     ACC, #B0<<9

            MOVH    T,ACC<<5
            MPY     P,T,#SQRT2

            LSL     ACC,#4
            MOVL    P,ACC,NTC
            MOVL    ACC,P

            MOV     T,AR4
            LSRL    ACC,T
            MOVH    AL,ACC<<2

			POP		ST1
			POP		ST0
            LRETR
