# 重复文件清单 (duplicate_file_list)

## 1. 同名文件出现在多个目录

| 文件名 | 出现位置 | 内容是否完全相同 |
|---|---|---|
| `DSP2803x_CSMPasswords.asm` | function<br>system | **否** |
| `DSP2803x_CodeStartBranch.asm` | function<br>system | **否** |
| `DSP2803x_CpuTimers.c` | function<br>system | **否** |
| `DSP2803x_Device.h` | public/28035_header<br>public/motor_header | **否** |
| `DSP2803x_GlobalVariableDefs.c` | function<br>system | **否** |
| `MotorVCInclude.h` | public/28035_header<br>public/motor_header | **否** |
| `f_canlink.h` | function/include<br>public/function_header | **否** |
| `f_comm.h` | function/include<br>public/function_header | 是 |
| `f_comm_DP.h` | function/include<br>public/function_header | 是 |
| `f_common.h` | function/include<br>public/function_header | 是 |
| `f_debug.h` | function/include<br>public/function_header | 是 |
| `f_dspcan.h` | function/include<br>public/function_header | **否** |
| `f_eeprom.h` | function/include<br>public/function_header | 是 |
| `f_error.h` | function/include<br>public/function_header | 是 |
| `f_fcDeal.h` | function/include<br>public/function_header | **否** |
| `f_frqSrc.h` | function/include<br>public/function_header | 是 |
| `f_funcCode.h` | function/include<br>public/function_header | **否** |
| `f_funcCode_disp.c` | function/include<br>public/function_header | **否** |
| `f_interface.h` | function/include<br>public/function_header | 是 |
| `f_invPara.h` | function/include<br>public/function_header | **否** |
| `f_io.h` | function/include<br>public/function_header | 是 |
| `f_main.h` | function/include<br>public/function_header | 是 |
| `f_menu.h` | function/include<br>public/function_header | **否** |
| `f_osc.h` | function/include<br>public/function_header | **否** |
| `f_p2p.h` | function/include<br>public/function_header | **否** |
| `f_plc.h` | function/include<br>public/function_header | **否** |
| `f_posCtrl.h` | function/include<br>public/function_header | 是 |
| `f_runSrc.h` | function/include<br>public/function_header | 是 |
| `f_table_eeprom2Fc.c` | function/include<br>public/function_header | 是 |
| `f_table_eeprom2Fc1.c` | function/include<br>public/function_header | 是 |
| `f_table_eeprom2Fc2.c` | function/include<br>public/function_header | 是 |
| `f_table_eeprom2Fc3.c` | function/include<br>public/function_header | 是 |
| `f_table_eeprom2Fc4.c` | function/include<br>public/function_header | 是 |
| `f_table_eeprom2Fc5.c` | function/include<br>public/function_header | 是 |
| `f_table_eeprom2Fc6---.c` | function/include<br>public/function_header | 是 |
| `f_ui.h` | function/include<br>public/function_header | 是 |
| `main.c` | function<br>system | 是 |

## 2. 内容完全相同的文件组 (任意文件名)

| # | 文件组 | 副本数 |
|---:|---|---:|
| 1 | function/main.c<br>system/main.c | 2 |
| 2 | function/include/f_comm.h<br>public/function_header/f_comm.h | 2 |
| 3 | function/include/f_common.h<br>public/function_header/f_common.h | 2 |
| 4 | function/include/f_comm_DP.h<br>public/function_header/f_comm_DP.h | 2 |
| 5 | function/include/f_debug.h<br>public/function_header/f_debug.h | 2 |
| 6 | function/include/f_eeprom.h<br>public/function_header/f_eeprom.h | 2 |
| 7 | function/include/f_error.h<br>public/function_header/f_error.h | 2 |
| 8 | function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h | 2 |
| 9 | function/include/f_interface.h<br>public/function_header/f_interface.h | 2 |
| 10 | function/include/f_io.h<br>public/function_header/f_io.h | 2 |
| 11 | function/include/f_main.h<br>public/function_header/f_main.h | 2 |
| 12 | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h | 2 |
| 13 | function/include/f_runSrc.h<br>public/function_header/f_runSrc.h | 2 |
| 14 | function/include/f_table_eeprom2Fc.c<br>public/function_header/f_table_eeprom2Fc.c | 2 |
| 15 | function/include/f_table_eeprom2Fc1.c<br>public/function_header/f_table_eeprom2Fc1.c | 2 |
| 16 | function/include/f_table_eeprom2Fc2.c<br>public/function_header/f_table_eeprom2Fc2.c | 2 |
| 17 | function/include/f_table_eeprom2Fc3.c<br>public/function_header/f_table_eeprom2Fc3.c | 2 |
| 18 | function/include/f_table_eeprom2Fc4.c<br>public/function_header/f_table_eeprom2Fc4.c | 2 |
| 19 | function/include/f_table_eeprom2Fc5.c<br>public/function_header/f_table_eeprom2Fc5.c | 2 |
| 20 | function/include/f_table_eeprom2Fc6---.c<br>public/function_header/f_table_eeprom2Fc6---.c | 2 |
| 21 | function/include/f_ui.h<br>public/function_header/f_ui.h | 2 |
| 22 | function/include/GetTable_eeprom2Fc.m<br>public/function_header/GetTable_eeprom2Fc.m | 2 |

## 3. 目录内但不在链接清单中的源码 (孤儿文件)

| 文件 | 说明 |
|---|---|
| `function/DSP2803x_PieCtrl.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/DSP2803x_SysCtrl.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/f_length.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/include/f_funcCode_disp.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/include/f_table_eeprom2Fc.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/include/f_table_eeprom2Fc1.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/include/f_table_eeprom2Fc2.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/include/f_table_eeprom2Fc3.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/include/f_table_eeprom2Fc4.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/include/f_table_eeprom2Fc5.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/include/f_table_eeprom2Fc6---.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `function/z_func.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `public/function_header/f_funcCode_disp.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `public/function_header/f_table_eeprom2Fc.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `public/function_header/f_table_eeprom2Fc1.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `public/function_header/f_table_eeprom2Fc2.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `public/function_header/f_table_eeprom2Fc3.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `public/function_header/f_table_eeprom2Fc4.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `public/function_header/f_table_eeprom2Fc5.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |
| `public/function_header/f_table_eeprom2Fc6---.c` | 目录中存在, 但 `Debug.lkf` 无对应 .obj |

