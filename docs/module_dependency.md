# 模块依赖 (module_dependency)

## 1. include 搜索顺序 (来自 jt380.pjt)

1. `public/28035_header`
2. `public/motor_header`
3. `public/function_header`

此外, `#include "..."` 会先搜索**包含者所在目录**。

## 2. 每个源文件的直接依赖 (已按上述顺序解析到实际文件)

| 文件 | 包含的头文件 (解析结果) |
|---|---|
| `function/DSP2803x_CSMPasswords.asm` | - |
| `function/DSP2803x_CodeStartBranch.asm` | - |
| `function/DSP2803x_CpuTimers.c` | `public/28035_header/DSP2803x_Device.h`<br>`public/28035_header/DSP2803x_Examples.h` |
| `function/DSP2803x_GlobalVariableDefs.c` | `public/28035_header/DSP2803x_Device.h` |
| `function/DSP2803x_PieCtrl.c` | `public/28035_header/DSP2803x_Device.h`<br>`public/28035_header/DSP2803x_Examples.h` |
| `function/DSP2803x_SysCtrl.c` | `public/28035_header/DSP2803x_Device.h`<br>`public/28035_header/DSP2803x_Examples.h` |
| `function/DSP2803x_usDelay.asm` | - |
| `function/f_canlink.c` | `public/function_header/f_funcCode.h`<br>`public/function_header/f_dspcan.h`<br>`public/function_header/f_canlink.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_plc.h`<br>`public/function_header/f_p2p.h` |
| `function/f_comm.c` | `public/function_header/f_comm.h`<br>`public/function_header/f_p2p.h`<br>`public/function_header/f_ui.h` |
| `function/f_comm_DP.c` | `public/function_header/f_comm_DP.h`<br>`public/function_header/f_main.h` |
| `function/f_common.c` | `public/function_header/f_common.h` |
| `function/f_debug.c` | `public/function_header/f_funcCode.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_ui.h`<br>`public/function_header/f_debug.h` |
| `function/f_dspcan.c` | `public/28035_header/DSP28x_Project.h`<br>`public/function_header/f_funcCode.h`<br>`public/function_header/f_dspcan.h` |
| `function/f_eeprom.c` | `public/function_header/f_eeprom.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_table_eeprom2Fc.c` |
| `function/f_error.c` | `public/function_header/f_error.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_eeprom.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_ui.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_posCtrl.h`<br>`public/function_header/f_invPara.h` |
| `function/f_fcDeal.c` | `public/function_header/f_menu.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_eeprom.h`<br>`public/function_header/f_invPara.h`<br>`public/function_header/f_fcDeal.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h` |
| `function/f_frqSrc.c` | `public/function_header/f_frqSrc.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_ui.h`<br>`public/function_header/f_p2p.h`<br>`public/function_header/f_comm.h` |
| `function/f_frqSrc_pid.c` | `public/function_header/f_frqSrc.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_ui.h` |
| `function/f_funcCode.c` | `public/function_header/f_funcCode.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h` |
| `function/f_interface.c` | `public/function_header/f_funcCode.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_posCtrl.h` |
| `function/f_invPara.c` | `public/function_header/f_menu.h`<br>`public/function_header/f_eeprom.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_invPara.h` |
| `function/f_io.c` | `public/function_header/f_io.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_posCtrl.h`<br>`public/function_header/f_error.h` |
| `function/f_length.c` | `f_length.h` *(未解析: 编译器自带/库头)* |
| `function/f_main.c` | `public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_eeprom.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_ui.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_posCtrl.h`<br>`public/function_header/f_osc.h`<br>`public/function_header/f_canlink.h`<br>`public/function_header/f_p2p.h` |
| `function/f_menu.c` | `public/function_header/f_menu.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_ui.h`<br>`public/function_header/f_eeprom.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_posCtrl.h`<br>`public/function_header/f_invPara.h`<br>`public/function_header/f_fcDeal.h`<br>`public/function_header/f_p2p.h`<br>`public/function_header/f_funcCode_disp.c` |
| `function/f_modbus.c` | `public/function_header/f_comm.h` |
| `function/f_motorFc.c` | `public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_invPara.h` |
| `function/f_osc.c` | `public/function_header/f_osc.h`<br>`public/function_header/f_comm.h` |
| `function/f_p2p.c` | `public/function_header/f_ui.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_error.h`<br>`public/function_header/f_p2p.h` |
| `function/f_plc.c` | `public/function_header/f_funcCode.h`<br>`public/function_header/f_dspcan.h`<br>`public/function_header/f_canlink.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_plc.h` |
| `function/f_profibus.c` | `public/function_header/f_comm.h` |
| `function/f_runSrc.c` | `public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_ui.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_eeprom.h`<br>`public/function_header/f_posCtrl.h` |
| `function/f_runSrc_accDecFrq.c` | `public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_debug.h` |
| `function/f_torqueCtrl.c` | `public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_p2p.h` |
| `function/f_ui.c` | `public/function_header/f_ui.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_error.h` |
| `function/f_vf.c` | `public/function_header/f_main.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_io.h` |
| `function/include/f_canlink.h` | - |
| `function/include/f_comm.h` | `function/include/f_funcCode.h`<br>`function/include/f_main.h`<br>`function/include/f_menu.h`<br>`function/include/f_runSrc.h`<br>`function/include/f_eeprom.h`<br>`function/include/f_frqSrc.h` |
| `function/include/f_comm_DP.h` | `function/include/f_funcCode.h` |
| `function/include/f_common.h` | `function/include/f_funcCode.h` |
| `function/include/f_debug.h` | - |
| `function/include/f_dspcan.h` | - |
| `function/include/f_eeprom.h` | `function/include/f_funcCode.h` |
| `function/include/f_error.h` | `function/include/f_funcCode.h` |
| `function/include/f_fcDeal.h` | - |
| `function/include/f_frqSrc.h` | `function/include/f_funcCode.h` |
| `function/include/f_funcCode.h` | `public/28035_header/DSP28x_Project.h`<br>`DSP280x_Device.h` *(未解析: 编译器自带/库头)*<br>`DSP280x_Examples.h` *(未解析: 编译器自带/库头)*<br>`function/include/f_debug.h`<br>`function/include/f_interface.h`<br>`function/include/f_p2p.h` |
| `function/include/f_funcCode_disp.c` | - |
| `function/include/f_interface.h` | - |
| `function/include/f_invPara.h` | - |
| `function/include/f_io.h` | `function/include/f_funcCode.h` |
| `function/include/f_main.h` | `function/include/f_funcCode.h`<br>`function/include/f_error.h`<br>`function/include/f_common.h`<br>`function/include/f_canlink.h` |
| `function/include/f_menu.h` | `function/include/f_funcCode.h` |
| `function/include/f_osc.h` | `function/include/f_funcCode.h` |
| `function/include/f_p2p.h` | - |
| `function/include/f_plc.h` | - |
| `function/include/f_posCtrl.h` | `function/include/f_funcCode.h` |
| `function/include/f_runSrc.h` | `function/include/f_funcCode.h` |
| `function/include/f_table_eeprom2Fc.c` | - |
| `function/include/f_table_eeprom2Fc1.c` | - |
| `function/include/f_table_eeprom2Fc2.c` | - |
| `function/include/f_table_eeprom2Fc3.c` | - |
| `function/include/f_table_eeprom2Fc4.c` | - |
| `function/include/f_table_eeprom2Fc5.c` | - |
| `function/include/f_table_eeprom2Fc6---.c` | - |
| `function/include/f_ui.h` | `function/include/f_funcCode.h` |
| `function/main.c` | `public/motor_header/MotorInclude.h` |
| `function/z_func.c` | `public/function_header/f_funcCode.h`<br>`public/function_header/f_ui.h`<br>`public/function_header/f_io.h`<br>`public/function_header/f_eeprom.h`<br>`public/function_header/f_comm.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_frqSrc.h`<br>`public/function_header/f_main.h` |
| `motor/MotorCarrier.c` | `public/motor_header/MotorInclude.h`<br>`public/motor_header/MotorEncoder.h`<br>`public/motor_header/MotorPwmInclude.h` |
| `motor/MotorConstant.c` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h` |
| `motor/MotorCurrentTransform.c` | `public/motor_header/MotorCurrentTransform.h`<br>`public/motor_header/MotorInclude.h`<br>`public/motor_header/MotorEncoder.h` |
| `motor/MotorDataExchange.c` | `public/motor_header/MotorInclude.h` |
| `motor/MotorDebug.c` | `public/motor_header/MotorDefine.h`<br>`public/motor_header/SubPrgInclude.h`<br>`public/motor_header/MotorInclude.h` |
| `motor/MotorEncoder.c` | `public/motor_header/MotorEncoder.h`<br>`public/motor_header/MotorPmsmMain.h`<br>`public/motor_header/MotorPmsmParEst.h`<br>`public/28035_header/MotorVCInclude.h`<br>`public/motor_header/SubPrgInclude.h` |
| `motor/MotorImParEst.c` | `public/motor_header/MotorParaIDinclude.h` |
| `motor/MotorInfoCollect.c` | `public/motor_header/MotorInfoCollectInclude.h` |
| `motor/MotorInvProtect.c` | `public/28035_header/DSP2803x_EPwm_defines.h`<br>`public/motor_header/MotorInvProtectInclude.h` |
| `motor/MotorMain.c` | `public/motor_header/MotorInclude.h`<br>`public/motor_header/MotorDataExchange.h`<br>`public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h` |
| `motor/MotorPWM.c` | `public/motor_header/MotorPwmInclude.h` |
| `motor/MotorParChange.c` | `public/motor_header/MotorInclude.h` |
| `motor/MotorParEst.c` | `public/motor_header/MotorParaIDinclude.h`<br>`public/motor_header/MotorPmsmParEst.h`<br>`public/motor_header/MotorEncoder.h` |
| `motor/MotorPmsmMain.c` | `public/28035_header/MotorVCInclude.h`<br>`public/motor_header/MotorInclude.h` |
| `motor/MotorPmsmParEst.c` | `public/motor_header/MotorInclude.h` |
| `motor/MotorPublicCal.c` | `public/motor_header/MotorPublicCalInclude.h` |
| `motor/MotorSVC.c` | `public/28035_header/MotorVCInclude.h`<br>`public/motor_header/MotorSvcInclude.h`<br>`public/motor_header/MotorEncoder.h` |
| `motor/MotorSpeedCheck.c` | `public/motor_header/MotorSpeedCheceInclude.h` |
| `motor/MotorVCMain.c` | `public/28035_header/MotorVCInclude.h`<br>`public/motor_header/MotorEncoder.h`<br>`public/motor_header/MotorPmsmParEst.h`<br>`public/motor_header/MotorPmsmMain.h` |
| `motor/MotorVF.c` | `public/motor_header/MotorVFInclude.h`<br>`public/motor_header/MotorInclude.h` |
| `motor/MotorVar.c` | `public/motor_header/MotorInclude.h` |
| `motor/ZeroPosCtl.c` | `public/motor_header/MotorInclude.h`<br>`public/motor_header/ZeroPosCtl.h` |
| `public/28035_header/DSP2803x_Adc.h` | - |
| `public/28035_header/DSP2803x_BootVars.h` | - |
| `public/28035_header/DSP2803x_Cla.h` | - |
| `public/28035_header/DSP2803x_Cla_Defines.h` | - |
| `public/28035_header/DSP2803x_Comp.h` | - |
| `public/28035_header/DSP2803x_CpuTimers.h` | - |
| `public/28035_header/DSP2803x_DefaultISR.h` | - |
| `public/28035_header/DSP2803x_DevEmu.h` | - |
| `public/28035_header/DSP2803x_Device.h` | `public/28035_header/DSP2803x_Adc.h`<br>`public/28035_header/DSP2803x_BootVars.h`<br>`public/28035_header/DSP2803x_DevEmu.h`<br>`public/28035_header/DSP2803x_Cla.h`<br>`public/28035_header/DSP2803x_Comp.h`<br>`public/28035_header/DSP2803x_CpuTimers.h`<br>`public/28035_header/DSP2803x_ECan.h`<br>`public/28035_header/DSP2803x_ECap.h`<br>`public/28035_header/DSP2803x_EPwm.h`<br>`public/28035_header/DSP2803x_EQep.h`<br>`public/28035_header/DSP2803x_Gpio.h`<br>`public/28035_header/DSP2803x_I2c.h`<br>`public/28035_header/DSP2803x_Lin.h`<br>`public/28035_header/DSP2803x_NmiIntrupt.h`<br>`public/28035_header/DSP2803x_PieCtrl.h`<br>`public/28035_header/DSP2803x_PieVect.h`<br>`public/28035_header/DSP2803x_Spi.h`<br>`public/28035_header/DSP2803x_Sci.h`<br>`public/28035_header/DSP2803x_SysCtrl.h`<br>`public/28035_header/DSP2803x_XIntrupt.h` |
| `public/28035_header/DSP2803x_ECan.h` | - |
| `public/28035_header/DSP2803x_ECap.h` | - |
| `public/28035_header/DSP2803x_EPwm.h` | - |
| `public/28035_header/DSP2803x_EPwm_defines.h` | - |
| `public/28035_header/DSP2803x_EQep.h` | - |
| `public/28035_header/DSP2803x_Examples.h` | `public/28035_header/DSP2803x_GlobalPrototypes.h`<br>`public/28035_header/DSP2803x_EPwm_defines.h`<br>`public/28035_header/DSP2803x_I2C_defines.h`<br>`public/28035_header/DSP2803x_Cla_Defines.h`<br>`public/28035_header/DSP2803x_DefaultISR.h` |
| `public/28035_header/DSP2803x_GlobalPrototypes.h` | - |
| `public/28035_header/DSP2803x_Gpio.h` | - |
| `public/28035_header/DSP2803x_I2C_defines.h` | - |
| `public/28035_header/DSP2803x_I2c.h` | - |
| `public/28035_header/DSP2803x_Lin.h` | - |
| `public/28035_header/DSP2803x_NmiIntrupt.h` | - |
| `public/28035_header/DSP2803x_PieCtrl.h` | - |
| `public/28035_header/DSP2803x_PieVect.h` | - |
| `public/28035_header/DSP2803x_SWPrioritizedIsrLevels.h` | - |
| `public/28035_header/DSP2803x_Sci.h` | - |
| `public/28035_header/DSP2803x_Spi.h` | - |
| `public/28035_header/DSP2803x_SysCtrl.h` | - |
| `public/28035_header/DSP2803x_XIntrupt.h` | - |
| `public/28035_header/DSP28x_Project.h` | `public/28035_header/DSP2803x_Device.h`<br>`public/28035_header/DSP2803x_Examples.h` |
| `public/28035_header/IQmathLib.h` | `math.h` *(未解析: 编译器自带/库头)*<br>`stdlib.h` *(未解析: 编译器自带/库头)*<br>`limits.h` *(未解析: 编译器自带/库头)* |
| `public/28035_header/MotorVCInclude.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h`<br>`public/motor_header/SubPrgInclude.h`<br>`public/motor_header/MotorInclude.h` |
| `public/28035_header/SFO_V6.h` | - |
| `public/28035_header/SystemDefine.h` | `public/28035_header/DSP2803x_Device.h` |
| `public/function_header/f_canlink.h` | - |
| `public/function_header/f_comm.h` | `public/function_header/f_funcCode.h`<br>`public/function_header/f_main.h`<br>`public/function_header/f_menu.h`<br>`public/function_header/f_runSrc.h`<br>`public/function_header/f_eeprom.h`<br>`public/function_header/f_frqSrc.h` |
| `public/function_header/f_comm_DP.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_common.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_debug.h` | - |
| `public/function_header/f_dspcan.h` | - |
| `public/function_header/f_eeprom.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_error.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_fcDeal.h` | - |
| `public/function_header/f_frqSrc.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_funcCode.h` | `public/28035_header/DSP28x_Project.h`<br>`public/function_header/f_debug.h`<br>`public/function_header/f_interface.h`<br>`public/function_header/f_p2p.h` |
| `public/function_header/f_funcCode_disp.c` | - |
| `public/function_header/f_interface.h` | - |
| `public/function_header/f_invPara.h` | - |
| `public/function_header/f_io.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_main.h` | `public/function_header/f_funcCode.h`<br>`public/function_header/f_error.h`<br>`public/function_header/f_common.h`<br>`public/function_header/f_canlink.h` |
| `public/function_header/f_menu.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_osc.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_p2p.h` | - |
| `public/function_header/f_plc.h` | - |
| `public/function_header/f_posCtrl.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_runSrc.h` | `public/function_header/f_funcCode.h` |
| `public/function_header/f_table_eeprom2Fc.c` | - |
| `public/function_header/f_table_eeprom2Fc1.c` | - |
| `public/function_header/f_table_eeprom2Fc2.c` | - |
| `public/function_header/f_table_eeprom2Fc3.c` | - |
| `public/function_header/f_table_eeprom2Fc4.c` | - |
| `public/function_header/f_table_eeprom2Fc5.c` | - |
| `public/function_header/f_table_eeprom2Fc6---.c` | - |
| `public/function_header/f_ui.h` | `public/function_header/f_funcCode.h` |
| `public/motor_header/DSP2803x_Device.h` | `public/28035_header/DSP2803x_Adc.h`<br>`public/28035_header/DSP2803x_BootVars.h`<br>`public/28035_header/DSP2803x_DevEmu.h`<br>`public/28035_header/DSP2803x_Cla.h`<br>`public/28035_header/DSP2803x_Comp.h`<br>`public/28035_header/DSP2803x_CpuTimers.h`<br>`public/28035_header/DSP2803x_ECan.h`<br>`public/28035_header/DSP2803x_ECap.h`<br>`public/28035_header/DSP2803x_EPwm.h`<br>`public/28035_header/DSP2803x_EQep.h`<br>`public/28035_header/DSP2803x_Gpio.h`<br>`public/28035_header/DSP2803x_I2c.h`<br>`public/28035_header/DSP2803x_Lin.h`<br>`public/28035_header/DSP2803x_NmiIntrupt.h`<br>`public/28035_header/DSP2803x_PieCtrl.h`<br>`public/28035_header/DSP2803x_PieVect.h`<br>`public/28035_header/DSP2803x_Spi.h`<br>`public/28035_header/DSP2803x_Sci.h`<br>`public/28035_header/DSP2803x_SysCtrl.h`<br>`public/28035_header/DSP2803x_XIntrupt.h`<br>`public/28035_header/DSP2803x_EPwm_defines.h` |
| `public/motor_header/DataTypeDef.h` | - |
| `public/motor_header/MotorCurrentTransform.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/MotorDataExchange.h` | - |
| `public/motor_header/MotorDefine.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/MotorEncoder.h` | `public/motor_header/MotorInclude.h`<br>`public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/MotorInclude.h` | `public/motor_header/MotorCurrentTransform.h`<br>`public/motor_header/SubPrgInclude.h`<br>`public/motor_header/MotorVFInclude.h`<br>`public/motor_header/MotorVCInclude.h`<br>`public/motor_header/MotorSvcInclude.h`<br>`public/motor_header/MotorInvProtectInclude.h`<br>`public/motor_header/MotorPwmInclude.h`<br>`public/motor_header/MotorStructDefine.h`<br>`public/motor_header/MotorInfoCollectInclude.h`<br>`public/motor_header/MotorParaIDinclude.h`<br>`public/motor_header/MotorPublicCalInclude.h`<br>`public/motor_header/MotorSpeedCheceInclude.h`<br>`public/motor_header/MotorDataExchange.h`<br>`public/motor_header/MotorPmsmMain.h`<br>`public/motor_header/MotorEncoder.h`<br>`public/motor_header/MotorPmsmParEst.h` |
| `public/motor_header/MotorInfoCollectInclude.h` | `public/motor_header/MotorInclude.h` |
| `public/motor_header/MotorInvProtectInclude.h` | `public/motor_header/MotorInclude.h`<br>`public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/MotorParaIDinclude.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorInclude.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/MotorPmsmMain.h` | - |
| `public/motor_header/MotorPmsmParEst.h` | - |
| `public/motor_header/MotorPublicCalInclude.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorInclude.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/MotorPwmInclude.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorInclude.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/MotorSpeedCheceInclude.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorInclude.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/MotorStructDefine.h` | - |
| `public/motor_header/MotorSvcInclude.h` | `public/motor_header/MotorInclude.h` |
| `public/motor_header/MotorVCInclude.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h`<br>`public/motor_header/SubPrgInclude.h`<br>`public/motor_header/MotorInclude.h` |
| `public/motor_header/MotorVFInclude.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h`<br>`public/motor_header/SubPrgInclude.h`<br>`public/motor_header/MotorCurrentTransform.h` |
| `public/motor_header/SubPrgInclude.h` | `public/28035_header/SystemDefine.h`<br>`public/motor_header/MotorDefine.h` |
| `public/motor_header/ZeroPosCtl.h` | `public/motor_header/DataTypeDef.h` |
| `subprogram/ASMSubPrg.asm` | - |
| `subprogram/SubPrg.c` | `public/motor_header/MotorDefine.h`<br>`public/motor_header/SubPrgInclude.h` |
| `system/DSP2803x_CSMPasswords.asm` | - |
| `system/DSP2803x_CodeStartBranch.asm` | - |
| `system/DSP2803x_CpuTimers.c` | `public/28035_header/DSP2803x_Device.h` |
| `system/DSP2803x_GlobalVariableDefs.c` | `public/28035_header/DSP2803x_Device.h` |
| `system/main.c` | `public/motor_header/MotorInclude.h` |
| `system/mainInit_2803x.c` | `public/motor_header/MotorDefine.h`<br>`public/motor_header/MotorInclude.h` |

## 3. 头文件被谁包含 (反向依赖)

| 头文件 | 被包含次数 | 包含者 |
|---|---:|---|
| `public/motor_header/MotorInclude.h` | 24 | function/main.c<br>motor/MotorCarrier.c<br>motor/MotorCurrentTransform.c<br>motor/MotorDataExchange.c<br>motor/MotorDebug.c<br>motor/MotorMain.c<br>motor/MotorParChange.c<br>motor/MotorPmsmMain.c<br>motor/MotorPmsmParEst.c<br>motor/MotorVF.c<br>motor/MotorVar.c<br>motor/ZeroPosCtl.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorEncoder.h<br>public/motor_header/MotorInfoCollectInclude.h<br>public/motor_header/MotorInvProtectInclude.h<br>public/motor_header/MotorParaIDinclude.h<br>public/motor_header/MotorPublicCalInclude.h<br>public/motor_header/MotorPwmInclude.h<br>public/motor_header/MotorSpeedCheceInclude.h<br>public/motor_header/MotorSvcInclude.h<br>public/motor_header/MotorVCInclude.h<br>system/main.c<br>system/mainInit_2803x.c |
| `public/function_header/f_funcCode.h` | 20 | function/f_canlink.c<br>function/f_debug.c<br>function/f_dspcan.c<br>function/f_funcCode.c<br>function/f_interface.c<br>function/f_plc.c<br>function/z_func.c<br>public/function_header/f_comm.h<br>public/function_header/f_comm_DP.h<br>public/function_header/f_common.h<br>public/function_header/f_eeprom.h<br>public/function_header/f_error.h<br>public/function_header/f_frqSrc.h<br>public/function_header/f_io.h<br>public/function_header/f_main.h<br>public/function_header/f_menu.h<br>public/function_header/f_osc.h<br>public/function_header/f_posCtrl.h<br>public/function_header/f_runSrc.h<br>public/function_header/f_ui.h |
| `public/function_header/f_main.h` | 19 | function/f_comm_DP.c<br>function/f_debug.c<br>function/f_eeprom.c<br>function/f_error.c<br>function/f_fcDeal.c<br>function/f_frqSrc.c<br>function/f_frqSrc_pid.c<br>function/f_funcCode.c<br>function/f_interface.c<br>function/f_io.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_motorFc.c<br>function/f_runSrc.c<br>function/f_runSrc_accDecFrq.c<br>function/f_torqueCtrl.c<br>function/f_vf.c<br>function/z_func.c<br>public/function_header/f_comm.h |
| `public/function_header/f_runSrc.h` | 19 | function/f_debug.c<br>function/f_error.c<br>function/f_fcDeal.c<br>function/f_frqSrc.c<br>function/f_frqSrc_pid.c<br>function/f_funcCode.c<br>function/f_interface.c<br>function/f_io.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_motorFc.c<br>function/f_p2p.c<br>function/f_runSrc.c<br>function/f_runSrc_accDecFrq.c<br>function/f_torqueCtrl.c<br>function/f_ui.c<br>function/f_vf.c<br>function/z_func.c<br>public/function_header/f_comm.h |
| `public/function_header/f_frqSrc.h` | 18 | function/f_error.c<br>function/f_fcDeal.c<br>function/f_frqSrc.c<br>function/f_frqSrc_pid.c<br>function/f_funcCode.c<br>function/f_interface.c<br>function/f_io.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_motorFc.c<br>function/f_p2p.c<br>function/f_runSrc.c<br>function/f_runSrc_accDecFrq.c<br>function/f_torqueCtrl.c<br>function/f_ui.c<br>function/f_vf.c<br>function/z_func.c<br>public/function_header/f_comm.h |
| `public/motor_header/MotorDefine.h` | 17 | motor/MotorConstant.c<br>motor/MotorDebug.c<br>motor/MotorMain.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorCurrentTransform.h<br>public/motor_header/MotorDefine.h<br>public/motor_header/MotorEncoder.h<br>public/motor_header/MotorInvProtectInclude.h<br>public/motor_header/MotorParaIDinclude.h<br>public/motor_header/MotorPublicCalInclude.h<br>public/motor_header/MotorPwmInclude.h<br>public/motor_header/MotorSpeedCheceInclude.h<br>public/motor_header/MotorVCInclude.h<br>public/motor_header/MotorVFInclude.h<br>public/motor_header/SubPrgInclude.h<br>subprogram/SubPrg.c<br>system/mainInit_2803x.c |
| `public/function_header/f_comm.h` | 16 | function/f_canlink.c<br>function/f_comm.c<br>function/f_error.c<br>function/f_frqSrc.c<br>function/f_invPara.c<br>function/f_io.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_modbus.c<br>function/f_osc.c<br>function/f_p2p.c<br>function/f_plc.c<br>function/f_profibus.c<br>function/f_runSrc.c<br>function/f_ui.c<br>function/z_func.c |
| `public/function_header/f_menu.h` | 16 | function/f_eeprom.c<br>function/f_error.c<br>function/f_fcDeal.c<br>function/f_frqSrc.c<br>function/f_frqSrc_pid.c<br>function/f_interface.c<br>function/f_invPara.c<br>function/f_io.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_motorFc.c<br>function/f_p2p.c<br>function/f_runSrc.c<br>function/f_runSrc_accDecFrq.c<br>function/f_ui.c<br>public/function_header/f_comm.h |
| `public/function_header/f_io.h` | 15 | function/f_error.c<br>function/f_frqSrc.c<br>function/f_frqSrc_pid.c<br>function/f_funcCode.c<br>function/f_interface.c<br>function/f_io.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_motorFc.c<br>function/f_p2p.c<br>function/f_runSrc.c<br>function/f_torqueCtrl.c<br>function/f_ui.c<br>function/f_vf.c<br>function/z_func.c |
| `public/28035_header/SystemDefine.h` | 14 | motor/MotorConstant.c<br>motor/MotorMain.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorCurrentTransform.h<br>public/motor_header/MotorDefine.h<br>public/motor_header/MotorEncoder.h<br>public/motor_header/MotorInvProtectInclude.h<br>public/motor_header/MotorParaIDinclude.h<br>public/motor_header/MotorPublicCalInclude.h<br>public/motor_header/MotorPwmInclude.h<br>public/motor_header/MotorSpeedCheceInclude.h<br>public/motor_header/MotorVCInclude.h<br>public/motor_header/MotorVFInclude.h<br>public/motor_header/SubPrgInclude.h |
| `function/include/f_funcCode.h` | 13 | function/include/f_comm.h<br>function/include/f_comm_DP.h<br>function/include/f_common.h<br>function/include/f_eeprom.h<br>function/include/f_error.h<br>function/include/f_frqSrc.h<br>function/include/f_io.h<br>function/include/f_main.h<br>function/include/f_menu.h<br>function/include/f_osc.h<br>function/include/f_posCtrl.h<br>function/include/f_runSrc.h<br>function/include/f_ui.h |
| `public/function_header/f_ui.h` | 11 | function/f_comm.c<br>function/f_debug.c<br>function/f_error.c<br>function/f_frqSrc.c<br>function/f_frqSrc_pid.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_p2p.c<br>function/f_runSrc.c<br>function/f_ui.c<br>function/z_func.c |
| `public/function_header/f_eeprom.h` | 9 | function/f_eeprom.c<br>function/f_error.c<br>function/f_fcDeal.c<br>function/f_invPara.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_runSrc.c<br>function/z_func.c<br>public/function_header/f_comm.h |
| `public/28035_header/DSP2803x_Device.h` | 8 | function/DSP2803x_CpuTimers.c<br>function/DSP2803x_GlobalVariableDefs.c<br>function/DSP2803x_PieCtrl.c<br>function/DSP2803x_SysCtrl.c<br>public/28035_header/DSP28x_Project.h<br>public/28035_header/SystemDefine.h<br>system/DSP2803x_CpuTimers.c<br>system/DSP2803x_GlobalVariableDefs.c |
| `public/function_header/f_p2p.h` | 8 | function/f_canlink.c<br>function/f_comm.c<br>function/f_frqSrc.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_p2p.c<br>function/f_torqueCtrl.c<br>public/function_header/f_funcCode.h |
| `public/motor_header/MotorEncoder.h` | 7 | motor/MotorCarrier.c<br>motor/MotorCurrentTransform.c<br>motor/MotorEncoder.c<br>motor/MotorParEst.c<br>motor/MotorSVC.c<br>motor/MotorVCMain.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/SubPrgInclude.h` | 7 | motor/MotorDebug.c<br>motor/MotorEncoder.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorInclude.h<br>public/motor_header/MotorVCInclude.h<br>public/motor_header/MotorVFInclude.h<br>subprogram/SubPrg.c |
| `public/function_header/f_posCtrl.h` | 6 | function/f_error.c<br>function/f_interface.c<br>function/f_io.c<br>function/f_main.c<br>function/f_menu.c<br>function/f_runSrc.c |
| `public/function_header/f_error.h` | 5 | function/f_error.c<br>function/f_io.c<br>function/f_p2p.c<br>function/f_ui.c<br>public/function_header/f_main.h |
| `public/function_header/f_invPara.h` | 5 | function/f_error.c<br>function/f_fcDeal.c<br>function/f_invPara.c<br>function/f_menu.c<br>function/f_motorFc.c |
| `public/28035_header/DSP2803x_Examples.h` | 4 | function/DSP2803x_CpuTimers.c<br>function/DSP2803x_PieCtrl.c<br>function/DSP2803x_SysCtrl.c<br>public/28035_header/DSP28x_Project.h |
| `public/function_header/f_canlink.h` | 4 | function/f_canlink.c<br>function/f_main.c<br>function/f_plc.c<br>public/function_header/f_main.h |
| `public/motor_header/MotorPmsmParEst.h` | 4 | motor/MotorEncoder.c<br>motor/MotorParEst.c<br>motor/MotorVCMain.c<br>public/motor_header/MotorInclude.h |
| `public/28035_header/MotorVCInclude.h` | 4 | motor/MotorEncoder.c<br>motor/MotorPmsmMain.c<br>motor/MotorSVC.c<br>motor/MotorVCMain.c |
| `public/function_header/f_dspcan.h` | 3 | function/f_canlink.c<br>function/f_dspcan.c<br>function/f_plc.c |
| `public/function_header/f_debug.h` | 3 | function/f_debug.c<br>function/f_runSrc_accDecFrq.c<br>public/function_header/f_funcCode.h |
| `public/28035_header/DSP28x_Project.h` | 3 | function/f_dspcan.c<br>function/include/f_funcCode.h<br>public/function_header/f_funcCode.h |
| `public/motor_header/MotorPwmInclude.h` | 3 | motor/MotorCarrier.c<br>motor/MotorPWM.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/MotorCurrentTransform.h` | 3 | motor/MotorCurrentTransform.c<br>public/motor_header/MotorInclude.h<br>public/motor_header/MotorVFInclude.h |
| `public/motor_header/MotorPmsmMain.h` | 3 | motor/MotorEncoder.c<br>motor/MotorVCMain.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/MotorParaIDinclude.h` | 3 | motor/MotorImParEst.c<br>motor/MotorParEst.c<br>public/motor_header/MotorInclude.h |
| `public/28035_header/DSP2803x_EPwm_defines.h` | 3 | motor/MotorInvProtect.c<br>public/28035_header/DSP2803x_Examples.h<br>public/motor_header/DSP2803x_Device.h |
| `public/function_header/f_plc.h` | 2 | function/f_canlink.c<br>function/f_plc.c |
| `public/function_header/f_common.h` | 2 | function/f_common.c<br>public/function_header/f_main.h |
| `public/function_header/f_fcDeal.h` | 2 | function/f_fcDeal.c<br>function/f_menu.c |
| `public/function_header/f_osc.h` | 2 | function/f_main.c<br>function/f_osc.c |
| `public/motor_header/MotorInfoCollectInclude.h` | 2 | motor/MotorInfoCollect.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/MotorInvProtectInclude.h` | 2 | motor/MotorInvProtect.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/MotorDataExchange.h` | 2 | motor/MotorMain.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/MotorPublicCalInclude.h` | 2 | motor/MotorPublicCal.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/MotorSpeedCheceInclude.h` | 2 | motor/MotorSpeedCheck.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/MotorSvcInclude.h` | 2 | motor/MotorSVC.c<br>public/motor_header/MotorInclude.h |
| `public/motor_header/MotorVFInclude.h` | 2 | motor/MotorVF.c<br>public/motor_header/MotorInclude.h |
| `public/28035_header/DSP2803x_Adc.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_BootVars.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_DevEmu.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_Cla.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_Comp.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_CpuTimers.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_ECan.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_ECap.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_EPwm.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_EQep.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_Gpio.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_I2c.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_Lin.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_NmiIntrupt.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_PieCtrl.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_PieVect.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_Spi.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_Sci.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_SysCtrl.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/28035_header/DSP2803x_XIntrupt.h` | 2 | public/28035_header/DSP2803x_Device.h<br>public/motor_header/DSP2803x_Device.h |
| `public/function_header/f_comm_DP.h` | 1 | function/f_comm_DP.c |
| `public/function_header/f_table_eeprom2Fc.c` | 1 | function/f_eeprom.c |
| `public/function_header/f_funcCode_disp.c` | 1 | function/f_menu.c |
| `function/include/f_main.h` | 1 | function/include/f_comm.h |
| `function/include/f_menu.h` | 1 | function/include/f_comm.h |
| `function/include/f_runSrc.h` | 1 | function/include/f_comm.h |
| `function/include/f_eeprom.h` | 1 | function/include/f_comm.h |
| `function/include/f_frqSrc.h` | 1 | function/include/f_comm.h |
| `function/include/f_debug.h` | 1 | function/include/f_funcCode.h |
| `function/include/f_interface.h` | 1 | function/include/f_funcCode.h |
| `function/include/f_p2p.h` | 1 | function/include/f_funcCode.h |
| `function/include/f_error.h` | 1 | function/include/f_main.h |
| `function/include/f_common.h` | 1 | function/include/f_main.h |
| `function/include/f_canlink.h` | 1 | function/include/f_main.h |
| `public/motor_header/ZeroPosCtl.h` | 1 | motor/ZeroPosCtl.c |
| `public/28035_header/DSP2803x_GlobalPrototypes.h` | 1 | public/28035_header/DSP2803x_Examples.h |
| `public/28035_header/DSP2803x_I2C_defines.h` | 1 | public/28035_header/DSP2803x_Examples.h |
| `public/28035_header/DSP2803x_Cla_Defines.h` | 1 | public/28035_header/DSP2803x_Examples.h |
| `public/28035_header/DSP2803x_DefaultISR.h` | 1 | public/28035_header/DSP2803x_Examples.h |
| `public/function_header/f_interface.h` | 1 | public/function_header/f_funcCode.h |
| `public/motor_header/MotorVCInclude.h` | 1 | public/motor_header/MotorInclude.h |
| `public/motor_header/MotorStructDefine.h` | 1 | public/motor_header/MotorInclude.h |
| `public/motor_header/DataTypeDef.h` | 1 | public/motor_header/ZeroPosCtl.h |

## 4. 未解析的 include (编译器自带头 / 库头)

| 头文件 | 出现次数 |
|---|---:|
| `f_length.h` | 1 |
| `DSP280x_Device.h` | 1 |
| `DSP280x_Examples.h` | 1 |
| `math.h` | 1 |
| `stdlib.h` | 1 |
| `limits.h` | 1 |
