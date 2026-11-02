
#ifndef __F_FC_DEAL_H__
#define __F_FC_DEAL_H__

#if F_DEBUG_RAM

#define DEBUG_F_RESTORE_COMPANY_PARA_DEAL   0
#define DEBUG_F_CLEAR_RECORD                0
#define DEBUG_F_LIMIT_OTHER_CODE            0

#elif 1

#define DEBUG_F_RESTORE_COMPANY_PARA_DEAL   1
#define DEBUG_F_CLEAR_RECORD                1
#define DEBUG_F_LIMIT_OTHER_CODE            1

#endif

#define INIT_EXCEPT_SERIES_S_0  GetCodeIndex(funcCode.group.ff[0])
#define INIT_EXCEPT_SERIES_E_0  GetCodeIndex(funcCode.group.ff[FFNUM-1])

#define INIT_EXCEPT_SERIES_S_1  GetCodeIndex(funcCode.group.fp[0])
#define INIT_EXCEPT_SERIES_E_1  GetCodeIndex(funcCode.group.fp[1])

#define INIT_EXCEPT_SERIES_S_2  FC_MOTOR1_START_INDEX
#define INIT_EXCEPT_SERIES_E_2  FC_MOTOR1_END_INDEX

#define INIT_EXCEPT_SERIES_S_3  FC_MOTOR2_START_INDEX
#define INIT_EXCEPT_SERIES_E_3  FC_MOTOR2_END_INDEX

#define INIT_EXCEPT_SERIES_S_6  FC_MOTOR3_START_INDEX
#define INIT_EXCEPT_SERIES_E_6  FC_MOTOR3_END_INDEX

#define INIT_EXCEPT_SERIES_S_7  FC_MOTOR4_START_INDEX
#define INIT_EXCEPT_SERIES_E_7  FC_MOTOR4_END_INDEX

#define INIT_EXCEPT_SERIES_S_4  GetCodeIndex(funcCode.group.ae[0])
#define INIT_EXCEPT_SERIES_E_4  GetCodeIndex(funcCode.group.ae[AENUM-1])

#define INIT_EXCEPT_SERIES_S_5  GetCodeIndex(funcCode.code.errorLatest1)
#define INIT_EXCEPT_SERIES_E_5  LAST_ERROR_RECORD_INDEX

#define INIT_EXCEPT_SINGLE_0    GetCodeIndex(funcCode.code.runTimeAddup)
#define INIT_EXCEPT_SINGLE_1    GetCodeIndex(funcCode.code.runTimeAddupSec)
#define INIT_EXCEPT_SINGLE_2    GetCodeIndex(funcCode.code.softVersion)
#define INIT_EXCEPT_SINGLE_3    GetCodeIndex(funcCode.code.radiatorTemp)
#define INIT_EXCEPT_SINGLE_4    GetCodeIndex(funcCode.code.temp2)
#define INIT_EXCEPT_SINGLE_5    GetCodeIndex(funcCode.code.frqPoint)

#define INIT_EXCEPT_SINGLE_6    GetCodeIndex(funcCode.code.powerUpTimeAddup)
#define INIT_EXCEPT_SINGLE_7    GetCodeIndex(funcCode.code.powerUpTimeAddupSec)
#define INIT_EXCEPT_SINGLE_8    GetCodeIndex(funcCode.code.powerAddup)
#define INIT_EXCEPT_SINGLE_9    GetCodeIndex(funcCode.code.errorFrqUnit)

#define INIT_EXCEPT_SINGLE_10   GetCodeIndex(funcCode.code.productVersion)
#define INIT_EXCEPT_SINGLE_11   GetCodeIndex(funcCode.code.softVersion)

#define CLEAR_RECORD_SERIES_S_0  GetCodeIndex(funcCode.code.errorLatest1)
#define CLEAR_RECORD_SERIES_E_0  LAST_ERROR_RECORD_INDEX

#define CLEAR_RECORD_SINGLE_0    GetCodeIndex(funcCode.code.runTimeAddup)
#define CLEAR_RECORD_SINGLE_1    GetCodeIndex(funcCode.code.runTimeAddupSec)
#define CLEAR_RECORD_SINGLE_2    GetCodeIndex(funcCode.code.powerUpTimeAddup)
#define CLEAR_RECORD_SINGLE_3    GetCodeIndex(funcCode.code.powerUpTimeAddupSec)
#define CLEAR_RECORD_SINGLE_4    GetCodeIndex(funcCode.code.powerAddup)
#define CLEAR_RECORD_SINGLE_5    GetCodeIndex(funcCode.code.errorFrqUnit)

#endif
