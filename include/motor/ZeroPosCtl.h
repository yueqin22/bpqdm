
#ifndef ZERO_POS_CTL_INCLUDE_H
#define ZERO_POS_CTL_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "DataTypeDef.h"

typedef struct ZERO_POS_STRUCT_DEF {
    u16     Flag;

	s16  	PosInit;
	s16  	PosLast;
	s16  	Pos;

    s16     KPPos;
    s16     KPSpeed;
	s16  	KPDecCoff;

	s16  	ItSet;
	s16  	ItKpPos;
	s16  	ItKpSpeed;

    s16     Speed;
    u32     TimeLast;
    u32     DetaTime;

}ZERO_POS_STRUCT;

#define abs(x)  	((x>0)?(x):-(x))

#define Max(x,y)	((x>y)?(x):(y))

#define Min(x,y)	((x<y)?(x):(y))

extern ZERO_POS_STRUCT          gZeroPos;

extern s16 Filter(s16 LastOne, s16 Input, s16 Coff);

#ifdef __cplusplus
}
#endif

#endif
