
#ifndef DSP2803x_DEV_EMU_H
#define DSP2803x_DEV_EMU_H

#ifdef __cplusplus
extern "C" {
#endif

struct DEVICECNF_BITS  {
   Uint16 rsvd1:3;
   Uint16 VMAPS:1;
   Uint16 rsvd2:1;
   Uint16 XRSn:1;
   Uint16 rsvd3:10;
   Uint16 rsvd4:3;
   Uint16 ENPROT:1;
   Uint16 rsvd5:7;
   Uint16 TRSTn:1;
   Uint16 PINOUTSELSTS:1;
   Uint16 PINOUTSELPUD:1;
   Uint16 rsvd6:2;
};

union DEVICECNF_REG {
   Uint32                 all;
   struct DEVICECNF_BITS  bit;
};

struct CLASSID_BITS   {
   Uint16 CLASSNO:8;
   Uint16 PARTTYPE:8;
};

union CLASSID_REG {
   Uint16               all;
   struct CLASSID_BITS  bit;
};

struct DEV_EMU_REGS {
   union DEVICECNF_REG DEVICECNF;
   union CLASSID_REG   CLASSID;
   Uint16              REVID;
};

struct PARTID_BITS   {
   Uint16 PARTNO:8;
   Uint16 PARTTYPE:8;
};

union PARTID_REG {
   Uint16               all;
   struct PARTID_BITS   bit;
};

struct PARTID_REGS {
   union PARTID_REG PARTID;
};

extern volatile struct DEV_EMU_REGS DevEmuRegs;
extern volatile struct PARTID_REGS PartIdRegs;

#ifdef __cplusplus
}
#endif

#endif
