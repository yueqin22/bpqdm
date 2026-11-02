# 原工程 -> 新工程 文件映射 (new_tree_module_map)

## 1. 参与构建的源码 (59 个编译单元)

| 原路径 | 新路径 | 类型 | 原编码 | 行尾 | 删除注释数 |
|---|---|---|---|---|---:|
| `subprogram/ASMSubPrg.asm` | `asm/ASMSubPrg.asm` | asm | gbk | CRLF | 113 |
| `system/DSP2803x_CSMPasswords.asm` | `asm/DSP2803x_CSMPasswords.asm` | asm | utf-8 | CRLF | 52 |
| `system/DSP2803x_CodeStartBranch.asm` | `asm/DSP2803x_CodeStartBranch.asm` | asm | gbk | CRLF | 27 |
| `function/DSP2803x_usDelay.asm` | `asm/DSP2803x_usDelay.asm` | asm | utf-8 | CRLF | 66 |
| `public/motor_header/DataTypeDef.h` | `include/common/DataTypeDef.h` | h | gbk | CRLF | 9 |
| `public/motor_header/MotorDefine.h` | `include/common/MotorDefine.h` | h | gbk | CRLF | 63 |
| `public/motor_header/MotorStructDefine.h` | `include/common/MotorStructDefine.h` | h | gbk | CRLF | 160 |
| `public/28035_header/SystemDefine.h` | `include/common/SystemDefine.h` | h | gbk | CRLF | 72 |
| `public/function_header/f_common.h` | `include/common/f_common.h` | h | utf-8 | CRLF | 1 |
| `public/28035_header/DSP2803x_Adc.h` | `include/device/DSP2803x_Adc.h` | h | utf-8 | CRLF | 226 |
| `public/28035_header/DSP2803x_BootVars.h` | `include/device/DSP2803x_BootVars.h` | h | utf-8 | CRLF | 22 |
| `public/28035_header/DSP2803x_Cla.h` | `include/device/DSP2803x_Cla.h` | h | utf-8 | CRLF | 103 |
| `public/28035_header/DSP2803x_Cla_Defines.h` | `include/device/DSP2803x_Cla_Defines.h` | h | utf-8 | CRLF | 28 |
| `public/28035_header/DSP2803x_Comp.h` | `include/device/DSP2803x_Comp.h` | h | utf-8 | CRLF | 39 |
| `public/28035_header/DSP2803x_CpuTimers.h` | `include/device/DSP2803x_CpuTimers.h` | h | utf-8 | CRLF | 71 |
| `public/28035_header/DSP2803x_DefaultISR.h` | `include/device/DSP2803x_DefaultISR.h` | h | utf-8 | CRLF | 115 |
| `public/28035_header/DSP2803x_DevEmu.h` | `include/device/DSP2803x_DevEmu.h` | h | utf-8 | CRLF | 49 |
| `public/28035_header/DSP2803x_Device.h` | `include/device/DSP2803x_Device.h` | h | utf-8 | CRLF | 49 |
| `public/28035_header/DSP2803x_ECan.h` | `include/device/DSP2803x_ECan.h` | h | utf-8 | CRLF | 731 |
| `public/28035_header/DSP2803x_ECap.h` | `include/device/DSP2803x_ECap.h` | h | utf-8 | CRLF | 89 |
| `public/28035_header/DSP2803x_EPwm.h` | `include/device/DSP2803x_EPwm.h` | h | utf-8 | CRLF | 317 |
| `public/28035_header/DSP2803x_EPwm_defines.h` | `include/device/DSP2803x_EPwm_defines.h` | h | utf-8 | CRLF | 66 |
| `public/28035_header/DSP2803x_EQep.h` | `include/device/DSP2803x_EQep.h` | h | utf-8 | CRLF | 152 |
| `public/28035_header/DSP2803x_Examples.h` | `include/device/DSP2803x_Examples.h` | h | gbk | CRLF | 59 |
| `public/28035_header/DSP2803x_GlobalPrototypes.h` | `include/device/DSP2803x_GlobalPrototypes.h` | h | utf-8 | CRLF | 40 |
| `public/28035_header/DSP2803x_Gpio.h` | `include/device/DSP2803x_Gpio.h` | h | utf-8 | CRLF | 211 |
| `public/28035_header/DSP2803x_I2C_defines.h` | `include/device/DSP2803x_I2C_defines.h` | h | utf-8 | CRLF | 49 |
| `public/28035_header/DSP2803x_I2c.h` | `include/device/DSP2803x_I2c.h` | h | utf-8 | CRLF | 115 |
| `public/28035_header/DSP2803x_Lin.h` | `include/device/DSP2803x_Lin.h` | h | utf-8 | CRLF | 329 |
| `public/28035_header/DSP2803x_NmiIntrupt.h` | `include/device/DSP2803x_NmiIntrupt.h` | h | utf-8 | CRLF | 42 |
| `public/28035_header/DSP2803x_PieCtrl.h` | `include/device/DSP2803x_PieCtrl.h` | h | utf-8 | CRLF | 93 |
| `public/28035_header/DSP2803x_PieVect.h` | `include/device/DSP2803x_PieVect.h` | h | utf-8 | CRLF | 111 |
| `public/28035_header/DSP2803x_SWPrioritizedIsrLevels.h` | `include/device/DSP2803x_SWPrioritizedIsrLevels.h` | h | utf-8 | CRLF | 457 |
| `public/28035_header/DSP2803x_Sci.h` | `include/device/DSP2803x_Sci.h` | h | utf-8 | CRLF | 136 |
| `public/28035_header/DSP2803x_Spi.h` | `include/device/DSP2803x_Spi.h` | h | utf-8 | CRLF | 115 |
| `public/28035_header/DSP2803x_SysCtrl.h` | `include/device/DSP2803x_SysCtrl.h` | h | utf-8 | CRLF | 254 |
| `public/28035_header/DSP2803x_XIntrupt.h` | `include/device/DSP2803x_XIntrupt.h` | h | utf-8 | CRLF | 28 |
| `public/28035_header/DSP28x_Project.h` | `include/device/DSP28x_Project.h` | h | utf-8 | CRLF | 15 |
| `public/28035_header/IQmathLib.h` | `include/device/IQmathLib.h` | h | utf-8 | CRLF | 146 |
| `public/28035_header/SFO_V6.h` | `include/device/SFO_V6.h` | h | utf-8 | CRLF | 49 |
| `public/function_header/f_canlink.h` | `include/function/f_canlink.h` | h | gbk | CRLF | 60 |
| `public/function_header/f_comm.h` | `include/function/f_comm.h` | h | gbk | CRLF | 67 |
| `public/function_header/f_comm_DP.h` | `include/function/f_comm_DP.h` | h | gbk | CRLF | 43 |
| `public/function_header/f_debug.h` | `include/function/f_debug.h` | h | gbk | CRLF | 30 |
| `public/function_header/f_dspcan.h` | `include/function/f_dspcan.h` | h | gbk | CRLF | 28 |
| `public/function_header/f_eeprom.h` | `include/function/f_eeprom.h` | h | gbk | CRLF | 60 |
| `public/function_header/f_error.h` | `include/function/f_error.h` | h | gbk | CRLF | 63 |
| `public/function_header/f_fcDeal.h` | `include/function/f_fcDeal.h` | h | gbk | CRLF | 57 |
| `public/function_header/f_frqSrc.h` | `include/function/f_frqSrc.h` | h | gbk | CRLF | 53 |
| `public/function_header/f_funcCode.h` | `include/function/f_funcCode.h` | h | utf-8+surrogateescape | CRLF | 1361 |
| `public/function_header/f_funcCode_disp.c` | `include/function/f_funcCode_disp.c` | h | gbk | CRLF | 217 |
| `public/function_header/f_interface.h` | `include/function/f_interface.h` | h | gbk | CRLF | 29 |
| `public/function_header/f_invPara.h` | `include/function/f_invPara.h` | h | gbk | CRLF | 67 |
| `public/function_header/f_io.h` | `include/function/f_io.h` | h | gbk | CRLF | 230 |
| `public/function_header/f_main.h` | `include/function/f_main.h` | h | gbk | CRLF | 38 |
| `public/function_header/f_menu.h` | `include/function/f_menu.h` | h | gbk | CRLF | 72 |
| `public/function_header/f_osc.h` | `include/function/f_osc.h` | h | gbk | CRLF | 80 |
| `public/function_header/f_p2p.h` | `include/function/f_p2p.h` | h | gbk | CRLF | 25 |
| `public/function_header/f_plc.h` | `include/function/f_plc.h` | h | gbk | CRLF | 17 |
| `public/function_header/f_posCtrl.h` | `include/function/f_posCtrl.h` | h | gbk | CRLF | 18 |
| `public/function_header/f_runSrc.h` | `include/function/f_runSrc.h` | h | gbk | CRLF | 171 |
| `public/function_header/f_table_eeprom2Fc.c` | `include/function/f_table_eeprom2Fc.c` | h | gbk | CRLF | 1004 |
| `public/function_header/f_table_eeprom2Fc1.c` | `include/function/f_table_eeprom2Fc1.c` | h | gbk | CRLF | 856 |
| `public/function_header/f_table_eeprom2Fc2.c` | `include/function/f_table_eeprom2Fc2.c` | h | gbk | CRLF | 856 |
| `public/function_header/f_table_eeprom2Fc3.c` | `include/function/f_table_eeprom2Fc3.c` | h | gbk | CRLF | 856 |
| `public/function_header/f_table_eeprom2Fc4.c` | `include/function/f_table_eeprom2Fc4.c` | h | gbk | CRLF | 856 |
| `public/function_header/f_table_eeprom2Fc5.c` | `include/function/f_table_eeprom2Fc5.c` | h | gbk | CRLF | 969 |
| `public/function_header/f_table_eeprom2Fc6---.c` | `include/function/f_table_eeprom2Fc6---.c` | h | gbk | CRLF | 1292 |
| `public/function_header/f_ui.h` | `include/function/f_ui.h` | h | gbk | CRLF | 109 |
| `public/motor_header/MotorCurrentTransform.h` | `include/motor/MotorCurrentTransform.h` | h | gbk | CRLF | 47 |
| `public/motor_header/MotorDataExchange.h` | `include/motor/MotorDataExchange.h` | h | gbk | CRLF | 8 |
| `public/motor_header/MotorEncoder.h` | `include/motor/MotorEncoder.h` | h | gbk | CRLF | 71 |
| `public/motor_header/MotorInclude.h` | `include/motor/MotorInclude.h` | h | gbk | CRLF | 34 |
| `public/motor_header/MotorInfoCollectInclude.h` | `include/motor/MotorInfoCollectInclude.h` | h | gbk | CRLF | 29 |
| `public/motor_header/MotorInvProtectInclude.h` | `include/motor/MotorInvProtectInclude.h` | h | gbk | CRLF | 60 |
| `public/motor_header/MotorParaIDinclude.h` | `include/motor/MotorParaIDinclude.h` | h | gbk | CRLF | 42 |
| `public/motor_header/MotorPmsmMain.h` | `include/motor/MotorPmsmMain.h` | h | gbk | CRLF | 104 |
| `public/motor_header/MotorPmsmParEst.h` | `include/motor/MotorPmsmParEst.h` | h | gbk | CRLF | 43 |
| `public/motor_header/MotorPublicCalInclude.h` | `include/motor/MotorPublicCalInclude.h` | h | gbk | CRLF | 31 |
| `public/motor_header/MotorPwmInclude.h` | `include/motor/MotorPwmInclude.h` | h | gbk | CRLF | 47 |
| `public/motor_header/MotorSpeedCheceInclude.h` | `include/motor/MotorSpeedCheceInclude.h` | h | gbk | CRLF | 66 |
| `public/motor_header/MotorSvcInclude.h` | `include/motor/MotorSvcInclude.h` | h | gbk | CRLF | 19 |
| `public/28035_header/MotorVCInclude.h` | `include/motor/MotorVCInclude.h` | h | gbk | CRLF | 58 |
| `public/motor_header/MotorVFInclude.h` | `include/motor/MotorVFInclude.h` | h | gbk | CRLF | 111 |
| `public/motor_header/SubPrgInclude.h` | `include/motor/SubPrgInclude.h` | h | gbk | CRLF | 56 |
| `public/motor_header/ZeroPosCtl.h` | `include/motor/ZeroPosCtl.h` | h | gbk | CRLF | 26 |
| `function/f_canlink.c` | `src/function/f_canlink.c` | c | gbk | CRLF | 164 |
| `function/f_comm.c` | `src/function/f_comm.c` | c | utf-8+surrogateescape | CRLF | 422 |
| `function/f_comm_DP.c` | `src/function/f_comm_DP.c` | c | gbk | CRLF | 147 |
| `function/f_common.c` | `src/function/f_common.c` | c | gbk | CRLF | 38 |
| `function/f_debug.c` | `src/function/f_debug.c` | c | gbk | CRLF | 175 |
| `function/f_dspcan.c` | `src/function/f_dspcan.c` | c | utf-8+surrogateescape | CRLF | 111 |
| `function/f_eeprom.c` | `src/function/f_eeprom.c` | c | gbk | CRLF | 397 |
| `function/f_error.c` | `src/function/f_error.c` | c | utf-8 | CRLF | 319 |
| `function/f_fcDeal.c` | `src/function/f_fcDeal.c` | c | gbk | CRLF | 143 |
| `function/f_frqSrc.c` | `src/function/f_frqSrc.c` | c | utf-8+surrogateescape | CRLF | 408 |
| `function/f_frqSrc_pid.c` | `src/function/f_frqSrc_pid.c` | c | gbk | CRLF | 175 |
| `function/f_funcCode.c` | `src/function/f_funcCode.c` | c | utf-8 | CRLF | 955 |
| `function/f_interface.c` | `src/function/f_interface.c` | c | utf-8+surrogateescape | CRLF | 292 |
| `function/f_invPara.c` | `src/function/f_invPara.c` | c | gbk | CRLF | 366 |
| `function/f_io.c` | `src/function/f_io.c` | c | gb18030 | CRLF | 1128 |
| `function/f_main.c` | `src/function/f_main.c` | c | gbk | CRLF | 358 |
| `function/f_menu.c` | `src/function/f_menu.c` | c | utf-8+surrogateescape | CRLF | 749 |
| `function/f_modbus.c` | `src/function/f_modbus.c` | c | gbk | CRLF | 85 |
| `function/f_motorFc.c` | `src/function/f_motorFc.c` | c | gbk | CRLF | 41 |
| `function/f_osc.c` | `src/function/f_osc.c` | c | gbk | CRLF | 250 |
| `function/f_p2p.c` | `src/function/f_p2p.c` | c | gbk | CRLF | 26 |
| `function/f_plc.c` | `src/function/f_plc.c` | c | gbk | CRLF | 93 |
| `function/f_profibus.c` | `src/function/f_profibus.c` | c | gb18030 | CRLF | 204 |
| `function/f_runSrc.c` | `src/function/f_runSrc.c` | c | utf-8 | CRLF | 717 |
| `function/f_runSrc_accDecFrq.c` | `src/function/f_runSrc_accDecFrq.c` | c | gbk | CRLF | 123 |
| `function/f_torqueCtrl.c` | `src/function/f_torqueCtrl.c` | c | gbk | CRLF | 62 |
| `function/f_ui.c` | `src/function/f_ui.c` | c | gbk | CRLF | 388 |
| `function/f_vf.c` | `src/function/f_vf.c` | c | gbk | CRLF | 39 |
| `motor/MotorCarrier.c` | `src/motor/MotorCarrier.c` | c | gbk | CRLF | 83 |
| `motor/MotorConstant.c` | `src/motor/MotorConstant.c` | c | gbk | CRLF | 54 |
| `motor/MotorCurrentTransform.c` | `src/motor/MotorCurrentTransform.c` | c | gbk | CRLF | 14 |
| `motor/MotorDataExchange.c` | `src/motor/MotorDataExchange.c` | c | gbk | CRLF | 56 |
| `motor/MotorDebug.c` | `src/motor/MotorDebug.c` | c | gbk | CRLF | 36 |
| `motor/MotorEncoder.c` | `src/motor/MotorEncoder.c` | c | utf-8+surrogateescape | CRLF | 192 |
| `motor/MotorImParEst.c` | `src/motor/MotorImParEst.c` | c | gbk | CRLF | 163 |
| `motor/MotorInfoCollect.c` | `src/motor/MotorInfoCollect.c` | c | gbk | CRLF | 26 |
| `motor/MotorInvProtect.c` | `src/motor/MotorInvProtect.c` | c | utf-8+surrogateescape | CRLF | 201 |
| `motor/MotorMain.c` | `src/motor/MotorMain.c` | c | utf-8+surrogateescape | CRLF | 177 |
| `motor/MotorPWM.c` | `src/motor/MotorPWM.c` | c | gbk | CRLF | 138 |
| `motor/MotorParChange.c` | `src/motor/MotorParChange.c` | c | utf-8+surrogateescape | CRLF | 127 |
| `motor/MotorParEst.c` | `src/motor/MotorParEst.c` | c | gbk | CRLF | 36 |
| `motor/MotorPmsmMain.c` | `src/motor/MotorPmsmMain.c` | c | utf-8+surrogateescape | CRLF | 118 |
| `motor/MotorPmsmParEst.c` | `src/motor/MotorPmsmParEst.c` | c | gb18030 | CRLF | 175 |
| `motor/MotorPublicCal.c` | `src/motor/MotorPublicCal.c` | c | gbk | CRLF | 75 |
| `motor/MotorSVC.c` | `src/motor/MotorSVC.c` | c | gb18030 | CRLF | 141 |
| `motor/MotorSpeedCheck.c` | `src/motor/MotorSpeedCheck.c` | c | gbk | CRLF | 77 |
| `motor/MotorVCMain.c` | `src/motor/MotorVCMain.c` | c | utf-8+surrogateescape | CRLF | 214 |
| `motor/MotorVF.c` | `src/motor/MotorVF.c` | c | gb18030 | CRLF | 160 |
| `motor/MotorVar.c` | `src/motor/MotorVar.c` | c | gbk | CRLF | 19 |
| `motor/ZeroPosCtl.c` | `src/motor/ZeroPosCtl.c` | c | gbk | CRLF | 9 |
| `function/DSP2803x_PieCtrl.c` | `src/orphan/DSP2803x_PieCtrl.c` | c | utf-8 | CRLF | 34 |
| `function/DSP2803x_SysCtrl.c` | `src/orphan/DSP2803x_SysCtrl.c` | c | utf-8 | CRLF | 220 |
| `function/f_length.c` | `src/orphan/f_length.c` | c | gbk | CRLF | 93 |
| `function/z_func.c` | `src/orphan/z_func.c` | c | gbk | CRLF | 159 |
| `subprogram/SubPrg.c` | `src/subprogram/SubPrg.c` | c | gbk | CRLF | 22 |
| `function/DSP2803x_CpuTimers.c` | `src/system/DSP2803x_CpuTimers.c` | c | utf-8 | CRLF | 53 |
| `function/DSP2803x_GlobalVariableDefs.c` | `src/system/DSP2803x_GlobalVariableDefs.c` | c | utf-8 | CRLF | 65 |
| `function/main.c` | `src/system/main.c` | c | gbk | CRLF | 39 |
| `system/mainInit_2803x.c` | `src/system/mainInit_2803x.c` | c | gbk | CRLF | 230 |

## 2. 头文件 (82 个)

| 原路径 | 新路径 | 说明 |
|---|---|---|
| `public/motor_header/DataTypeDef.h` | `include/common/DataTypeDef.h` | 公共类型/宏/结构定义 |
| `public/motor_header/MotorDefine.h` | `include/common/MotorDefine.h` | 公共类型/宏/结构定义 |
| `public/motor_header/MotorStructDefine.h` | `include/common/MotorStructDefine.h` | 公共类型/宏/结构定义 |
| `public/28035_header/SystemDefine.h` | `include/common/SystemDefine.h` | 公共类型/宏/结构定义 |
| `public/function_header/f_common.h` | `include/common/f_common.h` | 公共类型/宏/结构定义 |
| `public/28035_header/DSP2803x_Adc.h` | `include/device/DSP2803x_Adc.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_BootVars.h` | `include/device/DSP2803x_BootVars.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Cla.h` | `include/device/DSP2803x_Cla.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Cla_Defines.h` | `include/device/DSP2803x_Cla_Defines.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Comp.h` | `include/device/DSP2803x_Comp.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_CpuTimers.h` | `include/device/DSP2803x_CpuTimers.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_DefaultISR.h` | `include/device/DSP2803x_DefaultISR.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_DevEmu.h` | `include/device/DSP2803x_DevEmu.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Device.h` | `include/device/DSP2803x_Device.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_ECan.h` | `include/device/DSP2803x_ECan.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_ECap.h` | `include/device/DSP2803x_ECap.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_EPwm.h` | `include/device/DSP2803x_EPwm.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_EPwm_defines.h` | `include/device/DSP2803x_EPwm_defines.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_EQep.h` | `include/device/DSP2803x_EQep.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Examples.h` | `include/device/DSP2803x_Examples.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_GlobalPrototypes.h` | `include/device/DSP2803x_GlobalPrototypes.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Gpio.h` | `include/device/DSP2803x_Gpio.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_I2C_defines.h` | `include/device/DSP2803x_I2C_defines.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_I2c.h` | `include/device/DSP2803x_I2c.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Lin.h` | `include/device/DSP2803x_Lin.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_NmiIntrupt.h` | `include/device/DSP2803x_NmiIntrupt.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_PieCtrl.h` | `include/device/DSP2803x_PieCtrl.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_PieVect.h` | `include/device/DSP2803x_PieVect.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_SWPrioritizedIsrLevels.h` | `include/device/DSP2803x_SWPrioritizedIsrLevels.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Sci.h` | `include/device/DSP2803x_Sci.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_Spi.h` | `include/device/DSP2803x_Spi.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_SysCtrl.h` | `include/device/DSP2803x_SysCtrl.h` | TI 设备头文件 |
| `public/28035_header/DSP2803x_XIntrupt.h` | `include/device/DSP2803x_XIntrupt.h` | TI 设备头文件 |
| `public/28035_header/DSP28x_Project.h` | `include/device/DSP28x_Project.h` | TI 设备头文件 |
| `public/28035_header/IQmathLib.h` | `include/device/IQmathLib.h` | TI 设备头文件 |
| `public/28035_header/SFO_V6.h` | `include/device/SFO_V6.h` | TI 设备头文件 |
| `public/function_header/f_canlink.h` | `include/function/f_canlink.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_comm.h` | `include/function/f_comm.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_comm_DP.h` | `include/function/f_comm_DP.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_debug.h` | `include/function/f_debug.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_dspcan.h` | `include/function/f_dspcan.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_eeprom.h` | `include/function/f_eeprom.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_error.h` | `include/function/f_error.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_fcDeal.h` | `include/function/f_fcDeal.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_frqSrc.h` | `include/function/f_frqSrc.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_funcCode.h` | `include/function/f_funcCode.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_funcCode_disp.c` | `include/function/f_funcCode_disp.c` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_interface.h` | `include/function/f_interface.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_invPara.h` | `include/function/f_invPara.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_io.h` | `include/function/f_io.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_main.h` | `include/function/f_main.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_menu.h` | `include/function/f_menu.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_osc.h` | `include/function/f_osc.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_p2p.h` | `include/function/f_p2p.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_plc.h` | `include/function/f_plc.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_posCtrl.h` | `include/function/f_posCtrl.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_runSrc.h` | `include/function/f_runSrc.h` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_table_eeprom2Fc.c` | `include/function/f_table_eeprom2Fc.c` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_table_eeprom2Fc1.c` | `include/function/f_table_eeprom2Fc1.c` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_table_eeprom2Fc2.c` | `include/function/f_table_eeprom2Fc2.c` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_table_eeprom2Fc3.c` | `include/function/f_table_eeprom2Fc3.c` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_table_eeprom2Fc4.c` | `include/function/f_table_eeprom2Fc4.c` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_table_eeprom2Fc5.c` | `include/function/f_table_eeprom2Fc5.c` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_table_eeprom2Fc6---.c` | `include/function/f_table_eeprom2Fc6---.c` | 功能层接口 (含被 #include 的表文件) |
| `public/function_header/f_ui.h` | `include/function/f_ui.h` | 功能层接口 (含被 #include 的表文件) |
| `public/motor_header/MotorCurrentTransform.h` | `include/motor/MotorCurrentTransform.h` | 电机层接口 |
| `public/motor_header/MotorDataExchange.h` | `include/motor/MotorDataExchange.h` | 电机层接口 |
| `public/motor_header/MotorEncoder.h` | `include/motor/MotorEncoder.h` | 电机层接口 |
| `public/motor_header/MotorInclude.h` | `include/motor/MotorInclude.h` | 电机层接口 |
| `public/motor_header/MotorInfoCollectInclude.h` | `include/motor/MotorInfoCollectInclude.h` | 电机层接口 |
| `public/motor_header/MotorInvProtectInclude.h` | `include/motor/MotorInvProtectInclude.h` | 电机层接口 |
| `public/motor_header/MotorParaIDinclude.h` | `include/motor/MotorParaIDinclude.h` | 电机层接口 |
| `public/motor_header/MotorPmsmMain.h` | `include/motor/MotorPmsmMain.h` | 电机层接口 |
| `public/motor_header/MotorPmsmParEst.h` | `include/motor/MotorPmsmParEst.h` | 电机层接口 |
| `public/motor_header/MotorPublicCalInclude.h` | `include/motor/MotorPublicCalInclude.h` | 电机层接口 |
| `public/motor_header/MotorPwmInclude.h` | `include/motor/MotorPwmInclude.h` | 电机层接口 |
| `public/motor_header/MotorSpeedCheceInclude.h` | `include/motor/MotorSpeedCheceInclude.h` | 电机层接口 |
| `public/motor_header/MotorSvcInclude.h` | `include/motor/MotorSvcInclude.h` | 电机层接口 |
| `public/28035_header/MotorVCInclude.h` | `include/motor/MotorVCInclude.h` | 电机层接口 |
| `public/motor_header/MotorVFInclude.h` | `include/motor/MotorVFInclude.h` | 电机层接口 |
| `public/motor_header/SubPrgInclude.h` | `include/motor/SubPrgInclude.h` | 电机层接口 |
| `public/motor_header/ZeroPosCtl.h` | `include/motor/ZeroPosCtl.h` | 电机层接口 |
