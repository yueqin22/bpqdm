
#ifndef DSP2803x_I2C_DEFINES_H
#define DSP2803x_I2C_DEFINES_H

#define I2C_ERROR               0xFFFF
#define I2C_ARB_LOST_ERROR      0x0001
#define I2C_NACK_ERROR          0x0002
#define I2C_BUS_BUSY_ERROR      0x1000
#define I2C_STP_NOT_READY_ERROR 0x5555
#define I2C_NO_FLAGS            0xAAAA
#define I2C_SUCCESS             0x0000

#define I2C_CLR_AL_BIT          0x0001
#define I2C_CLR_NACK_BIT        0x0002
#define I2C_CLR_ARDY_BIT        0x0004
#define I2C_CLR_RRDY_BIT        0x0008
#define I2C_CLR_SCD_BIT         0x0020

#define I2C_NO_ISRC             0x0000
#define I2C_ARB_ISRC            0x0001
#define I2C_NACK_ISRC           0x0002
#define I2C_ARDY_ISRC           0x0003
#define I2C_RX_ISRC             0x0004
#define I2C_TX_ISRC             0x0005
#define I2C_SCD_ISRC            0x0006
#define I2C_AAS_ISRC            0x0007

#define I2C_NO_STOP  0
#define I2C_YES_STOP 1
#define I2C_RECEIVE  0
#define I2C_TRANSMIT 1
#define I2C_MAX_BUFFER_SIZE 4

#define I2C_NOTSLAVE      0
#define I2C_ADDR_AS_SLAVE 1
#define I2C_ST_MSG_READY  2

#define I2C_SND_MSG1 1
#define I2C_SND_MSG2 2

#define I2C_IDLE               0
#define I2C_SLAVE_RECEIVER     1
#define I2C_SLAVE_TRANSMITTER  2
#define I2C_MASTER_RECEIVER    3
#define I2C_MASTER_TRANSMITTER 4

#define I2C_MSGSTAT_INACTIVE          0x0000
#define I2C_MSGSTAT_SEND_WITHSTOP     0x0010
#define I2C_MSGSTAT_WRITE_BUSY        0x0011
#define I2C_MSGSTAT_SEND_NOSTOP       0x0020
#define I2C_MSGSTAT_SEND_NOSTOP_BUSY  0x0021
#define I2C_MSGSTAT_RESTART           0x0022
#define I2C_MSGSTAT_READ_BUSY         0x0023

#define I2C_TRUE  1
#define I2C_FALSE 0
#define I2C_YES   1
#define I2C_NO    0
#define I2C_DUMMY_BYTE 0

struct I2CMSG {
  Uint16 MsgStatus;

  Uint16 SlaveAddress;
  Uint16 NumOfBytes;
  Uint16 MemoryHighAddr;
  Uint16 MemoryLowAddr;
  Uint16 MsgBuffer[I2C_MAX_BUFFER_SIZE];

};

#endif
