# 全局符号清单 (global_symbol_list)

说明: 由源码静态扫描得到 (已剥离注释)。`def` = 有定义的全局量, `extern` = 仅声明。

## 1. 全局变量定义 (按文件)

| 文件 | 符号 | 类型 | 属性 |
|---|---|---|---|
| `function/DSP2803x_GlobalVariableDefs.c` | `EmuKey` | `Uint16` | def |
| `function/DSP2803x_GlobalVariableDefs.c` | `EmuBMode` | `Uint16` | def |
| `function/DSP2803x_GlobalVariableDefs.c` | `Flash_CPUScaleFactor` | `Uint32` | def |
| `function/f_canlink.c` | `CanRxTxCon` | `Uint16` | def |
| `function/f_canlink.c` | `CanLinkChara` | `Uint32` | def |
| `function/f_canlink.c` | `sBaud` | `Uint16` | def static |
| `function/f_comm.c` | `commSendData` | `COMM_SEND_DATA` | def |
| `function/f_comm.c` | `commRcvData` | `COMM_RCV_DATA` | def |
| `function/f_comm.c` | `commType` | `Uint16` | def |
| `function/f_comm.c` | `commProtocol` | `Uint16` | def |
| `function/f_comm.c` | `commTicker` | `Uint32` | def |
| `function/f_comm.c` | `canLinkTicker` | `Uint32` | def |
| `function/f_comm.c` | `commRunCmd` | `Uint16` | def |
| `function/f_comm.c` | `aoComm` | `Uint16` | def |
| `function/f_comm.c` | `userPwdPass4Comm` | `Uint16` | def |
| `function/f_comm.c` | `companyPwdPass4Comm` | `Uint16` | def |
| `function/f_comm.c` | `groupHidePassComm` | `Uint16` | def |
| `function/f_comm.c` | `commParitys` | `Uint16` | def |
| `function/f_comm.c` | `commParitys` | `Uint16` | def |
| `function/f_comm.c` | `COMM_ERR_INDEX` | `Uint16` | def |
| `function/f_comm.c` | `protocolFunc` | `protocolDeal` | def |
| `function/f_comm_DP.c` | `sendData_DP` | `Uint16` | def |
| `function/f_comm_DP.c` | `rcvData_DP` | `Uint16` | def |
| `function/f_comm_DP.c` | `dpParameter` | `DP_PARAMETER` | def |
| `function/f_comm_DP.c` | `dataByteNum` | `Uint16` | def |
| `function/f_comm_DP.c` | `dataByteNums` | `Uint16` | def |
| `function/f_comm_DP.c` | `dpTest` | `Uint16` | def |
| `function/f_common.c` | `crc16Table` | `Uint16` | def |
| `function/f_common.c` | `crc16Table` | `Uint16` | def |
| `function/f_debug.c` | `scope0` | `Uint16` | def |
| `function/f_debug.c` | `scope1` | `Uint16` | def |
| `function/f_debug.c` | `scope2` | `Uint16` | def |
| `function/f_debug.c` | `scope3` | `Uint16` | def |
| `function/f_debug.c` | `scope4` | `Uint16` | def |
| `function/f_debug.c` | `scope5` | `Uint16` | def |
| `function/f_debug.c` | `scopeIndex` | `Uint16` | def |
| `function/f_debug.c` | `scopeInterval` | `Uint16` | def |
| `function/f_debug.c` | `frq` | `int32` | extern |
| `function/f_debug.c` | `frqRun` | `int32` | extern |
| `function/f_debug.c` | `frqTmp` | `int32` | extern |
| `function/f_debug.c` | `pcRef` | `int32` | extern |
| `function/f_debug.c` | `pcErrorDisp` | `Uint16` | extern |
| `function/f_debug.c` | `matlabDebugRsvd` | `Uint16` | def |
| `function/f_debug.c` | `mPosAct` | `int32` | extern |
| `function/f_debug.c` | `mVelAct` | `int32` | extern |
| `function/f_debug.c` | `sPosAct` | `int32` | extern |
| `function/f_debug.c` | `sVelAct` | `int32` | extern |
| `function/f_debug.c` | `pcRefDisp` | `Uint16` | extern |
| `function/f_debug.c` | `pcFdbDisp` | `Uint16` | extern |
| `function/f_debug.c` | `mPosActDisp` | `Uint16` | extern |
| `function/f_debug.c` | `sPosActDisp` | `Uint16` | extern |
| `function/f_debug.c` | `i` | `int16` | def |
| `function/f_dspcan.c` | `eCanBaud` | `CAN_BAUD` | def |
| `function/f_dspcan.c` | `eCanTranEnFlag` | `Uint32` | def |
| `function/f_dspcan.c` | `eCanReEnFlag` | `Uint32` | def |
| `function/f_eeprom.c` | `funcCodeOneWriteNum` | `Uint16` | def |
| `function/f_eeprom.c` | `funcCodeOneWriteIndex` | `Uint16` | def |
| `function/f_eeprom.c` | `funcCodeRwMode` | `Uint16` | def |
| `function/f_eeprom.c` | `funcCodeRwModeTmp` | `Uint16` | def |
| `function/f_eeprom.c` | `startIndexWriteSeries` | `Uint16` | def |
| `function/f_eeprom.c` | `endIndexWriteSeries` | `Uint16` | def |
| `function/f_eeprom.c` | `endIndexRwFuncCode` | `Uint16` | def |
| `function/f_eeprom.c` | `funcCodeRead` | `FUNCCODE_RW` | def |
| `function/f_eeprom.c` | `rwFuncCodeRepeatDelayTicker` | `Uint16` | def |
| `function/f_eeprom.c` | `fcNoAttri2Eeprom` | `Uint16` | extern |
| `function/f_error.c` | `errorEeprom` | `Uint16` | def |
| `function/f_error.c` | `errorOther` | `Uint16` | def |
| `function/f_error.c` | `errorOtherOld` | `Uint16` | def |
| `function/f_error.c` | `errorCode` | `Uint16` | def |
| `function/f_error.c` | `errorInfo` | `Uint16` | def |
| `function/f_error.c` | `bUv` | `Uint16` | def |
| `function/f_error.c` | `errAutoRstNum` | `Uint16` | def |
| `function/f_error.c` | `errorInfoFromMotor` | `Uint16` | def |
| `function/f_error.c` | `errorCodeOld` | `Uint16` | def |
| `function/f_error.c` | `errorInfoOld` | `Uint16` | def |
| `function/f_error.c` | `errorCodeFromMotorList` | `Uint16` | def |
| `function/f_error.c` | `parallelMasterSendErrorResetTicker` | `Uint16` | extern |
| `function/f_fcDeal.c` | `limitedByOtherCodeIndex` | `Uint16` | def |
| `function/f_fcDeal.c` | `limitedByOtherCodeIndexNum` | `Uint16` | def |
| `function/f_frqSrc.c` | `upDownFrq` | `int32` | def |
| `function/f_frqSrc.c` | `upDownFrqInit` | `Uint16` | def |
| `function/f_frqSrc.c` | `upDownFrqTmp` | `int32` | def |
| `function/f_frqSrc.c` | `uPDownFrqMax` | `int32` | def |
| `function/f_frqSrc.c` | `uPDownFrqMin` | `int32` | def |
| `function/f_frqSrc.c` | `uPDownFrqLower1` | `int32` | def |
| `function/f_frqSrc.c` | `uPDownFrqLower2` | `int32` | def |
| `function/f_frqSrc.c` | `plcStep` | `Uint16` | def |
| `function/f_frqSrc.c` | `plcTime` | `Uint32` | def |
| `function/f_frqSrc.c` | `bPlcEndOneLoop` | `Uint16` | def |
| `function/f_frqSrc.c` | `plcStepRemOld` | `Uint16` | def |
| `function/f_frqSrc.c` | `plcTimeRemOld` | `Uint32` | def |
| `function/f_frqSrc.c` | `frq` | `int32` | def |
| `function/f_frqSrc.c` | `frqTmp` | `int32` | def |
| `function/f_frqSrc.c` | `frqFrac` | `int32` | def |
| `function/f_frqSrc.c` | `frqTmpFrac` | `int32` | def |
| `function/f_frqSrc.c` | `frqDroop` | `int32` | def |
| `function/f_frqSrc.c` | `frqCurAimFrac` | `int32` | def |
| `function/f_frqSrc.c` | `frqAim` | `int32` | def |
| `function/f_frqSrc.c` | `frqAimTmp` | `int32` | def |
| `function/f_frqSrc.c` | `frqAimTmp0` | `int32` | def |
| `function/f_frqSrc.c` | `frqCurAim` | `int32` | def |
| `function/f_frqSrc.c` | `timeBench` | `Uint16` | def |
| `function/f_frqSrc.c` | `frqDigitalTmp` | `int32` | def |
| `function/f_frqSrc.c` | `upperFrq` | `Uint16` | def |
| `function/f_frqSrc.c` | `lowerFrq` | `Uint16` | def |
| `function/f_frqSrc.c` | `maxFrq` | `Uint16` | def |
| `function/f_frqSrc.c` | `benchFrq` | `Uint16` | def |
| `function/f_frqSrc.c` | `frqAiPu` | `int32` | def |
| `function/f_frqSrc.c` | `frqPuQ15` | `Uint16` | def |
| `function/f_frqSrc.c` | `frqCalcSrc` | `Uint16` | def |
| `function/f_frqSrc.c` | `frqCalcSrcOld` | `Uint16` | def |
| `function/f_frqSrc.c` | `plcStepOld` | `Uint16` | def |
| `function/f_frqSrc.c` | `bStopPlc` | `Uint16` | def |
| `function/f_frqSrc.c` | `bFrqDigital` | `Uint16` | def |
| `function/f_frqSrc.c` | `frqKeyUpDownDelta` | `Uint16` | def |
| `function/f_frqSrc.c` | `bAntiReverseRun` | `Uint16` | def |
| `function/f_frqSrc.c` | `runMode` | `Uint16` | def |
| `function/f_frqSrc.c` | `frqAimOld4Dir` | `int32` | def |
| `function/f_frqSrc.c` | `runCmdBind` | `Uint16` | def |
| `function/f_frqSrc.c` | `frqPlcType` | `Uint16` | def |
| `function/f_frqSrc.c` | `frqMax` | `int32` | def |
| `function/f_frqSrc.c` | `frqMin` | `int32` | def |
| `function/f_frqSrc_pid.c` | `pidFunc` | `PID_FUNC` | def |
| `function/f_frqSrc_pid.c` | `pidFuncRef` | `LINE_CHANGE_STRUCT` | def |
| `function/f_frqSrc_pid.c` | `pidLpf` | `LowPassFilter` | def |
| `function/f_frqSrc_pid.c` | `pidInitTicker` | `Uint32` | def |
| `function/f_frqSrc_pid.c` | `pidFdbLoseTicker` | `Uint32` | def |
| `function/f_frqSrc_pid.c` | `pidError` | `int32` | def |
| `function/f_frqSrc_pid.c` | `bPidFdbLose` | `Uint16` | def |
| `function/f_funcCode.c` | `funcCode` | `FUNCCODE_ALL` | def |
| `function/f_funcCode.c` | `funcCodeGradeCurMenuMode` | `Uint16` | def |
| `function/f_funcCode.c` | `funcCodeGradeAll` | `Uint16` | def |
| `function/f_funcCode.c` | `funcCodeGradeSum` | `Uint16` | def |
| `function/f_funcCode.c` | `funcCodeAttribute` | `FUNCCODE_ATTRIBUTE` | def |
| `function/f_funcCode.c` | `fcNoAttri2Eeprom` | `Uint16` | def |
| `function/f_interface.c` | `frq2Core` | `Uint16` | def |
| `function/f_interface.c` | `frqCurAim2Core` | `Uint16` | def |
| `function/f_interface.c` | `rsvdData` | `Uint16` | def |
| `function/f_interface.c` | `driveCoeffFrqFdb` | `Uint16` | def |
| `function/f_interface.c` | `vcSpdLoopKp1` | `Uint16` | def |
| `function/f_interface.c` | `vcSpdLoopKp2` | `Uint16` | def |
| `function/f_interface.c` | `jerkFf` | `Uint16` | def |
| `function/f_interface.c` | `vfSeparateVol` | `Uint16` | extern |
| `function/f_interface.c` | `speedMotor` | `Uint16` | def |
| `function/f_interface.c` | `motorRun` | `Uint16` | def |
| `function/f_interface.c` | `frqRun` | `int32` | def |
| `function/f_interface.c` | `frqVFRun` | `int32` | def |
| `function/f_interface.c` | `frqVFRunRemainder` | `int32` | def |
| `function/f_interface.c` | `outVoltage` | `Uint16` | def |
| `function/f_interface.c` | `outCurrent` | `Uint16` | def |
| `function/f_interface.c` | `generatrixVoltage` | `Uint16` | def |
| `function/f_interface.c` | `outPower` | `int16` | def |
| `function/f_interface.c` | `errorCodeFromMotor` | `Uint16` | def |
| `function/f_interface.c` | `errorsCodeFromMotor` | `Uint16` | def |
| `function/f_interface.c` | `motorErrInfor` | `Uint16` | def |
| `function/f_interface.c` | `outCurrentDispOld` | `Uint16` | def |
| `function/f_interface.c` | `gPhiRt` | `int16` | def |
| `function/f_interface.c` | `gPhiRtDisp` | `int16` | def |
| `function/f_interface.c` | `pmsmRotorPos` | `Uint16` | def |
| `function/f_interface.c` | `frqFdb` | `Uint16` | def |
| `function/f_interface.c` | `frqFdbDisp` | `Uint16` | def |
| `function/f_interface.c` | `frqFdbTmp` | `int32` | def |
| `function/f_interface.c` | `frqFdbFlag` | `Uint16` | def |
| `function/f_interface.c` | `torqueCurrent` | `Uint16` | def |
| `function/f_interface.c` | `enCoderPosition` | `Uint16` | def |
| `function/f_interface.c` | `ABZPos` | `Uint16` | def |
| `function/f_interface.c` | `ZCount` | `Uint16` | def |
| `function/f_interface.c` | `antiVibrateCoeff` | `Uint16` | def |
| `function/f_interface.c` | `currentOc` | `Uint16` | def |
| `function/f_interface.c` | `currentPu` | `Uint16` | def |
| `function/f_interface.c` | `spdLoopOut` | `Uint16` | def |
| `function/f_interface.c` | `softOcDoFlag` | `Uint16` | extern |
| `function/f_interface.c` | `errorInfoFromMotor` | `Uint16` | extern |
| `function/f_interface.c` | `motorCtrlTuneStatus` | `Uint16` | extern |
| `function/f_interface.c` | `currentPu` | `Uint16` | def |
| `function/f_interface.c` | `powerAddDec` | `Uint16` | def |
| `function/f_interface.c` | `aptpSetOrigin` | `Uint32` | extern |
| `function/f_invPara.c` | `ovVoltageInitValue` | `Uint16` | def |
| `function/f_invPara.c` | `motor1ParaIndex` | `Uint16` | def |
| `function/f_invPara.c` | `invTypeParaTableT380` | `INV_TYPE_RELATED_PARA` | def |
| `function/f_invPara.c` | `invTypeParaTableT220` | `INV_TYPE_RELATED_PARA` | def |
| `function/f_invPara.c` | `invTypeParaTableS220` | `INV_TYPE_RELATED_PARA` | def |
| `function/f_invPara.c` | `invTypeParaTableT480` | `INV_TYPE_RELATED_PARA` | def |
| `function/f_invPara.c` | `invTypeParaTableT690` | `INV_TYPE_RELATED_PARA` | def |
| `function/f_invPara.c` | `invTypeParaTableT1140` | `INV_TYPE_RELATED_PARA` | def |
| `function/f_invPara.c` | `pInvTypeParaTable` | `INV_TYPE_RELATED_PARA *` | def |
| `function/f_invPara.c` | `INV_TYPE_POINT_LIMIT_TABLE` | `Uint16` | def |
| `function/f_invPara.c` | `INV_TYPE_BIG_ACC_DEC_START_TABLE` | `Uint16` | def |
| `function/f_invPara.c` | `motorSnIndexIncToMotor1` | `Uint16` | def |
| `function/f_io.c` | `oCurrentCheckFlag` | `Uint16` | def |
| `function/f_io.c` | `aiLpf` | `LowPassFilter` | def |
| `function/f_io.c` | `aoFmpValue` | `int32` | def |
| `function/f_io.c` | `aoFmpMax` | `int32` | def |
| `function/f_io.c` | `pulseInSet` | `int16` | def |
| `function/f_io.c` | `pulseInFrq` | `Uint32` | def |
| `function/f_io.c` | `pulseInFrq1` | `Uint32` | def |
| `function/f_io.c` | `lineSpeed` | `Uint16` | def |
| `function/f_io.c` | `softOcDoFlag` | `Uint16` | def |
| `function/f_io.c` | `motorForeOT` | `Uint16` | def |
| `function/f_io.c` | `motorOT` | `Uint16` | def |
| `function/f_io.c` | `temperature` | `Uint16` | def |
| `function/f_io.c` | `tickerTempDeal` | `Uint16` | def |
| `function/f_io.c` | `tempSampleSum` | `Uint32` | def |
| `function/f_io.c` | `temperatureVoltage` | `Uint16` | def |
| `function/f_io.c` | `temperatureVoltageOrigin` | `Uint16` | def |
| `function/f_io.c` | `voltageTempPT100` | `Uint16` | def |
| `function/f_io.c` | `voltageTempPT1000` | `Uint16` | def |
| `function/f_io.c` | `diLogic` | `Uint32` | def |
| `function/f_io.c` | `vdiSrc` | `Uint16` | def |
| `function/f_io.c` | `vdiFcSet` | `Uint16` | def |
| `function/f_io.c` | `doFuncCal` | `DO_FUNC_CAL` | def |
| `function/f_io.c` | `doLogic` | `Uint16` | def |
| `function/f_length.c` | `servoRunStatus` | `Uint16` | def |
| `function/f_length.c` | `servoRunControl` | `SERVO_RUN_PARA_STRUCT` | def |
| `function/f_length.c` | `qepPulseCalALl` | `Uint32` | def |
| `function/f_length.c` | `qepPulsePLC` | `Uint32` | def |
| `function/f_length.c` | `servoRunStarFlag` | `Uint16` | def |
| `function/f_length.c` | `servoControlEnable` | `Uint16` | def |
| `function/f_length.c` | `pulseNote1` | `Uint32` | def |
| `function/f_length.c` | `pulseNote2` | `Uint32` | def |
| `function/f_length.c` | `pulseNote3` | `Uint32` | def |
| `function/f_length.c` | `pulseTriangleLeft` | `Uint32` | def |
| `function/f_length.c` | `timeLeft1` | `Uint16` | def |
| `function/f_length.c` | `timeLeft2` | `Uint16` | def |
| `function/f_length.c` | `frqSaveDis` | `Uint16` | def |
| `function/f_length.c` | `motorPulseAll` | `Uint32` | def |
| `function/f_length.c` | `servoRunResetFlag` | `Uint16` | def |
| `function/f_length.c` | `servoCalFlag` | `Uint16` | def |
| `function/f_length.c` | `servoRunDIFlag` | `Uint16` | def |
| `function/f_length.c` | `servoPulseRunToPLC` | `Uint32` | def |
| `function/f_length.c` | `servoOverFlag` | `Uint16` | def |
| `function/f_length.c` | `servoEndTime` | `Uint16` | def |
| `function/f_main.c` | `memoryValue` | `Uint16` | def |
| `function/f_main.c` | `dataDigit` | `Uint16` | def |
| `function/f_main.c` | `kk` | `Uint16` | def |
| `function/f_main.c` | `jj` | `Uint16` | def |
| `function/f_main.c` | `upper1` | `Uint16` | def |
| `function/f_main.c` | `lower1` | `Uint16` | def |
| `function/f_main.c` | `index` | `Uint16` | def |
| `function/f_main.c` | `allTime` | `Uint32` | extern |
| `function/f_main.c` | `upTimes` | `Uint32` | extern |
| `function/f_main.c` | `aiaoChckReadOK` | `Uint16` | def |
| `function/f_main.c` | `mainLoopTicker` | `Uint16` | def |
| `function/f_main.c` | `timeBase` | `Uint32` | def |
| `function/f_main.c` | `timeBase05msOld` | `Uint32` | def |
| `function/f_main.c` | `ticker05msFuncInternal` | `Uint32` | def |
| `function/f_main.c` | `ticker05msFuncIn` | `Uint32` | def |
| `function/f_main.c` | `ticker05msFuncInternalDisp` | `Uint16` | def |
| `function/f_main.c` | `ticker05msFuncEach` | `Uint32` | def |
| `function/f_main.c` | `ticker05msFuncEa` | `Uint32` | def |
| `function/f_main.c` | `ticker05msFuncEachDisp` | `Uint16` | def |
| `function/f_main.c` | `allTime` | `Uint32` | def |
| `function/f_main.c` | `upTimes` | `Uint32` | def |
| `function/f_main.c` | `maxTime` | `Uint32` | def |
| `function/f_main.c` | `minTime` | `Uint32` | def |
| `function/f_main.c` | `CODE_RUN_DISPLAY_UPDATE_TIME` | `Uint16` | def |
| `function/f_main.c` | `sizeOfGroup` | `Uint16` | def |
| `function/f_main.c` | `sizeOfGrade` | `Uint16` | def |
| `function/f_main.c` | `sizeOfAll` | `Uint16` | def |
| `function/f_main.c` | `sizeOfFuncCodeAttribute` | `Uint16` | def |
| `function/f_main.c` | `sizeOfEeprom` | `Uint16` | def |
| `function/f_main.c` | `sizeOfPara` | `Uint16` | def |
| `function/f_main.c` | `errorFuncCode` | `Uint16` | def |
| `function/f_main.c` | `eeprom2FcAutoGenerate` | `Uint16` | def |
| `function/f_main.c` | `fcNoAttri2Eeprom` | `Uint16` | extern |
| `function/f_main.c` | `sizeOfTable_eeprom2Fc` | `Uint16` | def |
| `function/f_main.c` | `maxEepromAddr` | `Uint16` | def |
| `function/f_main.c` | `append` | `Uint16` | def |
| `function/f_menu.c` | `decNumber` | `int16` | def |
| `function/f_menu.c` | `hexNumber` | `int16` | def |
| `function/f_menu.c` | `deltaK` | `int16` | def |
| `function/f_menu.c` | `userMenuModeFcIndex` | `int16` | def |
| `function/f_menu.c` | `frqSrcFuncIndex` | `Uint16` | def |
| `function/f_menu.c` | `diFuncIndex` | `Uint16` | def |
| `function/f_menu.c` | `menu3Number` | `Uint16` | def |
| `function/f_menu.c` | `menuPwdNumber` | `Uint16` | def |
| `function/f_menu.c` | `curFcDispDigits` | `Uint16` | def |
| `function/f_menu.c` | `superFactoryPass` | `Uint16` | def |
| `function/f_menu.c` | `groupHidePwdStatus` | `Uint16` | def |
| `function/f_menu.c` | `groupHideChkOkFlag` | `Uint16` | def |
| `function/f_menu.c` | `accDecFrqPrcFlag` | `Uint16` | def |
| `function/f_menu.c` | `bFrqDigitalDone4WaitDelay` | `Uint16` | def |
| `function/f_menu.c` | `frqDisp` | `Uint16` | def |
| `function/f_menu.c` | `frqAimDisp` | `Uint16` | def |
| `function/f_menu.c` | `frqPLCDisp` | `Uint16` | def |
| `function/f_menu.c` | `frqAimPLCDisp` | `Uint16` | def |
| `function/f_menu.c` | `pidFuncRefDisp` | `Uint16` | def |
| `function/f_menu.c` | `pidFuncFdbDisp` | `Uint16` | def |
| `function/f_menu.c` | `outVoltageDisp` | `Uint16` | def |
| `function/f_menu.c` | `outCurrentDisp` | `Uint16` | def |
| `function/f_menu.c` | `itDisp` | `Uint16` | def |
| `function/f_menu.c` | `loadSpeedDisp` | `Uint16` | def |
| `function/f_menu.c` | `currentOcDisp` | `Uint16` | def |
| `function/f_menu.c` | `pulseInFrqDisp` | `Uint16` | def |
| `function/f_menu.c` | `frqRunDisp` | `Uint16` | def |
| `function/f_menu.c` | `pcOriginDisp` | `Uint16` | def |
| `function/f_menu.c` | `frqXDisp` | `Uint16` | def |
| `function/f_menu.c` | `frqYDisp` | `Uint16` | def |
| `function/f_menu.c` | `torqueCurrentAct` | `Uint32` | def |
| `function/f_menu.c` | `menu` | `sysMenu` | def |
| `function/f_menu.c` | `checkMenuModeCmd` | `Uint16` | def |
| `function/f_menu.c` | `checkMenuModePara` | `Uint16` | def |
| `function/f_menu.c` | `checkMenuModeSerachNone` | `Uint16` | def |
| `function/f_motorFc.c` | `polePair` | `Uint16` | def |
| `function/f_motorFc.c` | `motorSnIndexNIncToMotor2` | `Uint16` | def |
| `function/f_motorFc.c` | `decNumber` | `int16` | extern |
| `function/f_osc.c` | `BautRateCfg` | `Uint16` | def static |
| `function/f_osc.c` | `OscCongData` | `DSP_OSC_CON_DATA` | def static |
| `function/f_osc.c` | `OscConFrameBuf` | `uint8` | def static |
| `function/f_osc.c` | `BaudCfg` | `uint8` | def static |
| `function/f_p2p.c` | `p2pData` | `P2P_DATA_STRUCT` | def |
| `function/f_plc.c` | `InvTrFastIndex` | `Uint16` | def |
| `function/f_plc.c` | `InvTrFastTab` | `Uint16` | def |
| `function/f_plc.c` | `InvTrSlowTab` | `Uint16` | def |
| `function/f_plc.c` | `InvRecFastTab` | `Uint16` | def |
| `function/f_plc.c` | `InvRecSlowTab` | `Uint16` | def |
| `function/f_profibus.c` | `dpFrqAim` | `Uint16` | def |
| `function/f_profibus.c` | `DPDataNum` | `Uint16` | def |
| `function/f_profibus.c` | `rcvRigthFlag` | `Uint16` | def |
| `function/f_profibus.c` | `DPRcvTest` | `Uint16` | def |
| `function/f_profibus.c` | `saveEepromIndex` | `Uint16` | def |
| `function/f_profibus.c` | `saveEepromFlag` | `Uint16` | def |
| `function/f_runSrc.c` | `pcRef` | `int32` | extern |
| `function/f_runSrc.c` | `pcFdb` | `int32` | extern |
| `function/f_runSrc.c` | `startProtectSrc` | `Uint16` | def |
| `function/f_runSrc.c` | `otherStopSrc` | `Uint16` | def |
| `function/f_runSrc.c` | `otherStopLowerSrc` | `Uint16` | def |
| `function/f_runSrc.c` | `runSrc` | `Uint16` | def |
| `function/f_runSrc.c` | `accFrqTime` | `Uint32` | def |
| `function/f_runSrc.c` | `decFrqTime` | `Uint32` | def |
| `function/f_runSrc.c` | `swingFrqLimit` | `Uint16` | def |
| `function/f_runSrc.c` | `setRunLostTime` | `Uint16` | def |
| `function/f_runSrc.c` | `setRunTimeAim` | `Uint16` | def |
| `function/f_runSrc.c` | `shuntFlag` | `Uint16` | def |
| `function/f_runSrc.c` | `accDecTimeSrcPlc` | `Uint16` | def |
| `function/f_runSrc.c` | `runDirPanelOld` | `Uint16` | def |
| `function/f_runSrc.c` | `tuneCmd` | `Uint16` | def |
| `function/f_runSrc.c` | `saveTuneDataFlag` | `Uint16` | def |
| `function/f_runSrc.c` | `spdLoopOut` | `Uint16` | extern |
| `function/f_runSrc.c` | `bFrqCurAimChg` | `Uint16` | extern |
| `function/f_runSrc.c` | `motorCtrlTuneStatus` | `Uint16` | def |
| `function/f_runSrc.c` | `accelArray` | `int32` | def |
| `function/f_runSrc.c` | `accelTickerArray` | `int32` | def |
| `function/f_runSrc.c` | `accel` | `int32` | def |
| `function/f_runSrc.c` | `accelDisp` | `Uint16` | def |
| `function/f_runSrc_accDecFrq.c` | `frqLine` | `LINE_CHANGE_STRUCT` | def |
| `function/f_runSrc_accDecFrq.c` | `frqCurAimOld` | `int32` | def |
| `function/f_runSrc_accDecFrq.c` | `accTimeOld` | `int32` | def |
| `function/f_runSrc_accDecFrq.c` | `decTimeOld` | `int32` | def |
| `function/f_runSrc_accDecFrq.c` | `bFrqCurAimChg` | `Uint16` | def |
| `function/f_torqueCtrl.c` | `upperTorque` | `int16` | def |
| `function/f_torqueCtrl.c` | `torqueAim` | `int16` | def |
| `function/f_torqueCtrl.c` | `torqueSetEthDp` | `Uint16` | def |
| `function/f_torqueCtrl.c` | `torqueUpperEthDp` | `Uint16` | def |
| `function/f_torqueCtrl.c` | `itDisp` | `Uint16` | extern |
| `function/f_torqueCtrl.c` | `torqurLine` | `LINE_CHANGE_STRUCT` | def |
| `function/f_ui.c` | `flashMaxSDSD` | `Uint16` | def |
| `function/f_ui.c` | `keySelStatus` | `Uint16` | def |
| `function/f_ui.c` | `keySelType` | `Uint16` | def |
| `function/f_ui.c` | `keySelTypeBak` | `Uint16` | def |
| `function/f_ui.c` | `keyChecking` | `Uint16` | def |
| `function/f_ui.c` | `onQuickTicker` | `Uint16` | def |
| `function/f_ui.c` | `onQuickTickerMax` | `Uint16` | def |
| `function/f_ui.c` | `onShiftPassViewTicker` | `Uint16` | def |
| `function/f_ui.c` | `onShiftPassViewTickerMax` | `Uint16` | def |
| `function/f_ui.c` | `factoryPwd` | `Uint16` | def |
| `function/f_ui.c` | `DISPLAY_CODE` | `Uint16` | def |
| `function/f_ui.c` | `LED_CODE` | `Uint16` | def |
| `function/f_ui.c` | `displayBuffer` | `Uint16` | def |
| `function/f_ui.c` | `scanStep` | `Uint16` | def |
| `function/f_ui.c` | `keyBordTestFlag` | `Uint16` | def |
| `function/f_ui.c` | `keyBordValue` | `Uint16` | def |
| `function/f_ui.c` | `keyPreCtrl` | `Uint16` | def |
| `function/f_ui.c` | `keyFunc` | `Uint16` | def |
| `function/f_ui.c` | `upDownDelta` | `Uint16` | def |
| `function/f_ui.c` | `keyAnimateTime` | `Uint16` | def |
| `function/f_ui.c` | `keyAnimateTime1` | `Uint16` | def |
| `function/f_ui.c` | `keyAnimateTimeAll` | `Uint16` | def |
| `function/f_ui.c` | `checkMenuModePara` | `Uint16` | extern |
| `function/f_ui.c` | `checkMenuModeCmd` | `Uint16` | extern |
| `function/f_vf.c` | `vfSeprateVolAim` | `Uint16` | def |
| `function/f_vf.c` | `vfSeparateVol` | `Uint16` | def |
| `function/f_vf.c` | `vfSeprateVolAccLine` | `LINE_CHANGE_STRUCT` | def |
| `function/f_vf.c` | `outVoltageDisp` | `Uint16` | extern |
| `function/include/f_canlink.h` | `CanRxTxCon` | `Uint16` | extern |
| `function/include/f_canlink.h` | `canLinkTicker` | `Uint32` | extern |
| `function/include/f_comm.h` | `commRunCmd` | `Uint16` | extern |
| `function/include/f_comm.h` | `commTicker` | `Uint32` | extern |
| `function/include/f_comm.h` | `aoComm` | `Uint16` | extern |
| `function/include/f_comm.h` | `sendFrame` | `Uint16` | extern |
| `function/include/f_comm.h` | `rcvFrame` | `Uint16` | extern |
| `function/include/f_comm.h` | `commReadData` | `Uint16` | extern |
| `function/include/f_comm.h` | `commProtocol` | `Uint16` | extern |
| `function/include/f_comm.h` | `commRcvData` | `COMM_RCV_DATA` | extern |
| `function/include/f_comm.h` | `commSendData` | `COMM_SEND_DATA` | extern |
| `function/include/f_comm_DP.h` | `dpParameter` | `DP_PARAMETER` | extern |
| `function/include/f_comm_DP.h` | `sendData_DP` | `Uint16` | extern |
| `function/include/f_comm_DP.h` | `rcvData_DP` | `Uint16` | extern |
| `function/include/f_debug.h` | `daValue` | `Uint16` | extern |
| `function/include/f_eeprom.h` | `funcCodeRead` | `FUNCCODE_RW` | extern |
| `function/include/f_eeprom.h` | `funcCodeOneWriteIndex` | `Uint16` | extern |
| `function/include/f_eeprom.h` | `funcCodeOneWriteNum` | `Uint16` | extern |
| `function/include/f_eeprom.h` | `rwFuncCodeRepeatDelayTicker` | `Uint16` | extern |
| `function/include/f_eeprom.h` | `funcCodeRwMode` | `Uint16` | extern |
| `function/include/f_eeprom.h` | `funcCodeRwModeTmp` | `Uint16` | extern |
| `function/include/f_eeprom.h` | `startIndexWriteSeries` | `Uint16` | extern |
| `function/include/f_eeprom.h` | `endIndexWriteSeries` | `Uint16` | extern |
| `function/include/f_eeprom.h` | `errorEeprom` | `Uint16` | extern |
| `function/include/f_error.h` | `motorErrInfor` | `Uint16` | extern |
| `function/include/f_error.h` | `errorCode` | `Uint16` | extern |
| `function/include/f_error.h` | `errorCodeOld` | `Uint16` | extern |
| `function/include/f_error.h` | `errAutoRstNum` | `Uint16` | extern |
| `function/include/f_error.h` | `bUv` | `Uint16` | extern |
| `function/include/f_error.h` | `errorOther` | `Uint16` | extern |
| `function/include/f_error.h` | `errorsCodeFromMotor` | `Uint16` | extern |
| `function/include/f_error.h` | `errorInfo` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `pidFunc` | `PID_FUNC` | extern |
| `function/include/f_frqSrc.h` | `frq` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqTmp` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqFrac` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqTmpFrac` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqCurAimFrac` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqDroop` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqAim` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqAiPu` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqAimTmp` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqAimTmp0` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqCurAim` | `int32` | extern |
| `function/include/f_frqSrc.h` | `frqKeyUpDownDelta` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `upperFrq` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `lowerFrq` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `maxFrq` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `benchFrq` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `frqPuQ15` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `uPDownFrqMax` | `int32` | extern |
| `function/include/f_frqSrc.h` | `uPDownFrqMin` | `int32` | extern |
| `function/include/f_frqSrc.h` | `upDownFrqInit` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `plcStep` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `plcTime` | `Uint32` | extern |
| `function/include/f_frqSrc.h` | `bPlcEndOneLoop` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `plcStepRemOld` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `plcTimeRemOld` | `Uint32` | extern |
| `function/include/f_frqSrc.h` | `timeBench` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `frqDigitalTmp` | `int32` | extern |
| `function/include/f_frqSrc.h` | `bFrqDigital` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `frqCalcSrcOld` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `bStopPlc` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `bAntiReverseRun` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `frqAimOld4Dir` | `int32` | extern |
| `function/include/f_frqSrc.h` | `runMode` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `dpFrqAim` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `frqCalcSrc` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `plcStepOld` | `Uint16` | extern |
| `function/include/f_frqSrc.h` | `upDownFrq` | `int32` | extern |
| `function/include/f_frqSrc.h` | `upDownFrqTmp` | `int32` | extern |
| `function/include/f_funcCode.h` | `funcCodeAttribute` | `FUNCCODE_ATTRIBUTE` | extern |
| `function/include/f_funcCode.h` | `funcCode` | `FUNCCODE_ALL` | extern |
| `function/include/f_funcCode.h` | `funcCodeGradeSum` | `Uint16` | extern |
| `function/include/f_funcCode.h` | `saveEepromIndex` | `Uint16` | extern |
| `function/include/f_funcCode.h` | `ovVoltageInitValue` | `Uint16` | extern |
| `function/include/f_funcCode.h` | `funcCodeGradeAll` | `Uint16` | extern |
| `function/include/f_funcCode.h` | `funcCodeGradeCurMenuMode` | `Uint16` | extern |
| `function/include/f_funcCode_disp.c` | `ticker05msFuncEachDisp` | `Uint16` | extern |
| `function/include/f_funcCode_disp.c` | `ticker05msFuncInternalDisp` | `Uint16` | extern |
| `function/include/f_funcCode_disp.c` | `vfSeparateVol` | `Uint16` | extern |
| `function/include/f_funcCode_disp.c` | `vfSeprateVolAim` | `Uint16` | extern |
| `function/include/f_funcCode_disp.c` | `temperature` | `Uint16` | extern |
| `function/include/f_funcCode_disp.c` | `ZCount` | `Uint16` | extern |
| `function/include/f_funcCode_disp.c` | `antiVibrateCoeff` | `Uint16` | extern |
| `function/include/f_funcCode_disp.c` | `stopDispIndex` | `Uint16` | def |
| `function/include/f_interface.h` | `gSendToMotor05MsDataBuff` | `int16` | extern |
| `function/include/f_interface.h` | `gSendToMotor2MsDataBuff` | `int16` | extern |
| `function/include/f_interface.h` | `gSendToFunctionDataBuff` | `int16` | extern |
| `function/include/f_interface.h` | `gRealTimeToFunctionDataBuff` | `int16` | extern |
| `function/include/f_interface.h` | `gParaIdToFunctionDataBuff` | `int16` | extern |
| `function/include/f_interface.h` | `frq2Core` | `Uint16` | extern |
| `function/include/f_interface.h` | `frqCurAim2Core` | `Uint16` | extern |
| `function/include/f_interface.h` | `errorCodeFromMotor` | `Uint16` | extern |
| `function/include/f_interface.h` | `currentOc` | `Uint16` | extern |
| `function/include/f_interface.h` | `generatrixVoltage` | `Uint16` | extern |
| `function/include/f_interface.h` | `outVoltage` | `Uint16` | extern |
| `function/include/f_interface.h` | `outCurrent` | `Uint16` | extern |
| `function/include/f_interface.h` | `outPower` | `int16` | extern |
| `function/include/f_interface.h` | `currentPu` | `Uint16` | extern |
| `function/include/f_interface.h` | `rsvdData` | `Uint16` | extern |
| `function/include/f_interface.h` | `frqRun` | `int32` | extern |
| `function/include/f_interface.h` | `frqVFRun` | `int32` | extern |
| `function/include/f_interface.h` | `frqVFRunRemainder` | `int32` | extern |
| `function/include/f_interface.h` | `frqFdb` | `Uint16` | extern |
| `function/include/f_interface.h` | `frqFdbDisp` | `Uint16` | extern |
| `function/include/f_interface.h` | `frqFdbTmp` | `int32` | extern |
| `function/include/f_interface.h` | `frqFdbFlag` | `Uint16` | extern |
| `function/include/f_interface.h` | `motorRun` | `Uint16` | extern |
| `function/include/f_interface.h` | `outCurrentDispOld` | `Uint16` | extern |
| `function/include/f_interface.h` | `enCoderPosition` | `Uint16` | extern |
| `function/include/f_interface.h` | `ABZPos` | `Uint16` | extern |
| `function/include/f_interface.h` | `gPhiRtDisp` | `int16` | extern |
| `function/include/f_interface.h` | `pmsmRotorPos` | `Uint16` | extern |
| `function/include/f_invPara.h` | `pInvTypeParaTable` | `INV_TYPE_RELATED_PARA *` | extern |
| `function/include/f_io.h` | `pulseInFrq` | `Uint32` | extern |
| `function/include/f_io.h` | `pulseInSet` | `int16` | extern |
| `function/include/f_io.h` | `lineSpeed` | `Uint16` | extern |
| `function/include/f_main.h` | `limitedByOtherCodeIndex` | `Uint16` | extern |
| `function/include/f_main.h` | `limitedByOtherCodeIndexNum` | `Uint16` | extern |
| `function/include/f_main.h` | `polePair` | `Uint16` | extern |
| `function/include/f_main.h` | `torqueCurrent` | `Uint16` | extern |
| `function/include/f_main.h` | `memoryValue` | `Uint16` | extern |
| `function/include/f_main.h` | `aiaoChckReadOK` | `Uint16` | extern |
| `function/include/f_menu.h` | `superFactoryPass` | `Uint16` | extern |
| `function/include/f_menu.h` | `menu` | `sysMenu` | extern |
| `function/include/f_menu.h` | `accDecFrqPrcFlag` | `Uint16` | extern |
| `function/include/f_menu.h` | `bFrqDigital` | `Uint16` | extern |
| `function/include/f_menu.h` | `bFrqDigitalDone4WaitDelay` | `Uint16` | extern |
| `function/include/f_menu.h` | `outCurrentDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `outCurrentDispView` | `Uint16` | extern |
| `function/include/f_menu.h` | `currentOcDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `frqAimDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `frqPLCDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `frqAimPLCDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `outVoltageDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `frqRunDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `pcOriginDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `itDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `frqDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `frqXDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `frqYDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `torqueCurrentAct` | `Uint32` | extern |
| `function/include/f_menu.h` | `limitDealData` | `LIMIT_DEAL_STRUCT` | extern |
| `function/include/f_menu.h` | `decNumber` | `int16` | extern |
| `function/include/f_menu.h` | `loadSpeedDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `pidFuncRefDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `pidFuncFdbDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `pulseInFrqDisp` | `Uint16` | extern |
| `function/include/f_menu.h` | `vfSeparateVol` | `Uint16` | extern |
| `function/include/f_menu.h` | `vfSeprateVolAim` | `Uint16` | extern |
| `function/include/f_menu.h` | `mainLoopTicker` | `Uint16` | extern |
| `function/include/f_p2p.h` | `p2pData` | `P2P_DATA_STRUCT` | extern |
| `function/include/f_posCtrl.h` | `frqPcOut` | `int32` | extern |
| `function/include/f_posCtrl.h` | `frqPcOutFrac` | `int32` | extern |
| `function/include/f_posCtrl.h` | `aptpRef` | `int32` | extern |
| `function/include/f_posCtrl.h` | `pCmdRef` | `int32` | extern |
| `function/include/f_posCtrl.h` | `pcRefStart` | `Uint32` | extern |
| `function/include/f_posCtrl.h` | `pcOrigin` | `Uint32` | extern |
| `function/include/f_posCtrl.h` | `aptpAbsZeroOk` | `Uint16` | extern |
| `function/include/f_posCtrl.h` | `aptpCurPos` | `int32` | extern |
| `function/include/f_posCtrl.h` | `ppr` | `Uint32` | extern |
| `function/include/f_posCtrl.h` | `frqAimPg` | `int32` | extern |
| `function/include/f_posCtrl.h` | `jerk` | `int16` | extern |
| `function/include/f_posCtrl.h` | `pcCurrentPulse` | `Uint16` | extern |
| `function/include/f_posCtrl.h` | `pcError` | `int32` | extern |
| `function/include/f_posCtrl.h` | `aptpSetDisp` | `Uint16` | extern |
| `function/include/f_posCtrl.h` | `pcErrorDisp` | `Uint16` | extern |
| `function/include/f_posCtrl.h` | `pulseInFrqPg2Disp` | `Uint16` | extern |
| `function/include/f_posCtrl.h` | `pCmdRefBeforeEGear` | `Uint16` | extern |
| `function/include/f_posCtrl.h` | `pCmdRefAfterEGear` | `Uint16` | extern |
| `function/include/f_posCtrl.h` | `bPcErrorOk` | `Uint16` | extern |
| `function/include/f_posCtrl.h` | `bPcErrorNear` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `accFrqTime` | `Uint32` | extern |
| `function/include/f_runSrc.h` | `decFrqTime` | `Uint32` | extern |
| `function/include/f_runSrc.h` | `startProtectSrc` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `otherStopSrc` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `runSrc` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `bDiAccDecTime` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `accDecTimeSrcPlc` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `runDirPanelOld` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `shuntFlag` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `setRunLostTime` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `setRunTimeAim` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `swingFrqLimit` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `tuneCmd` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `accel` | `int32` | extern |
| `function/include/f_runSrc.h` | `accelDisp` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `pcFdbDisp` | `Uint16` | extern |
| `function/include/f_runSrc.h` | `frqCurAimOld` | `int32` | extern |
| `function/include/f_runSrc.h` | `accTimeOld` | `int32` | extern |
| `function/include/f_runSrc.h` | `decTimeOld` | `int32` | extern |
| `function/include/f_runSrc.h` | `frqLine` | `LINE_CHANGE_STRUCT` | extern |
| `function/include/f_runSrc.h` | `upperTorque` | `int16` | extern |
| `function/include/f_runSrc.h` | `torqueAim` | `int16` | extern |
| `function/include/f_table_eeprom2Fc.c` | `eeprom2Fc` | `Uint16` | def |
| `function/include/f_table_eeprom2Fc1.c` | `eeprom2Fc` | `Uint16` | def |
| `function/include/f_table_eeprom2Fc2.c` | `eeprom2Fc` | `Uint16` | def |
| `function/include/f_table_eeprom2Fc3.c` | `eeprom2Fc` | `Uint16` | def |
| `function/include/f_table_eeprom2Fc4.c` | `eeprom2Fc` | `Uint16` | def |
| `function/include/f_table_eeprom2Fc5.c` | `eeprom2Fc` | `Uint16` | def |
| `function/include/f_table_eeprom2Fc6---.c` | `eeprom2Fc` | `int` | def |
| `function/include/f_ui.h` | `keyBordTestFlag` | `Uint16` | extern |
| `function/include/f_ui.h` | `keyBordValue` | `Uint16` | extern |
| `function/include/f_ui.h` | `DISPLAY_CODE` | `Uint16` | extern |
| `function/include/f_ui.h` | `LED_CODE` | `Uint16` | extern |
| `function/include/f_ui.h` | `scanStep` | `Uint16` | extern |
| `function/include/f_ui.h` | `displayBuffer` | `Uint16` | extern |
| `function/include/f_ui.h` | `upDownDelta` | `Uint16` | extern |
| `function/include/f_ui.h` | `factoryPwd` | `Uint16` | extern |
| `function/include/f_ui.h` | `keyFunc` | `Uint16` | extern |
| `function/include/f_ui.h` | `keyPreCtrl` | `Uint16` | extern |
| `function/include/f_ui.h` | `keySelStatus` | `Uint16` | extern |
| `function/z_func.c` | `gSendToMotor05MsDataBuff` | `int16` | def |
| `function/z_func.c` | `gSendToMotor2MsDataBuff` | `int16` | def |
| `function/z_func.c` | `gSendToFunctionDataBuff` | `int16` | def |
| `function/z_func.c` | `gParaIdToFunctionDataBuff` | `int16` | def |
| `function/z_func.c` | `gRealTimeToFunctionDataBuff` | `int16` | def |
| `function/z_func.c` | `sizeOfTable_eeprom2Fc` | `Uint16` | extern |
| `motor/MotorCarrier.c` | `gFcCal` | `FC_CAL_STRUCT` | def |
| `motor/MotorCarrier.c` | `gSynPWM` | `SYN_PWM_STRUCT` | def |
| `motor/MotorCarrier.c` | `gPhase` | `ANGLE_STRUCT` | def |
| `motor/MotorCarrier.c` | `gDeadBand` | `DEAD_BAND_STRUCT` | def |
| `motor/MotorDataExchange.c` | `gSendToMotor05MsDataBuff` | `int` | def |
| `motor/MotorDataExchange.c` | `gSendToMotor2MsDataBuff` | `int` | def |
| `motor/MotorDataExchange.c` | `gSendToFunctionDataBuff` | `int` | def |
| `motor/MotorDataExchange.c` | `gRealTimeToFunctionDataBuff` | `int` | def |
| `motor/MotorDataExchange.c` | `gParaIdToFunctionDataBuff` | `int` | def |
| `motor/MotorDataExchange.c` | `uReservedData` | `Uint` | def |
| `motor/MotorDataExchange.c` | `gSoftVersion` | `Uint` | def |
| `motor/MotorDataExchange.c` | `gIMTQ12_obs` | `MT_STRUCT_Q24` | extern |
| `motor/MotorDataExchange.c` | `gABFluxS_LowFilter` | `ALPHABETA_STRUCT` | extern |
| `motor/MotorDataExchange.c` | `gABFluxS_comp` | `ALPHABETA_STRUCT` | extern |
| `motor/MotorDebug.c` | `gDebugBuffer` | `Uint` | def |
| `motor/MotorDebug.c` | `gDebugPoint` | `Uint` | def |
| `motor/MotorDebug.c` | `gDebugFlag` | `int` | def |
| `motor/MotorDebug.c` | `saveIndexFunc` | `Ulong` | def |
| `motor/MotorDebug.c` | `saveIndexP` | `Ulong` | def |
| `motor/MotorDebug.c` | `saveCurveNum` | `Uint` | def |
| `motor/MotorDebug.c` | `saveRstMem` | `Uint` | def |
| `motor/MotorDebug.c` | `SaveTmCnt` | `Uint` | def |
| `motor/MotorDebug.c` | `startSave` | `int` | def |
| `motor/MotorDebug.c` | `pVD1` | `int *` | def |
| `motor/MotorDebug.c` | `pVD2` | `int *` | def |
| `motor/MotorDebug.c` | `pVD3` | `int *` | def |
| `motor/MotorDebug.c` | `pVD4` | `int *` | def |
| `motor/MotorEncoder.c` | `gPGData` | `PG_DATA_STRUCT` | def |
| `motor/MotorEncoder.c` | `gUVWPG` | `IPM_UVW_PG_STRUCT` | def |
| `motor/MotorEncoder.c` | `gPGDir` | `IPM_PG_DIR_STRUCT` | def |
| `motor/MotorEncoder.c` | `gFVCSpeed` | `FVC_SPEED_STRUCT` | def |
| `motor/MotorEncoder.c` | `gRotorTrans` | `ROTOR_TRANS_STRUCT` | def |
| `motor/MotorEncoder.c` | `gSpeedFilter` | `BURR_FILTER_STRUCT` | def |
| `motor/MotorEncoder.c` | `gSpeedLine` | `CUR_LINE_STRUCT_DEF` | def |
| `motor/MotorInfoCollect.c` | `gADC` | `ADC_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gUDC` | `UDC_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gCurSamp` | `IUVW_SAMPLING_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gIUVWQ12` | `UVW_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gIUVWQ24` | `UVW_STRUCT_Q24` | def |
| `motor/MotorInfoCollect.c` | `gIMTQ12` | `MT_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gIMTQ24` | `MT_STRUCT_Q24` | def |
| `motor/MotorInfoCollect.c` | `gIMTSetQ12` | `MT_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gIAlphBeta` | `ALPHABETA_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gIAlphBetaQ12` | `ALPHABETA_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gIAmpTheta` | `AMPTHETA_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gLineCur` | `LINE_CURRENT_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gExcursionInfo` | `CUR_EXCURSION_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gTemperature` | `TEMPLETURE_STRUCT` | def |
| `motor/MotorInfoCollect.c` | `gAI` | `AI_STRUCT` | def |
| `motor/MotorInvProtect.c` | `gOverLoad` | `OVER_LOAD_PROTECT` | def |
| `motor/MotorInvProtect.c` | `gPhaseLose` | `PHASE_LOSE_STRUCT` | def |
| `motor/MotorInvProtect.c` | `gInLose` | `INPUT_LOSE_STRUCT` | def |
| `motor/MotorInvProtect.c` | `gLoadLose` | `LOAD_LOSE_STRUCT` | def |
| `motor/MotorInvProtect.c` | `gFanCtrl` | `FAN_CTRL_STRUCT` | def |
| `motor/MotorInvProtect.c` | `gBuffResCnt` | `Ulong` | def |
| `motor/MotorInvProtect.c` | `gCBCProtect` | `CBC_PROTECT_STRUCT` | def |
| `motor/MotorPWM.c` | `gOutVolt` | `OUT_VOLT_STRUCT` | def |
| `motor/MotorPWM.c` | `gPWM` | `PWM_OUT_STRUCT` | def |
| `motor/MotorPWM.c` | `gRatio` | `Uint` | def |
| `motor/MotorPWM.c` | `EPWMFlagU` | `Uint` | def |
| `motor/MotorPWM.c` | `EPWMFlagV` | `Uint` | def |
| `motor/MotorPWM.c` | `EPWMFlagW` | `Uint` | def |
| `motor/MotorPWM.c` | `iTrig1_Cpwm` | `Uint` | def |
| `motor/MotorPWM.c` | `iTrig1_Dpwm` | `Uint` | def |
| `motor/MotorPWM.c` | `iTrig2_Pwm` | `Uint` | def |
| `motor/MotorPWM.c` | `iTrig3_Cpwm` | `Uint` | def |
| `motor/MotorPWM.c` | `iTrig3_Dpwm` | `Uint` | def |
| `motor/MotorParEst.c` | `gUVCoff` | `UV_BIAS_COFF_STRUCT` | def |
| `motor/MotorParEst.c` | `gMotorExtReg` | `MOTOR_EXTERN_STRUCT` | def |
| `motor/MotorParEst.c` | `gGetParVarable` | `MOTOR_PARA_EST` | def |
| `motor/MotorParEst.c` | `gRrLoIdentify` | `IDENTIFY_RRLO_VARIABLE` | def |
| `motor/MotorParEst.c` | `gLmIoIdentify` | `IDENTIFY_LMIO_VARIABLE` | def |
| `motor/MotorPmsmParEst.c` | `gIPMZero` | `IPM_ZERO_POS_STRUCT` | def |
| `motor/MotorPmsmParEst.c` | `gIPMInitPos` | `IPM_INITPOS_PULSE_STR` | def |
| `motor/MotorPmsmParEst.c` | `gEstBemf` | `PMSM_EST_BEMF` | def |
| `motor/MotorPmsmParEst.c` | `gPmParEst` | `PMSM_EST_PARAM_DATA` | def |
| `motor/MotorPublicCal.c` | `gDCBrake` | `DC_BRAKE_STRUCT` | def |
| `motor/MotorPublicCal.c` | `gBrake` | `BRAKE_CONTROL_STRUCT` | def |
| `motor/MotorPublicCal.c` | `gShortGnd` | `SHORT_GND_STRUCT` | def |
| `motor/MotorPublicCal.c` | `gLowPower` | `JUDGE_POWER_LOW` | def |
| `motor/MotorSVC.c` | `gFluxR` | `FLUX_STRUCT` | def |
| `motor/MotorSVC.c` | `gFluxS` | `FLUX_STRUCT` | def |
| `motor/MotorSVC.c` | `gCalFlux` | `SVC_FLUX_CAL_STRUCT` | def |
| `motor/MotorSVC.c` | `gSVCSpeed` | `SVC_SPEED_STRUCT` | def |
| `motor/MotorSVC.c` | `gABFluxS_LowFilter` | `ALPHABETA_STRUCT` | def |
| `motor/MotorSVC.c` | `gABFluxS_comp` | `ALPHABETA_STRUCT` | def |
| `motor/MotorSVC.c` | `gIMTQ24_obs` | `MT_STRUCT_Q24` | def |
| `motor/MotorSVC.c` | `gIMTQ12_obs` | `MT_STRUCT_Q24` | def |
| `motor/MotorSpeedCheck.c` | `gFeisu` | `FEISU_STRUCT` | def |
| `motor/MotorSpeedCheck.c` | `gFeisuNew` | `FEISU_STRUCT_NEW` | def |
| `motor/MotorSpeedCheck.c` | `gSpeedCheckPID` | `PID_STRUCT` | def |
| `motor/MotorSpeedCheck.c` | `gSpeedCheckPID1` | `PID_STRUCT` | def |
| `motor/MotorVCMain.c` | `gVCPar` | `VC_INFO_STRUCT` | def |
| `motor/MotorVCMain.c` | `gIMTSet` | `MT_STRUCT_Q24` | def |
| `motor/MotorVCMain.c` | `gIMTSetApply` | `MT_STRUCT_Q24` | def |
| `motor/MotorVCMain.c` | `gUMTSet` | `MT_STRUCT` | def |
| `motor/MotorVCMain.c` | `gUAmpTheta` | `AMPTHETA_STRUCT` | def |
| `motor/MotorVCMain.c` | `gImAcrQ24` | `PID32_STRUCT` | def |
| `motor/MotorVCMain.c` | `gItAcrQ24` | `PID32_STRUCT` | def |
| `motor/MotorVCMain.c` | `gAsr` | `ASR_STRUCT` | def |
| `motor/MotorVCMain.c` | `gVcCsrPara` | `VC_CSR_PARA` | def |
| `motor/MotorVCMain.c` | `gIMSetAcr` | `PID_STRUCT` | def |
| `motor/MotorVCMain.c` | `gWspid` | `PID_STRUCT` | def |
| `motor/MotorVCMain.c` | `gPWMVAlphBeta` | `MT_STRUCT` | def |
| `motor/MotorVCMain.c` | `gUdcLimitIt` | `UDC_LIMIT_IT_STRUCT` | def |
| `motor/MotorVCMain.c` | `gSvcVoltAdj` | `PID_STRUCT` | def |
| `motor/MotorVCMain.c` | `gABVoltSet` | `ALPHABETA_STRUCT` | def |
| `motor/MotorVCMain.c` | `SVC0Signal` | `SVC0_STRUCT` | def |
| `motor/MotorVF.c` | `gVFPar` | `VF_INFO_STRUCT` | def |
| `motor/MotorVF.c` | `gVFAutoVar` | `VF_AUTO_TORQUEBOOST_VAR` | def |
| `motor/MotorVF.c` | `gWsComp` | `VF_WS_COMP_STRUCT` | def |
| `motor/MotorVF.c` | `gVarAvr` | `VAR_AVR_STRUCT` | def |
| `motor/MotorVF.c` | `gVfOsc` | `OVER_SHOCK_STRUCT` | def |
| `motor/MotorVF.c` | `gOscAmp` | `THREE_ORDER_FILTER` | def |
| `motor/MotorVF.c` | `gVfOverUdc` | `VF_OVER_UDC_DAMP` | def |
| `motor/MotorVF.c` | `gOvCur2` | `OVER_CURRENT_DAMP2` | def |
| `motor/MotorVF.c` | `gVfFreq2` | `VF_FREQ_DEAL` | def |
| `motor/MotorVF.c` | `gOvCur` | `OVER_CURRENT_DAMP` | def |
| `motor/MotorVF.c` | `gOvUdc` | `OVER_UDC_CTL_STRUCT` | def |
| `motor/MotorVF.c` | `gHVfOscDamp` | `HVF_OSC_DAMP_STRUCT` | def |
| `motor/MotorVF.c` | `gHVfOscIndex` | `HVF_OSC_JUDGE_INDEX` | def |
| `motor/MotorVF.c` | `gHVfDeadBandCompOpt` | `HVF_DB_COMP_OPT` | def |
| `motor/MotorVF.c` | `gHVfCur` | `MT_STRUCT` | def |
| `motor/MotorVar.c` | `gInvInfo` | `INV_STRUCT` | def |
| `motor/MotorVar.c` | `gMotorInfo` | `MOTOR_STRUCT` | def |
| `motor/MotorVar.c` | `gMotorExtInfo` | `MOTOR_EXTERN_STRUCT` | def |
| `motor/MotorVar.c` | `gMotorExtPer` | `MOTOR_EXTERN_STRUCT` | def |
| `motor/MotorVar.c` | `gMainStatus` | `RUN_STATUS_STRUCT` | def |
| `motor/MotorVar.c` | `gMainCmd` | `BASE_COMMAND_STRUCT` | def |
| `motor/MotorVar.c` | `gExtendCmd` | `MAIN_COMMAND_EXTEND_UNION` | def |
| `motor/MotorVar.c` | `gSubCommand` | `SUB_COMMAND_UNION` | def |
| `motor/MotorVar.c` | `gCtrMotorType` | `CONTROL_MOTOR_TYPE_ENUM` | def |
| `motor/MotorVar.c` | `gPowerTrq` | `MOTOR_POWER_TORQUE` | def |
| `motor/MotorVar.c` | `gBasePar` | `BASE_PAR_STRUCT` | def |
| `motor/MotorVar.c` | `gComPar` | `COM_PAR_INFO_STRUCT` | def |
| `motor/MotorVar.c` | `gCpuTime` | `CPU_TIME_STRUCT` | def |
| `motor/MotorVar.c` | `gTestDataReceive` | `MOTOR_DEBUG_DATA_RECEIVE_STRUCT` | def |
| `motor/MotorVar.c` | `gTestDataDisplay` | `MOTOR_DEBUG_DATA_DISPLAY_STRUCT` | def |
| `motor/ZeroPosCtl.c` | `gZeroPos` | `ZERO_POS_STRUCT` | def |
| `public/28035_header/DSP2803x_BootVars.h` | `EmuKey` | `Uint16` | extern |
| `public/28035_header/DSP2803x_BootVars.h` | `EmuBMode` | `Uint16` | extern |
| `public/28035_header/DSP2803x_BootVars.h` | `Flash_CPUScaleFactor` | `Uint32` | extern |
| `public/28035_header/DSP2803x_GlobalPrototypes.h` | `RamfuncsLoadStart` | `Uint16` | extern |
| `public/28035_header/DSP2803x_GlobalPrototypes.h` | `RamfuncsLoadEnd` | `Uint16` | extern |
| `public/28035_header/DSP2803x_GlobalPrototypes.h` | `RamfuncsRunStart` | `Uint16` | extern |
| `public/28035_header/MotorVCInclude.h` | `gVCPar` | `VC_INFO_STRUCT` | extern |
| `public/28035_header/MotorVCInclude.h` | `gIMTSet` | `MT_STRUCT_Q24` | extern |
| `public/28035_header/MotorVCInclude.h` | `gIMTSetApply` | `MT_STRUCT_Q24` | extern |
| `public/28035_header/MotorVCInclude.h` | `gUMTSet` | `MT_STRUCT` | extern |
| `public/28035_header/MotorVCInclude.h` | `gImAcrQ24` | `PID32_STRUCT` | extern |
| `public/28035_header/MotorVCInclude.h` | `gItAcrQ24` | `PID32_STRUCT` | extern |
| `public/28035_header/MotorVCInclude.h` | `gABVoltSet` | `ALPHABETA_STRUCT` | extern |
| `public/function_header/f_canlink.h` | `CanRxTxCon` | `Uint16` | extern |
| `public/function_header/f_canlink.h` | `canLinkTicker` | `Uint32` | extern |
| `public/function_header/f_comm.h` | `commRunCmd` | `Uint16` | extern |
| `public/function_header/f_comm.h` | `commTicker` | `Uint32` | extern |
| `public/function_header/f_comm.h` | `aoComm` | `Uint16` | extern |
| `public/function_header/f_comm.h` | `sendFrame` | `Uint16` | extern |
| `public/function_header/f_comm.h` | `rcvFrame` | `Uint16` | extern |
| `public/function_header/f_comm.h` | `commReadData` | `Uint16` | extern |
| `public/function_header/f_comm.h` | `commProtocol` | `Uint16` | extern |
| `public/function_header/f_comm.h` | `commRcvData` | `COMM_RCV_DATA` | extern |
| `public/function_header/f_comm.h` | `commSendData` | `COMM_SEND_DATA` | extern |
| `public/function_header/f_comm_DP.h` | `dpParameter` | `DP_PARAMETER` | extern |
| `public/function_header/f_comm_DP.h` | `sendData_DP` | `Uint16` | extern |
| `public/function_header/f_comm_DP.h` | `rcvData_DP` | `Uint16` | extern |
| `public/function_header/f_debug.h` | `daValue` | `Uint16` | extern |
| `public/function_header/f_eeprom.h` | `funcCodeRead` | `FUNCCODE_RW` | extern |
| `public/function_header/f_eeprom.h` | `funcCodeOneWriteIndex` | `Uint16` | extern |
| `public/function_header/f_eeprom.h` | `funcCodeOneWriteNum` | `Uint16` | extern |
| `public/function_header/f_eeprom.h` | `rwFuncCodeRepeatDelayTicker` | `Uint16` | extern |
| `public/function_header/f_eeprom.h` | `funcCodeRwMode` | `Uint16` | extern |
| `public/function_header/f_eeprom.h` | `funcCodeRwModeTmp` | `Uint16` | extern |
| `public/function_header/f_eeprom.h` | `startIndexWriteSeries` | `Uint16` | extern |
| `public/function_header/f_eeprom.h` | `endIndexWriteSeries` | `Uint16` | extern |
| `public/function_header/f_eeprom.h` | `errorEeprom` | `Uint16` | extern |
| `public/function_header/f_error.h` | `motorErrInfor` | `Uint16` | extern |
| `public/function_header/f_error.h` | `errorCode` | `Uint16` | extern |
| `public/function_header/f_error.h` | `errorCodeOld` | `Uint16` | extern |
| `public/function_header/f_error.h` | `errAutoRstNum` | `Uint16` | extern |
| `public/function_header/f_error.h` | `bUv` | `Uint16` | extern |
| `public/function_header/f_error.h` | `errorOther` | `Uint16` | extern |
| `public/function_header/f_error.h` | `errorsCodeFromMotor` | `Uint16` | extern |
| `public/function_header/f_error.h` | `errorInfo` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `pidFunc` | `PID_FUNC` | extern |
| `public/function_header/f_frqSrc.h` | `frq` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqTmp` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqFrac` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqTmpFrac` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqCurAimFrac` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqDroop` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqAim` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqAiPu` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqAimTmp` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqAimTmp0` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqCurAim` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `frqKeyUpDownDelta` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `upperFrq` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `lowerFrq` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `maxFrq` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `benchFrq` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `frqPuQ15` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `uPDownFrqMax` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `uPDownFrqMin` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `upDownFrqInit` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `plcStep` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `plcTime` | `Uint32` | extern |
| `public/function_header/f_frqSrc.h` | `bPlcEndOneLoop` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `plcStepRemOld` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `plcTimeRemOld` | `Uint32` | extern |
| `public/function_header/f_frqSrc.h` | `timeBench` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `frqDigitalTmp` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `bFrqDigital` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `frqCalcSrcOld` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `bStopPlc` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `bAntiReverseRun` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `frqAimOld4Dir` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `runMode` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `dpFrqAim` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `frqCalcSrc` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `plcStepOld` | `Uint16` | extern |
| `public/function_header/f_frqSrc.h` | `upDownFrq` | `int32` | extern |
| `public/function_header/f_frqSrc.h` | `upDownFrqTmp` | `int32` | extern |
| `public/function_header/f_funcCode.h` | `funcCodeAttribute` | `FUNCCODE_ATTRIBUTE` | extern |
| `public/function_header/f_funcCode.h` | `funcCode` | `FUNCCODE_ALL` | extern |
| `public/function_header/f_funcCode.h` | `funcCodeGradeSum` | `Uint16` | extern |
| `public/function_header/f_funcCode.h` | `saveEepromIndex` | `Uint16` | extern |
| `public/function_header/f_funcCode.h` | `ovVoltageInitValue` | `Uint16` | extern |
| `public/function_header/f_funcCode.h` | `funcCodeGradeAll` | `Uint16` | extern |
| `public/function_header/f_funcCode.h` | `funcCodeGradeCurMenuMode` | `Uint16` | extern |
| `public/function_header/f_funcCode_disp.c` | `ticker05msFuncEachDisp` | `Uint16` | extern |
| `public/function_header/f_funcCode_disp.c` | `ticker05msFuncInternalDisp` | `Uint16` | extern |
| `public/function_header/f_funcCode_disp.c` | `vfSeparateVol` | `Uint16` | extern |
| `public/function_header/f_funcCode_disp.c` | `vfSeprateVolAim` | `Uint16` | extern |
| `public/function_header/f_funcCode_disp.c` | `temperature` | `Uint16` | extern |
| `public/function_header/f_funcCode_disp.c` | `ZCount` | `Uint16` | extern |
| `public/function_header/f_funcCode_disp.c` | `antiVibrateCoeff` | `Uint16` | extern |
| `public/function_header/f_funcCode_disp.c` | `stopDispIndex` | `Uint16` | def |
| `public/function_header/f_interface.h` | `gSendToMotor05MsDataBuff` | `int16` | extern |
| `public/function_header/f_interface.h` | `gSendToMotor2MsDataBuff` | `int16` | extern |
| `public/function_header/f_interface.h` | `gSendToFunctionDataBuff` | `int16` | extern |
| `public/function_header/f_interface.h` | `gRealTimeToFunctionDataBuff` | `int16` | extern |
| `public/function_header/f_interface.h` | `gParaIdToFunctionDataBuff` | `int16` | extern |
| `public/function_header/f_interface.h` | `frq2Core` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `frqCurAim2Core` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `errorCodeFromMotor` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `currentOc` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `generatrixVoltage` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `outVoltage` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `outCurrent` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `outPower` | `int16` | extern |
| `public/function_header/f_interface.h` | `currentPu` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `rsvdData` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `frqRun` | `int32` | extern |
| `public/function_header/f_interface.h` | `frqVFRun` | `int32` | extern |
| `public/function_header/f_interface.h` | `frqVFRunRemainder` | `int32` | extern |
| `public/function_header/f_interface.h` | `frqFdb` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `frqFdbDisp` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `frqFdbTmp` | `int32` | extern |
| `public/function_header/f_interface.h` | `frqFdbFlag` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `motorRun` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `outCurrentDispOld` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `enCoderPosition` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `ABZPos` | `Uint16` | extern |
| `public/function_header/f_interface.h` | `gPhiRtDisp` | `int16` | extern |
| `public/function_header/f_interface.h` | `pmsmRotorPos` | `Uint16` | extern |
| `public/function_header/f_invPara.h` | `pInvTypeParaTable` | `INV_TYPE_RELATED_PARA *` | extern |
| `public/function_header/f_io.h` | `pulseInFrq` | `Uint32` | extern |
| `public/function_header/f_io.h` | `pulseInSet` | `int16` | extern |
| `public/function_header/f_io.h` | `lineSpeed` | `Uint16` | extern |
| `public/function_header/f_main.h` | `limitedByOtherCodeIndex` | `Uint16` | extern |
| `public/function_header/f_main.h` | `limitedByOtherCodeIndexNum` | `Uint16` | extern |
| `public/function_header/f_main.h` | `polePair` | `Uint16` | extern |
| `public/function_header/f_main.h` | `torqueCurrent` | `Uint16` | extern |
| `public/function_header/f_main.h` | `memoryValue` | `Uint16` | extern |
| `public/function_header/f_main.h` | `aiaoChckReadOK` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `superFactoryPass` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `menu` | `sysMenu` | extern |
| `public/function_header/f_menu.h` | `accDecFrqPrcFlag` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `bFrqDigital` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `bFrqDigitalDone4WaitDelay` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `outCurrentDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `outCurrentDispView` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `currentOcDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `frqAimDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `frqPLCDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `frqAimPLCDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `outVoltageDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `frqRunDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `pcOriginDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `itDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `frqDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `frqXDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `frqYDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `torqueCurrentAct` | `Uint32` | extern |
| `public/function_header/f_menu.h` | `limitDealData` | `LIMIT_DEAL_STRUCT` | extern |
| `public/function_header/f_menu.h` | `decNumber` | `int16` | extern |
| `public/function_header/f_menu.h` | `loadSpeedDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `pidFuncRefDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `pidFuncFdbDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `pulseInFrqDisp` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `vfSeparateVol` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `vfSeprateVolAim` | `Uint16` | extern |
| `public/function_header/f_menu.h` | `mainLoopTicker` | `Uint16` | extern |
| `public/function_header/f_p2p.h` | `p2pData` | `P2P_DATA_STRUCT` | extern |
| `public/function_header/f_posCtrl.h` | `frqPcOut` | `int32` | extern |
| `public/function_header/f_posCtrl.h` | `frqPcOutFrac` | `int32` | extern |
| `public/function_header/f_posCtrl.h` | `aptpRef` | `int32` | extern |
| `public/function_header/f_posCtrl.h` | `pCmdRef` | `int32` | extern |
| `public/function_header/f_posCtrl.h` | `pcRefStart` | `Uint32` | extern |
| `public/function_header/f_posCtrl.h` | `pcOrigin` | `Uint32` | extern |
| `public/function_header/f_posCtrl.h` | `aptpAbsZeroOk` | `Uint16` | extern |
| `public/function_header/f_posCtrl.h` | `aptpCurPos` | `int32` | extern |
| `public/function_header/f_posCtrl.h` | `ppr` | `Uint32` | extern |
| `public/function_header/f_posCtrl.h` | `frqAimPg` | `int32` | extern |
| `public/function_header/f_posCtrl.h` | `jerk` | `int16` | extern |
| `public/function_header/f_posCtrl.h` | `pcCurrentPulse` | `Uint16` | extern |
| `public/function_header/f_posCtrl.h` | `pcError` | `int32` | extern |
| `public/function_header/f_posCtrl.h` | `aptpSetDisp` | `Uint16` | extern |
| `public/function_header/f_posCtrl.h` | `pcErrorDisp` | `Uint16` | extern |
| `public/function_header/f_posCtrl.h` | `pulseInFrqPg2Disp` | `Uint16` | extern |
| `public/function_header/f_posCtrl.h` | `pCmdRefBeforeEGear` | `Uint16` | extern |
| `public/function_header/f_posCtrl.h` | `pCmdRefAfterEGear` | `Uint16` | extern |
| `public/function_header/f_posCtrl.h` | `bPcErrorOk` | `Uint16` | extern |
| `public/function_header/f_posCtrl.h` | `bPcErrorNear` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `accFrqTime` | `Uint32` | extern |
| `public/function_header/f_runSrc.h` | `decFrqTime` | `Uint32` | extern |
| `public/function_header/f_runSrc.h` | `startProtectSrc` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `otherStopSrc` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `runSrc` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `bDiAccDecTime` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `accDecTimeSrcPlc` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `runDirPanelOld` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `shuntFlag` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `setRunLostTime` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `setRunTimeAim` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `swingFrqLimit` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `tuneCmd` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `accel` | `int32` | extern |
| `public/function_header/f_runSrc.h` | `accelDisp` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `pcFdbDisp` | `Uint16` | extern |
| `public/function_header/f_runSrc.h` | `frqCurAimOld` | `int32` | extern |
| `public/function_header/f_runSrc.h` | `accTimeOld` | `int32` | extern |
| `public/function_header/f_runSrc.h` | `decTimeOld` | `int32` | extern |
| `public/function_header/f_runSrc.h` | `frqLine` | `LINE_CHANGE_STRUCT` | extern |
| `public/function_header/f_runSrc.h` | `upperTorque` | `int16` | extern |
| `public/function_header/f_runSrc.h` | `torqueAim` | `int16` | extern |
| `public/function_header/f_table_eeprom2Fc.c` | `eeprom2Fc` | `Uint16` | def |
| `public/function_header/f_table_eeprom2Fc1.c` | `eeprom2Fc` | `Uint16` | def |
| `public/function_header/f_table_eeprom2Fc2.c` | `eeprom2Fc` | `Uint16` | def |
| `public/function_header/f_table_eeprom2Fc3.c` | `eeprom2Fc` | `Uint16` | def |
| `public/function_header/f_table_eeprom2Fc4.c` | `eeprom2Fc` | `Uint16` | def |
| `public/function_header/f_table_eeprom2Fc5.c` | `eeprom2Fc` | `Uint16` | def |
| `public/function_header/f_table_eeprom2Fc6---.c` | `eeprom2Fc` | `int` | def |
| `public/function_header/f_ui.h` | `keyBordTestFlag` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `keyBordValue` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `DISPLAY_CODE` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `LED_CODE` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `scanStep` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `displayBuffer` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `upDownDelta` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `factoryPwd` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `keyFunc` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `keyPreCtrl` | `Uint16` | extern |
| `public/function_header/f_ui.h` | `keySelStatus` | `Uint16` | extern |
| `public/motor_header/MotorInclude.h` | `gInvInfo` | `INV_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gCtrMotorType` | `CONTROL_MOTOR_TYPE_ENUM` | extern |
| `public/motor_header/MotorInclude.h` | `gMotorInfo` | `MOTOR_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gMotorExtInfo` | `MOTOR_EXTERN_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gMotorExtPer` | `MOTOR_EXTERN_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gMainStatus` | `RUN_STATUS_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gMainCmd` | `BASE_COMMAND_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gExtendCmd` | `MAIN_COMMAND_EXTEND_UNION` | extern |
| `public/motor_header/MotorInclude.h` | `gSubCommand` | `SUB_COMMAND_UNION` | extern |
| `public/motor_header/MotorInclude.h` | `gBasePar` | `BASE_PAR_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gComPar` | `COM_PAR_INFO_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gPowerTrq` | `MOTOR_POWER_TORQUE` | extern |
| `public/motor_header/MotorInclude.h` | `gCpuTime` | `CPU_TIME_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gTestDataReceive` | `MOTOR_DEBUG_DATA_RECEIVE_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `gTestDataDisplay` | `MOTOR_DEBUG_DATA_DISPLAY_STRUCT` | extern |
| `public/motor_header/MotorInclude.h` | `pVD1` | `int *` | extern |
| `public/motor_header/MotorInclude.h` | `pVD2` | `int *` | extern |
| `public/motor_header/MotorInclude.h` | `pVD3` | `int *` | extern |
| `public/motor_header/MotorInclude.h` | `pVD4` | `int *` | extern |
| `public/motor_header/MotorInclude.h` | `pVD5` | `int *` | extern |
| `public/motor_header/MotorInclude.h` | `pVD6` | `int *` | extern |
| `public/motor_header/MotorInclude.h` | `startSave` | `int` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `ISamp` | `I_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gADC` | `ADC_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gUDC` | `UDC_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gCurSamp` | `IUVW_SAMPLING_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gIUVWQ12` | `UVW_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gIUVWQ24` | `UVW_STRUCT_Q24` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gIAlphBeta` | `ALPHABETA_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gIAlphBetaQ12` | `ALPHABETA_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gIMTQ12` | `MT_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gIMTSetQ12` | `MT_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gIMTQ24` | `MT_STRUCT_Q24` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gIAmpTheta` | `AMPTHETA_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gLineCur` | `LINE_CURRENT_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gExcursionInfo` | `CUR_EXCURSION_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gTemperature` | `TEMPLETURE_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gAI` | `AI_STRUCT` | extern |
| `public/motor_header/MotorInfoCollectInclude.h` | `gWspid` | `PID_STRUCT` | extern |
| `public/motor_header/MotorInvProtectInclude.h` | `gOverLoad` | `OVER_LOAD_PROTECT` | extern |
| `public/motor_header/MotorInvProtectInclude.h` | `gPhaseLose` | `PHASE_LOSE_STRUCT` | extern |
| `public/motor_header/MotorInvProtectInclude.h` | `gInLose` | `INPUT_LOSE_STRUCT` | extern |
| `public/motor_header/MotorInvProtectInclude.h` | `gLoadLose` | `LOAD_LOSE_STRUCT` | extern |
| `public/motor_header/MotorInvProtectInclude.h` | `gFanCtrl` | `FAN_CTRL_STRUCT` | extern |
| `public/motor_header/MotorInvProtectInclude.h` | `gBuffResCnt` | `Ulong` | extern |
| `public/motor_header/MotorInvProtectInclude.h` | `gCBCProtect` | `CBC_PROTECT_STRUCT` | extern |
| `public/motor_header/MotorParaIDinclude.h` | `WaitCnt` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `IsSampleValue` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `CurrentMax` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `CurrentRatio` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `RrL07PulseOverSig` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `RrL0CycleCnt` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `SampleTimes` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `PwmCompareValue` | `Uint` | def |
| `public/motor_header/MotorParaIDinclude.h` | `UdcVoltage` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `RrAndRsMax` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `RrAndRsMin` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `LoMax` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `LoMin` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `RrAndRsAccValue` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `LoAccValue` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `WaitCnt` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `DestinationFreq` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `DataSavedNum` | `int` | def |
| `public/motor_header/MotorParaIDinclude.h` | `VFOvShock` | `Uint` | def |
| `public/motor_header/MotorParaIDinclude.h` | `lIsValue` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `LmAccValue` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `IoAccValue` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `lImAccValue` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `lIsAccValue` | `long` | def |
| `public/motor_header/MotorParaIDinclude.h` | `gUVCoff` | `UV_BIAS_COFF_STRUCT` | extern |
| `public/motor_header/MotorParaIDinclude.h` | `gGetParVarable` | `MOTOR_PARA_EST` | extern |
| `public/motor_header/MotorParaIDinclude.h` | `gMotorExtReg` | `MOTOR_EXTERN_STRUCT` | extern |
| `public/motor_header/MotorParaIDinclude.h` | `gRrLoIdentify` | `IDENTIFY_RRLO_VARIABLE` | extern |
| `public/motor_header/MotorParaIDinclude.h` | `gLmIoIdentify` | `IDENTIFY_LMIO_VARIABLE` | extern |
| `public/motor_header/MotorPublicCalInclude.h` | `gDCBrake` | `DC_BRAKE_STRUCT` | extern |
| `public/motor_header/MotorPublicCalInclude.h` | `gBrake` | `BRAKE_CONTROL_STRUCT` | extern |
| `public/motor_header/MotorPublicCalInclude.h` | `gShortGnd` | `SHORT_GND_STRUCT` | extern |
| `public/motor_header/MotorPublicCalInclude.h` | `gLowPower` | `JUDGE_POWER_LOW` | extern |
| `public/motor_header/MotorPwmInclude.h` | `gFcCal` | `FC_CAL_STRUCT` | extern |
| `public/motor_header/MotorPwmInclude.h` | `gPhase` | `ANGLE_STRUCT` | extern |
| `public/motor_header/MotorPwmInclude.h` | `gOutVolt` | `OUT_VOLT_STRUCT` | extern |
| `public/motor_header/MotorPwmInclude.h` | `gRatio` | `Uint` | extern |
| `public/motor_header/MotorPwmInclude.h` | `gPWM` | `PWM_OUT_STRUCT` | extern |
| `public/motor_header/MotorPwmInclude.h` | `gSynPWM` | `SYN_PWM_STRUCT` | extern |
| `public/motor_header/MotorPwmInclude.h` | `gDeadBand` | `DEAD_BAND_STRUCT` | extern |
| `public/motor_header/MotorSpeedCheceInclude.h` | `gFeisu` | `FEISU_STRUCT` | extern |
| `public/motor_header/MotorSpeedCheceInclude.h` | `gFeisuNew` | `FEISU_STRUCT_NEW` | extern |
| `public/motor_header/MotorSpeedCheceInclude.h` | `gSpeedCheckPID` | `PID_STRUCT` | extern |
| `public/motor_header/MotorSpeedCheceInclude.h` | `gSpeedCheckPID1` | `PID_STRUCT` | extern |
| `public/motor_header/MotorSvcInclude.h` | `gFluxR` | `FLUX_STRUCT` | extern |
| `public/motor_header/MotorSvcInclude.h` | `gFluxS` | `FLUX_STRUCT` | extern |
| `public/motor_header/MotorSvcInclude.h` | `gIMTQ24_obs` | `MT_STRUCT_Q24` | extern |
| `public/motor_header/MotorSvcInclude.h` | `gIMTQ12_obs` | `MT_STRUCT_Q24` | extern |
| `public/motor_header/MotorVCInclude.h` | `gVCPar` | `VC_INFO_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gIMTSet` | `MT_STRUCT_Q24` | extern |
| `public/motor_header/MotorVCInclude.h` | `gIMTSetApply` | `MT_STRUCT_Q24` | extern |
| `public/motor_header/MotorVCInclude.h` | `gUMTSet` | `MT_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gUAmpTheta` | `AMPTHETA_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gImAcrQ24` | `PID32_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gItAcrQ24` | `PID32_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gIMAcr` | `PID32_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gITAcr` | `PID32_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gAsr` | `ASR_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gVcCsrPara` | `VC_CSR_PARA` | extern |
| `public/motor_header/MotorVCInclude.h` | `gModifyws` | `MODIFYWS_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gPWMVAlphBeta` | `MT_STRUCT` | extern |
| `public/motor_header/MotorVCInclude.h` | `gUdcLimitIt` | `UDC_LIMIT_IT_STRUCT` | extern |
| `public/motor_header/MotorVFInclude.h` | `gVFPar` | `VF_INFO_STRUCT` | extern |
| `public/motor_header/MotorVFInclude.h` | `gVFAutoVar` | `VF_AUTO_TORQUEBOOST_VAR` | extern |
| `public/motor_header/MotorVFInclude.h` | `gVfOsc` | `OVER_SHOCK_STRUCT` | extern |
| `public/motor_header/MotorVFInclude.h` | `gWsComp` | `VF_WS_COMP_STRUCT` | extern |
| `public/motor_header/MotorVFInclude.h` | `gVarAvr` | `VAR_AVR_STRUCT` | extern |
| `public/motor_header/MotorVFInclude.h` | `gHVfCur` | `MT_STRUCT` | extern |
| `public/motor_header/MotorVFInclude.h` | `gOscAmp` | `THREE_ORDER_FILTER` | extern |
| `public/motor_header/MotorVFInclude.h` | `gOvUdc` | `OVER_UDC_CTL_STRUCT` | extern |
| `public/motor_header/MotorVFInclude.h` | `gHVfOscDamp` | `HVF_OSC_DAMP_STRUCT` | extern |
| `public/motor_header/MotorVFInclude.h` | `gHVfOscIndex` | `HVF_OSC_JUDGE_INDEX` | extern |
| `public/motor_header/MotorVFInclude.h` | `gHVfDeadBandCompOpt` | `HVF_DB_COMP_OPT` | extern |
| `public/motor_header/ZeroPosCtl.h` | `gZeroPos` | `ZERO_POS_STRUCT` | extern |
| `system/DSP2803x_GlobalVariableDefs.c` | `EmuKey` | `Uint16` | def |
| `system/DSP2803x_GlobalVariableDefs.c` | `EmuBMode` | `Uint16` | def |
| `system/DSP2803x_GlobalVariableDefs.c` | `Flash_CPUScaleFactor` | `Uint32` | def |
| `system/mainInit_2803x.c` | `prginRAM` | `COPY_TABLE` | extern |
| `system/mainInit_2803x.c` | `gErrorIntCnt` | `int` | def |

## 2. 同名符号跨文件出现

| 符号 | 出现文件 |
|---|---|
| `ABZPos` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `CanRxTxCon` | function/f_canlink.c<br>function/include/f_canlink.h<br>public/function_header/f_canlink.h |
| `DISPLAY_CODE` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `EmuBMode` | function/DSP2803x_GlobalVariableDefs.c<br>public/28035_header/DSP2803x_BootVars.h<br>system/DSP2803x_GlobalVariableDefs.c |
| `EmuKey` | function/DSP2803x_GlobalVariableDefs.c<br>public/28035_header/DSP2803x_BootVars.h<br>system/DSP2803x_GlobalVariableDefs.c |
| `Flash_CPUScaleFactor` | function/DSP2803x_GlobalVariableDefs.c<br>public/28035_header/DSP2803x_BootVars.h<br>system/DSP2803x_GlobalVariableDefs.c |
| `LED_CODE` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `ZCount` | function/f_interface.c<br>function/include/f_funcCode_disp.c<br>public/function_header/f_funcCode_disp.c |
| `accDecFrqPrcFlag` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `accDecTimeSrcPlc` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `accFrqTime` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `accTimeOld` | function/f_runSrc_accDecFrq.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `accel` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `accelDisp` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `aiaoChckReadOK` | function/f_main.c<br>function/include/f_main.h<br>public/function_header/f_main.h |
| `antiVibrateCoeff` | function/f_interface.c<br>function/include/f_funcCode_disp.c<br>public/function_header/f_funcCode_disp.c |
| `aoComm` | function/f_comm.c<br>function/include/f_comm.h<br>public/function_header/f_comm.h |
| `aptpAbsZeroOk` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `aptpCurPos` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `aptpRef` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `aptpSetDisp` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `bAntiReverseRun` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `bDiAccDecTime` | function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `bFrqCurAimChg` | function/f_runSrc.c<br>function/f_runSrc_accDecFrq.c |
| `bFrqDigital` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>function/include/f_menu.h<br>public/function_header/f_frqSrc.h<br>public/function_header/f_menu.h |
| `bFrqDigitalDone4WaitDelay` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `bPcErrorNear` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `bPcErrorOk` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `bPlcEndOneLoop` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `bStopPlc` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `bUv` | function/f_error.c<br>function/include/f_error.h<br>public/function_header/f_error.h |
| `benchFrq` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `canLinkTicker` | function/f_comm.c<br>function/include/f_canlink.h<br>public/function_header/f_canlink.h |
| `checkMenuModeCmd` | function/f_menu.c<br>function/f_ui.c |
| `checkMenuModePara` | function/f_menu.c<br>function/f_ui.c |
| `commProtocol` | function/f_comm.c<br>function/include/f_comm.h<br>public/function_header/f_comm.h |
| `commRcvData` | function/f_comm.c<br>function/include/f_comm.h<br>public/function_header/f_comm.h |
| `commReadData` | function/include/f_comm.h<br>public/function_header/f_comm.h |
| `commRunCmd` | function/f_comm.c<br>function/include/f_comm.h<br>public/function_header/f_comm.h |
| `commSendData` | function/f_comm.c<br>function/include/f_comm.h<br>public/function_header/f_comm.h |
| `commTicker` | function/f_comm.c<br>function/include/f_comm.h<br>public/function_header/f_comm.h |
| `currentOc` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `currentOcDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `currentPu` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `daValue` | function/include/f_debug.h<br>public/function_header/f_debug.h |
| `decFrqTime` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `decNumber` | function/f_menu.c<br>function/f_motorFc.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `decTimeOld` | function/f_runSrc_accDecFrq.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `displayBuffer` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `dpFrqAim` | function/f_profibus.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `dpParameter` | function/f_comm_DP.c<br>function/include/f_comm_DP.h<br>public/function_header/f_comm_DP.h |
| `eeprom2Fc` | function/include/f_table_eeprom2Fc.c<br>function/include/f_table_eeprom2Fc1.c<br>function/include/f_table_eeprom2Fc2.c<br>function/include/f_table_eeprom2Fc3.c<br>function/include/f_table_eeprom2Fc4.c<br>function/include/f_table_eeprom2Fc5.c<br>function/include/f_table_eeprom2Fc6---.c<br>public/function_header/f_table_eeprom2Fc.c<br>public/function_header/f_table_eeprom2Fc1.c<br>public/function_header/f_table_eeprom2Fc2.c<br>public/function_header/f_table_eeprom2Fc3.c<br>public/function_header/f_table_eeprom2Fc4.c<br>public/function_header/f_table_eeprom2Fc5.c<br>public/function_header/f_table_eeprom2Fc6---.c |
| `enCoderPosition` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `endIndexWriteSeries` | function/f_eeprom.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `errAutoRstNum` | function/f_error.c<br>function/include/f_error.h<br>public/function_header/f_error.h |
| `errorCode` | function/f_error.c<br>function/include/f_error.h<br>public/function_header/f_error.h |
| `errorCodeFromMotor` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `errorCodeOld` | function/f_error.c<br>function/include/f_error.h<br>public/function_header/f_error.h |
| `errorEeprom` | function/f_error.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `errorInfo` | function/f_error.c<br>function/include/f_error.h<br>public/function_header/f_error.h |
| `errorInfoFromMotor` | function/f_error.c<br>function/f_interface.c |
| `errorOther` | function/f_error.c<br>function/include/f_error.h<br>public/function_header/f_error.h |
| `errorsCodeFromMotor` | function/f_interface.c<br>function/include/f_error.h<br>public/function_header/f_error.h |
| `factoryPwd` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `fcNoAttri2Eeprom` | function/f_eeprom.c<br>function/f_funcCode.c<br>function/f_main.c |
| `frq` | function/f_debug.c<br>function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frq2Core` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqAiPu` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqAim` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqAimDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `frqAimOld4Dir` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqAimPLCDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `frqAimPg` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `frqAimTmp` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqAimTmp0` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqCalcSrc` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqCalcSrcOld` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqCurAim` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqCurAim2Core` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqCurAimFrac` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqCurAimOld` | function/f_runSrc_accDecFrq.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `frqDigitalTmp` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `frqDroop` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqFdb` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqFdbDisp` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqFdbFlag` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqFdbTmp` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqFrac` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqKeyUpDownDelta` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqLine` | function/f_runSrc_accDecFrq.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `frqPLCDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `frqPcOut` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `frqPcOutFrac` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `frqPuQ15` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqRun` | function/f_debug.c<br>function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqRunDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `frqTmp` | function/f_debug.c<br>function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqTmpFrac` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `frqVFRun` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqVFRunRemainder` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `frqXDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `frqYDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `funcCode` | function/f_funcCode.c<br>function/include/f_funcCode.h<br>public/function_header/f_funcCode.h |
| `funcCodeAttribute` | function/f_funcCode.c<br>function/include/f_funcCode.h<br>public/function_header/f_funcCode.h |
| `funcCodeGradeAll` | function/f_funcCode.c<br>function/include/f_funcCode.h<br>public/function_header/f_funcCode.h |
| `funcCodeGradeCurMenuMode` | function/f_funcCode.c<br>function/include/f_funcCode.h<br>public/function_header/f_funcCode.h |
| `funcCodeGradeSum` | function/f_funcCode.c<br>function/include/f_funcCode.h<br>public/function_header/f_funcCode.h |
| `funcCodeOneWriteIndex` | function/f_eeprom.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `funcCodeOneWriteNum` | function/f_eeprom.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `funcCodeRead` | function/f_eeprom.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `funcCodeRwMode` | function/f_eeprom.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `funcCodeRwModeTmp` | function/f_eeprom.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `gABFluxS_LowFilter` | motor/MotorDataExchange.c<br>motor/MotorSVC.c |
| `gABFluxS_comp` | motor/MotorDataExchange.c<br>motor/MotorSVC.c |
| `gABVoltSet` | motor/MotorVCMain.c<br>public/28035_header/MotorVCInclude.h |
| `gADC` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gAI` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gAsr` | motor/MotorVCMain.c<br>public/motor_header/MotorVCInclude.h |
| `gBasePar` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gBrake` | motor/MotorPublicCal.c<br>public/motor_header/MotorPublicCalInclude.h |
| `gBuffResCnt` | motor/MotorInvProtect.c<br>public/motor_header/MotorInvProtectInclude.h |
| `gCBCProtect` | motor/MotorInvProtect.c<br>public/motor_header/MotorInvProtectInclude.h |
| `gComPar` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gCpuTime` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gCtrMotorType` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gCurSamp` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gDCBrake` | motor/MotorPublicCal.c<br>public/motor_header/MotorPublicCalInclude.h |
| `gDeadBand` | motor/MotorCarrier.c<br>public/motor_header/MotorPwmInclude.h |
| `gExcursionInfo` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gExtendCmd` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gFanCtrl` | motor/MotorInvProtect.c<br>public/motor_header/MotorInvProtectInclude.h |
| `gFcCal` | motor/MotorCarrier.c<br>public/motor_header/MotorPwmInclude.h |
| `gFeisu` | motor/MotorSpeedCheck.c<br>public/motor_header/MotorSpeedCheceInclude.h |
| `gFeisuNew` | motor/MotorSpeedCheck.c<br>public/motor_header/MotorSpeedCheceInclude.h |
| `gFluxR` | motor/MotorSVC.c<br>public/motor_header/MotorSvcInclude.h |
| `gFluxS` | motor/MotorSVC.c<br>public/motor_header/MotorSvcInclude.h |
| `gGetParVarable` | motor/MotorParEst.c<br>public/motor_header/MotorParaIDinclude.h |
| `gHVfCur` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gHVfDeadBandCompOpt` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gHVfOscDamp` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gHVfOscIndex` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gIAlphBeta` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gIAlphBetaQ12` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gIAmpTheta` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gIMTQ12` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gIMTQ12_obs` | motor/MotorDataExchange.c<br>motor/MotorSVC.c<br>public/motor_header/MotorSvcInclude.h |
| `gIMTQ24` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gIMTQ24_obs` | motor/MotorSVC.c<br>public/motor_header/MotorSvcInclude.h |
| `gIMTSet` | motor/MotorVCMain.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorVCInclude.h |
| `gIMTSetApply` | motor/MotorVCMain.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorVCInclude.h |
| `gIMTSetQ12` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gIUVWQ12` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gIUVWQ24` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gImAcrQ24` | motor/MotorVCMain.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorVCInclude.h |
| `gInLose` | motor/MotorInvProtect.c<br>public/motor_header/MotorInvProtectInclude.h |
| `gInvInfo` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gItAcrQ24` | motor/MotorVCMain.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorVCInclude.h |
| `gLineCur` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gLmIoIdentify` | motor/MotorParEst.c<br>public/motor_header/MotorParaIDinclude.h |
| `gLoadLose` | motor/MotorInvProtect.c<br>public/motor_header/MotorInvProtectInclude.h |
| `gLowPower` | motor/MotorPublicCal.c<br>public/motor_header/MotorPublicCalInclude.h |
| `gMainCmd` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gMainStatus` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gMotorExtInfo` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gMotorExtPer` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gMotorExtReg` | motor/MotorParEst.c<br>public/motor_header/MotorParaIDinclude.h |
| `gMotorInfo` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gOscAmp` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gOutVolt` | motor/MotorPWM.c<br>public/motor_header/MotorPwmInclude.h |
| `gOvUdc` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gOverLoad` | motor/MotorInvProtect.c<br>public/motor_header/MotorInvProtectInclude.h |
| `gPWM` | motor/MotorPWM.c<br>public/motor_header/MotorPwmInclude.h |
| `gPWMVAlphBeta` | motor/MotorVCMain.c<br>public/motor_header/MotorVCInclude.h |
| `gParaIdToFunctionDataBuff` | function/include/f_interface.h<br>function/z_func.c<br>motor/MotorDataExchange.c<br>public/function_header/f_interface.h |
| `gPhase` | motor/MotorCarrier.c<br>public/motor_header/MotorPwmInclude.h |
| `gPhaseLose` | motor/MotorInvProtect.c<br>public/motor_header/MotorInvProtectInclude.h |
| `gPhiRtDisp` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `gPowerTrq` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gRatio` | motor/MotorPWM.c<br>public/motor_header/MotorPwmInclude.h |
| `gRealTimeToFunctionDataBuff` | function/include/f_interface.h<br>function/z_func.c<br>motor/MotorDataExchange.c<br>public/function_header/f_interface.h |
| `gRrLoIdentify` | motor/MotorParEst.c<br>public/motor_header/MotorParaIDinclude.h |
| `gSendToFunctionDataBuff` | function/include/f_interface.h<br>function/z_func.c<br>motor/MotorDataExchange.c<br>public/function_header/f_interface.h |
| `gSendToMotor05MsDataBuff` | function/include/f_interface.h<br>function/z_func.c<br>motor/MotorDataExchange.c<br>public/function_header/f_interface.h |
| `gSendToMotor2MsDataBuff` | function/include/f_interface.h<br>function/z_func.c<br>motor/MotorDataExchange.c<br>public/function_header/f_interface.h |
| `gShortGnd` | motor/MotorPublicCal.c<br>public/motor_header/MotorPublicCalInclude.h |
| `gSpeedCheckPID` | motor/MotorSpeedCheck.c<br>public/motor_header/MotorSpeedCheceInclude.h |
| `gSpeedCheckPID1` | motor/MotorSpeedCheck.c<br>public/motor_header/MotorSpeedCheceInclude.h |
| `gSubCommand` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gSynPWM` | motor/MotorCarrier.c<br>public/motor_header/MotorPwmInclude.h |
| `gTemperature` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gTestDataDisplay` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gTestDataReceive` | motor/MotorVar.c<br>public/motor_header/MotorInclude.h |
| `gUAmpTheta` | motor/MotorVCMain.c<br>public/motor_header/MotorVCInclude.h |
| `gUDC` | motor/MotorInfoCollect.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gUMTSet` | motor/MotorVCMain.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorVCInclude.h |
| `gUVCoff` | motor/MotorParEst.c<br>public/motor_header/MotorParaIDinclude.h |
| `gUdcLimitIt` | motor/MotorVCMain.c<br>public/motor_header/MotorVCInclude.h |
| `gVCPar` | motor/MotorVCMain.c<br>public/28035_header/MotorVCInclude.h<br>public/motor_header/MotorVCInclude.h |
| `gVFAutoVar` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gVFPar` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gVarAvr` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gVcCsrPara` | motor/MotorVCMain.c<br>public/motor_header/MotorVCInclude.h |
| `gVfOsc` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gWsComp` | motor/MotorVF.c<br>public/motor_header/MotorVFInclude.h |
| `gWspid` | motor/MotorVCMain.c<br>public/motor_header/MotorInfoCollectInclude.h |
| `gZeroPos` | motor/ZeroPosCtl.c<br>public/motor_header/ZeroPosCtl.h |
| `generatrixVoltage` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `itDisp` | function/f_menu.c<br>function/f_torqueCtrl.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `jerk` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `keyBordTestFlag` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `keyBordValue` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `keyFunc` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `keyPreCtrl` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `keySelStatus` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `limitDealData` | function/include/f_menu.h<br>public/function_header/f_menu.h |
| `limitedByOtherCodeIndex` | function/f_fcDeal.c<br>function/include/f_main.h<br>public/function_header/f_main.h |
| `limitedByOtherCodeIndexNum` | function/f_fcDeal.c<br>function/include/f_main.h<br>public/function_header/f_main.h |
| `lineSpeed` | function/f_io.c<br>function/include/f_io.h<br>public/function_header/f_io.h |
| `loadSpeedDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `lowerFrq` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `mainLoopTicker` | function/f_main.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `maxFrq` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `memoryValue` | function/f_main.c<br>function/include/f_main.h<br>public/function_header/f_main.h |
| `menu` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `motorCtrlTuneStatus` | function/f_interface.c<br>function/f_runSrc.c |
| `motorErrInfor` | function/f_interface.c<br>function/include/f_error.h<br>public/function_header/f_error.h |
| `motorRun` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `otherStopSrc` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `outCurrent` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `outCurrentDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `outCurrentDispOld` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `outCurrentDispView` | function/include/f_menu.h<br>public/function_header/f_menu.h |
| `outPower` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `outVoltage` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `outVoltageDisp` | function/f_menu.c<br>function/f_vf.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `ovVoltageInitValue` | function/f_invPara.c<br>function/include/f_funcCode.h<br>public/function_header/f_funcCode.h |
| `p2pData` | function/f_p2p.c<br>function/include/f_p2p.h<br>public/function_header/f_p2p.h |
| `pCmdRef` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pCmdRefAfterEGear` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pCmdRefBeforeEGear` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pInvTypeParaTable` | function/f_invPara.c<br>function/include/f_invPara.h<br>public/function_header/f_invPara.h |
| `pVD1` | motor/MotorDebug.c<br>public/motor_header/MotorInclude.h |
| `pVD2` | motor/MotorDebug.c<br>public/motor_header/MotorInclude.h |
| `pVD3` | motor/MotorDebug.c<br>public/motor_header/MotorInclude.h |
| `pVD4` | motor/MotorDebug.c<br>public/motor_header/MotorInclude.h |
| `pcCurrentPulse` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pcError` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pcErrorDisp` | function/f_debug.c<br>function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pcFdbDisp` | function/f_debug.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `pcOrigin` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pcOriginDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `pcRef` | function/f_debug.c<br>function/f_runSrc.c |
| `pcRefStart` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pidFunc` | function/f_frqSrc_pid.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `pidFuncFdbDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `pidFuncRefDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `plcStep` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `plcStepOld` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `plcStepRemOld` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `plcTime` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `plcTimeRemOld` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `pmsmRotorPos` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `polePair` | function/f_motorFc.c<br>function/include/f_main.h<br>public/function_header/f_main.h |
| `ppr` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pulseInFrq` | function/f_io.c<br>function/include/f_io.h<br>public/function_header/f_io.h |
| `pulseInFrqDisp` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `pulseInFrqPg2Disp` | function/include/f_posCtrl.h<br>public/function_header/f_posCtrl.h |
| `pulseInSet` | function/f_io.c<br>function/include/f_io.h<br>public/function_header/f_io.h |
| `rcvData_DP` | function/f_comm_DP.c<br>function/include/f_comm_DP.h<br>public/function_header/f_comm_DP.h |
| `rcvFrame` | function/include/f_comm.h<br>public/function_header/f_comm.h |
| `rsvdData` | function/f_interface.c<br>function/include/f_interface.h<br>public/function_header/f_interface.h |
| `runDirPanelOld` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `runMode` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `runSrc` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `rwFuncCodeRepeatDelayTicker` | function/f_eeprom.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `saveEepromIndex` | function/f_profibus.c<br>function/include/f_funcCode.h<br>public/function_header/f_funcCode.h |
| `scanStep` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `sendData_DP` | function/f_comm_DP.c<br>function/include/f_comm_DP.h<br>public/function_header/f_comm_DP.h |
| `sendFrame` | function/include/f_comm.h<br>public/function_header/f_comm.h |
| `setRunLostTime` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `setRunTimeAim` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `shuntFlag` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `sizeOfTable_eeprom2Fc` | function/f_main.c<br>function/z_func.c |
| `softOcDoFlag` | function/f_interface.c<br>function/f_io.c |
| `spdLoopOut` | function/f_interface.c<br>function/f_runSrc.c |
| `startIndexWriteSeries` | function/f_eeprom.c<br>function/include/f_eeprom.h<br>public/function_header/f_eeprom.h |
| `startProtectSrc` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `startSave` | motor/MotorDebug.c<br>public/motor_header/MotorInclude.h |
| `stopDispIndex` | function/include/f_funcCode_disp.c<br>public/function_header/f_funcCode_disp.c |
| `superFactoryPass` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `swingFrqLimit` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `temperature` | function/f_io.c<br>function/include/f_funcCode_disp.c<br>public/function_header/f_funcCode_disp.c |
| `ticker05msFuncEachDisp` | function/f_main.c<br>function/include/f_funcCode_disp.c<br>public/function_header/f_funcCode_disp.c |
| `ticker05msFuncInternalDisp` | function/f_main.c<br>function/include/f_funcCode_disp.c<br>public/function_header/f_funcCode_disp.c |
| `timeBench` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `torqueAim` | function/f_torqueCtrl.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `torqueCurrent` | function/f_interface.c<br>function/include/f_main.h<br>public/function_header/f_main.h |
| `torqueCurrentAct` | function/f_menu.c<br>function/include/f_menu.h<br>public/function_header/f_menu.h |
| `tuneCmd` | function/f_runSrc.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `uPDownFrqMax` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `uPDownFrqMin` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `upDownDelta` | function/f_ui.c<br>function/include/f_ui.h<br>public/function_header/f_ui.h |
| `upDownFrq` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `upDownFrqInit` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `upDownFrqTmp` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `upperFrq` | function/f_frqSrc.c<br>function/include/f_frqSrc.h<br>public/function_header/f_frqSrc.h |
| `upperTorque` | function/f_torqueCtrl.c<br>function/include/f_runSrc.h<br>public/function_header/f_runSrc.h |
| `vfSeparateVol` | function/f_interface.c<br>function/f_vf.c<br>function/include/f_funcCode_disp.c<br>function/include/f_menu.h<br>public/function_header/f_funcCode_disp.c<br>public/function_header/f_menu.h |
| `vfSeprateVolAim` | function/f_vf.c<br>function/include/f_funcCode_disp.c<br>function/include/f_menu.h<br>public/function_header/f_funcCode_disp.c<br>public/function_header/f_menu.h |

## 3. 函数清单 (按文件)

| 文件 | 函数 | 返回类型 | 属性 |
|---|---|---|---|
| `function/DSP2803x_CpuTimers.c` | `InitCpuTimers` | `void` | - |
| `function/DSP2803x_CpuTimers.c` | `ConfigCpuTimer` | `void` | - |
| `function/DSP2803x_PieCtrl.c` | `InitPieCtrl` | `void` | - |
| `function/DSP2803x_PieCtrl.c` | `EnableInterrupts` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `InitSysCtrl` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `InitFlash` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `ServiceDog` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `DisableDog` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `InitPll` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `InitPeripheralClocks` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `CsmUnlock` | `Uint16` | - |
| `function/DSP2803x_SysCtrl.c` | `IntOsc1Sel` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `IntOsc2Sel` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `XtalOscSel` | `void` | - |
| `function/DSP2803x_SysCtrl.c` | `ExtOscSel` | `void` | - |
| `function/f_canlink.c` | `InitCanlinkTran` | `void` | - |
| `function/f_canlink.c` | `CanlinkDataTran` | `Uint16` | - |
| `function/f_canlink.c` | `InitCanlinkRec` | `void` | - |
| `function/f_canlink.c` | `CanlinkDataRec` | `Uint16` | - |
| `function/f_canlink.c` | `InitCanlink` | `Uint16` | - |
| `function/f_canlink.c` | `CanlinkFun` | `void` | - |
| `function/f_canlink.c` | `CanLlinkDataDeal` | `void` | - |
| `function/f_canlink.c` | `CanLinkRemoteDeal` | `void` | - |
| `function/f_canlink.c` | `CanControlWriter` | `Uint16` | - |
| `function/f_canlink.c` | `CanControlRead` | `Uint16` | - |
| `function/f_canlink.c` | `CanLinkConDeal` | `void` | - |
| `function/f_canlink.c` | `CanHostTx` | `void` | - |
| `function/f_canlink.c` | `CanlinkFun` | `void` | - |
| `function/f_comm.c` | `SciDeal` | `void` | - |
| `function/f_comm.c` | `SCI_RXD_isr` | `void` | - |
| `function/f_comm.c` | `SCI_RXD_isr` | `void` | - |
| `function/f_comm.c` | `SCI_TXD_isr` | `void` | - |
| `function/f_comm.c` | `SCI_TXD_isr` | `void` | - |
| `function/f_comm.c` | `Lina_Level0_ISR` | `void` | - |
| `function/f_comm.c` | `Lina_Level1_ISR` | `void` | - |
| `function/f_comm.c` | `SciErrCheck` | `Uint16` | - |
| `function/f_comm.c` | `CommWrite` | `Uint16` | - |
| `function/f_comm.c` | `resetLinSci` | `void` | - |
| `function/f_comm.c` | `closeRTX` | `void` | - |
| `function/f_comm.c` | `setRxConfig` | `void` | - |
| `function/f_comm.c` | `setTxConfig` | `void` | - |
| `function/f_comm.c` | `commErrorDeal` | `void` | - |
| `function/f_comm.c` | `commStatusDeal` | `void` | - |
| `function/f_comm.c` | `CommDataReRcv` | `void` | - |
| `function/f_comm.c` | `CommDataSend` | `void` | - |
| `function/f_comm.c` | `InitSetScia` | `void` | - |
| `function/f_comm.c` | `UpdateSciFormat` | `void` | - |
| `function/f_comm.c` | `InitSciaGpio` | `void` | - |
| `function/f_comm.c` | `InitSetScia` | `void` | - |
| `function/f_comm.c` | `InitSciaGpio` | `void` | - |
| `function/f_comm.c` | `SciDeal` | `void` | - |
| `function/f_comm.c` | `UpdateSciFormat` | `void` | - |
| `function/f_comm.c` | `SCI_RXD_isr` | `void` | - |
| `function/f_comm.c` | `SCI_TXD_isr` | `void` | - |
| `function/f_comm.c` | `CommWrite` | `Uint16` | - |
| `function/f_comm.c` | `CommRead` | `Uint16` | - |
| `function/f_comm_DP.c` | `SCI_DP_RXD_isr` | `void` | - |
| `function/f_comm_DP.c` | `SCI_DP_TXD_isr` | `void` | - |
| `function/f_comm_DP.c` | `CommDpStartSend` | `inline` | - |
| `function/f_comm_DP.c` | `CommDpRcvDataDeal` | `void` | - |
| `function/f_comm_DP.c` | `CommDpSendDataDeal` | `void` | - |
| `function/f_comm_DP.c` | `SciDpDeal` | `void` | - |
| `function/f_comm_DP.c` | `ErrAgainSend` | `void` | - |
| `function/f_comm_DP.c` | `InitSetSciDp` | `void` | - |
| `function/f_comm_DP.c` | `UpdateSciDpFormat` | `void` | - |
| `function/f_comm_DP.c` | `InitSciaGpioDp` | `void` | - |
| `function/f_comm_DP.c` | `InitScibGpioDp` | `void` | - |
| `function/f_comm_DP.c` | `InitSciDpBaudRate` | `void` | - |
| `function/f_comm_DP.c` | `GetDataByteNum` | `void` | - |
| `function/f_comm_DP.c` | `DpDataDeal` | `void` | - |
| `function/f_comm_DP.c` | `InitScibGpioDp` | `void` | - |
| `function/f_comm_DP.c` | `InitSetSciDp` | `void` | - |
| `function/f_comm_DP.c` | `SciDpDeal` | `void` | - |
| `function/f_common.c` | `CrcValueByteCalc` | `Uint16` | - |
| `function/f_common.c` | `CrcValueWordCalc` | `Uint16` | - |
| `function/f_common.c` | `CrcValueByteCalc` | `Uint16` | - |
| `function/f_common.c` | `Reverse4Bit` | `Uint16` | - |
| `function/f_common.c` | `Reverse8Bit` | `Uint16` | - |
| `function/f_debug.c` | `BitAssign` | `void` | - |
| `function/f_debug.c` | `UniversalDebug` | `void` | - |
| `function/f_debug.c` | `UniversalDebug32` | `void` | - |
| `function/f_debug.c` | `PowerOnDebug` | `void` | - |
| `function/f_debug.c` | `InitDa` | `void` | - |
| `function/f_debug.c` | `UpdateDaBuffer` | `void` | - |
| `function/f_debug.c` | `DaOut` | `void` | - |
| `function/f_dspcan.c` | `InitdspECan` | `Uint16` | - |
| `function/f_dspcan.c` | `InitTranMbox` | `void` | - |
| `function/f_dspcan.c` | `InitRecMbox` | `void` | - |
| `function/f_dspcan.c` | `eCanDataTran` | `Uint16` | - |
| `function/f_dspcan.c` | `eCanDataRec` | `Uint16` | - |
| `function/f_eeprom.c` | `ReadFuncCode` | `Uint16` | - |
| `function/f_eeprom.c` | `WriteFuncCode` | `Uint16` | - |
| `function/f_eeprom.c` | `EepromDeal` | `void` | - |
| `function/f_eeprom.c` | `EepromOperateTimeDeal` | `void` | - |
| `function/f_eeprom.c` | `InitI2CGpio` | `void` | - |
| `function/f_eeprom.c` | `InitSetI2ca` | `void` | - |
| `function/f_eeprom.c` | `I2cDealBeforeInit` | `void` | - |
| `function/f_eeprom.c` | `I2CNoACK` | `void` | - |
| `function/f_eeprom.c` | `I2CStop` | `void` | - |
| `function/f_eeprom.c` | `I2CRcvByte` | `void` | - |
| `function/f_eeprom.c` | `GetFcIndexFromEepromIndex` | `Uint16` | - |
| `function/f_eeprom.c` | `GetEepromIndexFromFcIndex` | `Uint16` | - |
| `function/f_error.c` | `ErrorDeal` | `void` | - |
| `function/f_error.c` | `UpdateError` | `void` | - |
| `function/f_error.c` | `getMotorErrorInfo` | `Uint16` | - |
| `function/f_error.c` | `getMotorError` | `void` | - |
| `function/f_error.c` | `GetNewError` | `void` | - |
| `function/f_error.c` | `GetNewErrorAttribute` | `void` | - |
| `function/f_error.c` | `ErrorSceneSave` | `void` | - |
| `function/f_error.c` | `ErrorReset` | `void` | - |
| `function/f_error.c` | `GetErrorResetDeal` | `void` | - |
| `function/f_error.c` | `AutoResetErrorDeal` | `Uint16` | - |
| `function/f_error.c` | `AutoResetErrorTrueDeal` | `Uint16` | - |
| `function/f_error.c` | `AutoResetErrorFcDeal` | `Uint16` | - |
| `function/f_error.c` | `GetErrorAttribute` | `Uint16` | - |
| `function/f_error.c` | `ErrorDeal` | `void` | - |
| `function/f_fcDeal.c` | `GetFuncCodeInit` | `Uint16` | - |
| `function/f_fcDeal.c` | `ClearRecordDeal` | `void` | - |
| `function/f_fcDeal.c` | `RestoreCompanyParaOtherDeal` | `void` | - |
| `function/f_fcDeal.c` | `PowerOffRemDeal` | `void` | - |
| `function/f_fcDeal.c` | `GetFuncCodeInitOriginal` | `Uint16` | - |
| `function/f_frqSrc.c` | `FrqSrcDeal` | `void` | - |
| `function/f_frqSrc.c` | `UpdateFrqAim` | `void` | - |
| `function/f_frqSrc.c` | `UpdateRunMode` | `void` | - |
| `function/f_frqSrc.c` | `FrqSrcOperate` | `int32` | - |
| `function/f_frqSrc.c` | `getUpDownLimit` | `void` | - |
| `function/f_frqSrc.c` | `ResetUpDownFrq` | `void` | - |
| `function/f_frqSrc.c` | `GetFrq` | `int32` | - |
| `function/f_frqSrc.c` | `GetFrqComp` | `int32` | - |
| `function/f_frqSrc.c` | `GetFrqX` | `int32` | - |
| `function/f_frqSrc.c` | `GetFrqY` | `int32` | - |
| `function/f_frqSrc_pid.c` | `FrqPidSetDeal` | `int32` | - |
| `function/f_frqSrc_pid.c` | `PidFuncCalc` | `void` | - |
| `function/f_frqSrc_pid.c` | `FrqPidSetDeal` | `int32` | - |
| `function/f_frqSrc_pid.c` | `PidFuncCalc` | `void` | - |
| `function/f_interface.c` | `copyDataFunc2Motor05ms` | `void` | - |
| `function/f_interface.c` | `copyDataFunc2Motor2ms` | `void` | - |
| `function/f_interface.c` | `copyDataFunc2CF2ms` | `void` | - |
| `function/f_interface.c` | `copyDataCore2Func2ms` | `void` | - |
| `function/f_interface.c` | `copyDataCore2Uf2ms` | `void` | - |
| `function/f_interface.c` | `copyDataCore2Func05ms` | `void` | - |
| `function/f_interface.c` | `checkSpeedFdbDir` | `void` | - |
| `function/f_interface.c` | `UpdateDataCore2Func` | `void` | - |
| `function/f_interface.c` | `UpdateDataFunc2Core0p5ms` | `void` | - |
| `function/f_interface.c` | `UpdateDataFunc2Core2ms` | `void` | - |
| `function/f_invPara.c` | `InverterTypeRelatedParaDeal` | `void` | - |
| `function/f_invPara.c` | `InverterTypeRelatedNoMotorParaSaveDeal` | `void` | - |
| `function/f_invPara.c` | `MotorPowerRelatedParaDeal` | `void` | - |
| `function/f_invPara.c` | `TorqueBoostDeal` | `Uint16` | - |
| `function/f_invPara.c` | `ValidateInvType` | `Uint16` | - |
| `function/f_invPara.c` | `UpdateInvType` | `void` | - |
| `function/f_invPara.c` | `SaveTuneData` | `void` | - |
| `function/f_io.c` | `DiCalc` | `void` | - |
| `function/f_io.c` | `Dout1` | `void` | - |
| `function/f_io.c` | `Dout2` | `void` | - |
| `function/f_io.c` | `Dout38` | `void` | - |
| `function/f_io.c` | `Dout41` | `void` | - |
| `function/f_io.c` | `Dout3` | `void` | - |
| `function/f_io.c` | `Dout4` | `void` | - |
| `function/f_io.c` | `Dout5` | `void` | - |
| `function/f_io.c` | `Dout23` | `void` | - |
| `function/f_io.c` | `Dout6` | `void` | - |
| `function/f_io.c` | `Dout7` | `void` | - |
| `function/f_io.c` | `Dout8` | `void` | - |
| `function/f_io.c` | `Dout9` | `void` | - |
| `function/f_io.c` | `Dout10` | `void` | - |
| `function/f_io.c` | `Dout11` | `void` | - |
| `function/f_io.c` | `Dout12` | `void` | - |
| `function/f_io.c` | `Dout13` | `void` | - |
| `function/f_io.c` | `Dout14` | `void` | - |
| `function/f_io.c` | `Dout15` | `void` | - |
| `function/f_io.c` | `Dout16` | `void` | - |
| `function/f_io.c` | `Dout17` | `void` | - |
| `function/f_io.c` | `Dout18` | `void` | - |
| `function/f_io.c` | `Dout19` | `void` | - |
| `function/f_io.c` | `DoutRSVD` | `void` | - |
| `function/f_io.c` | `Dout21` | `void` | - |
| `function/f_io.c` | `Dout22` | `void` | - |
| `function/f_io.c` | `Dout24` | `void` | - |
| `function/f_io.c` | `Dout25` | `void` | - |
| `function/f_io.c` | `Dout26` | `void` | - |
| `function/f_io.c` | `Dout27` | `void` | - |
| `function/f_io.c` | `Dout28` | `void` | - |
| `function/f_io.c` | `Dout29` | `void` | - |
| `function/f_io.c` | `Dout30` | `void` | - |
| `function/f_io.c` | `Dout31` | `void` | - |
| `function/f_io.c` | `Dout32` | `void` | - |
| `function/f_io.c` | `Dout33` | `void` | - |
| `function/f_io.c` | `Dout34` | `void` | - |
| `function/f_io.c` | `Dout35` | `void` | - |
| `function/f_io.c` | `Dout36` | `void` | - |
| `function/f_io.c` | `Dout37` | `void` | - |
| `function/f_io.c` | `Dout39` | `void` | - |
| `function/f_io.c` | `Dout40` | `void` | - |
| `function/f_io.c` | `DoCalc` | `void` | - |
| `function/f_io.c` | `TemperatureDeal` | `void` | - |
| `function/f_io.c` | `TemperatureSensorDeal` | `void` | - |
| `function/f_io.c` | `GetTemperature` | `void` | - |
| `function/f_io.c` | `GetTemperatureCalc` | `Uint16` | - |
| `function/f_io.c` | `TemperatureAfterDeal` | `void` | - |
| `function/f_io.c` | `TemperatureDeal` | `void` | - |
| `function/f_io.c` | `AiCalc` | `void` | - |
| `function/f_io.c` | `AoCalcChannel` | `void` | - |
| `function/f_io.c` | `PulseInCalc` | `void` | - |
| `function/f_io.c` | `PulseInSample` | `void` | - |
| `function/f_io.c` | `FMPDeal` | `void` | - |
| `function/f_io.c` | `PulseOutCalc` | `void` | - |
| `function/f_io.c` | `PulseOutDeal` | `void` | - |
| `function/f_io.c` | `LineCalc` | `void` | - |
| `function/f_io.c` | `LpfCalc` | `void` | - |
| `function/f_io.c` | `InitSetEPWM4` | `void` | - |
| `function/f_io.c` | `InitSetEPWM5` | `void` | - |
| `function/f_io.c` | `InitSetEPWM6` | `void` | - |
| `function/f_io.c` | `InitSetEcap2` | `void` | - |
| `function/f_io.c` | `InitSetEcap3` | `void` | - |
| `function/f_io.c` | `InitSetEcap1` | `void` | - |
| `function/f_io.c` | `InitSetEcap4` | `void` | - |
| `function/f_io.c` | `InitDIGpio` | `void` | - |
| `function/f_io.c` | `InitDOGpio` | `void` | - |
| `function/f_io.c` | `InitECap2Gpio` | `void` | - |
| `function/f_io.c` | `InitEPwm5Gpio` | `void` | - |
| `function/f_io.c` | `InitEPwm4Gpio` | `void` | - |
| `function/f_io.c` | `InitEPwm6Gpio` | `void` | - |
| `function/f_io.c` | `InitECap3Gpio` | `void` | - |
| `function/f_io.c` | `InitECap1Gpio` | `void` | - |
| `function/f_io.c` | `InitECap4Gpio` | `void` | - |
| `function/f_length.c` | `ServoRunCtrl` | `void` | - |
| `function/f_main.c` | `InitForFunctionApp` | `void` | - |
| `function/f_main.c` | `InitDspFunc` | `void` | - |
| `function/f_main.c` | `Main2msFunction` | `void` | - |
| `function/f_main.c` | `Main0msFunction` | `void` | - |
| `function/f_main.c` | `Main05msFunction` | `void` | - |
| `function/f_main.c` | `Main05msFunctionEachStart` | `void` | - |
| `function/f_main.c` | `Main05msFunctionEachEnd` | `void` | - |
| `function/f_main.c` | `Main05msFunctionA` | `void` | - |
| `function/f_main.c` | `Main05msFunctionB` | `void` | - |
| `function/f_main.c` | `Main05msFunctionC` | `void` | - |
| `function/f_main.c` | `Main05msFunctionD` | `void` | - |
| `function/f_main.c` | `CodeRunMonitor` | `void` | - |
| `function/f_main.c` | `GetNumberDigit` | `Uint16` | - |
| `function/f_main.c` | `GetNumberDigit1` | `void` | - |
| `function/f_main.c` | `GetNumberDigit2` | `void` | - |
| `function/f_main.c` | `SoftVersionDeal` | `void` | - |
| `function/f_main.c` | `ValidateFuncCode` | `void` | - |
| `function/f_main.c` | `ValidateEepromAddr` | `void` | - |
| `function/f_main.c` | `GetTableEeprom2Fc` | `void` | - |
| `function/f_main.c` | `CheckEeprom2Fc` | `void` | - |
| `function/f_main.c` | `getMomeryValue` | `void` | - |
| `function/f_menu.c` | `MenuOnQuick` | `void` | - |
| `function/f_menu.c` | `MenuModeSwitch` | `void` | - |
| `function/f_menu.c` | `Menu1OnPrg` | `void` | - |
| `function/f_menu.c` | `ValidateTuneCmd` | `Uint16` | - |
| `function/f_menu.c` | `LimitDeal` | `Uint16` | - |
| `function/f_menu.c` | `NoSameDeal` | `Uint16` | - |
| `function/f_menu.c` | `UpdateMenu0DispStatus` | `void` | - |
| `function/f_menu.c` | `UpdateErrorDisplayBuffer` | `void` | - |
| `function/f_menu.c` | `ModifyFunccodeUpDown` | `Uint16` | - |
| `function/f_menu.c` | `ModifyFunccodeEnter` | `Uint16` | - |
| `function/f_menu.c` | `DispDataDeal` | `void` | - |
| `function/f_menu.c` | `RestoreCompanyParaRamDeal` | `void` | - |
| `function/f_menu.c` | `Menu0AddMenuLevel` | `void` | - |
| `function/f_menu.c` | `GetDispDigits` | `Uint16` | - |
| `function/f_menu.c` | `UpdateGroupDisplay` | `void` | - |
| `function/f_menu.c` | `GroupUpDown` | `Uint16` | - |
| `function/f_menu.c` | `DealUserMenuModeGroupGrade` | `void` | - |
| `function/f_menu.c` | `LimitOverTurnDeal` | `Uint16` | - |
| `function/f_menu.c` | `DealCheckMenuModeGroupGrade` | `void` | - |
| `function/f_menu.c` | `MenuModeDeal` | `void` | - |
| `function/f_menu.c` | `UpdataFuncCodeGrade` | `void` | - |
| `function/f_menu.c` | `FcDigit2Bin` | `Uint16` | - |
| `function/f_menu.c` | `MotorDebugFcDeal` | `void` | - |
| `function/f_menu.c` | `MenuInit` | `void` | - |
| `function/f_menu.c` | `UpdateDisplayBufferVisualIoStatus` | `void` | - |
| `function/f_menu.c` | `UpdateDisplayBufferVisualDiFunc` | `void` | - |
| `function/f_menu.c` | `MenuPwdOnPrg` | `void` | - |
| `function/f_menu.c` | `MenuPwdInputOnUpDown` | `void` | - |
| `function/f_modbus.c` | `ModbusRcvDataDeal` | `void` | - |
| `function/f_modbus.c` | `ModbusStartDeal` | `Uint16` | - |
| `function/f_modbus.c` | `UpdateModbusCommFormat` | `void` | - |
| `function/f_modbus.c` | `ModbusSendDataDeal` | `void` | - |
| `function/f_modbus.c` | `ModbusCommErrCheck` | `Uint16` | - |
| `function/f_motorFc.c` | `UpdateMotorPara` | `void` | - |
| `function/f_motorFc.c` | `UpdateMotorPara` | `void` | - |
| `function/f_osc.c` | `OscSciFunction` | `void` | - |
| `function/f_osc.c` | `InitOsc` | `void` | - |
| `function/f_osc.c` | `InitSciOSC` | `void` | - |
| `function/f_osc.c` | `SciDataTX` | `uint8` | - |
| `function/f_osc.c` | `OscTxFifoTask` | `void` | - |
| `function/f_osc.c` | `sciaTxFifoIsr` | `void` | - |
| `function/f_osc.c` | `SciDataRx` | `uint8` | - |
| `function/f_osc.c` | `sciaRxFifoIsr` | `void` | - |
| `function/f_osc.c` | `SciDataRxDeal` | `void` | - |
| `function/f_osc.c` | `OscDataQcq` | `void` | - |
| `function/f_osc.c` | `OscDataTxDeal` | `void` | - |
| `function/f_osc.c` | `GetOscData` | `uint8` | - |
| `function/f_osc.c` | `ClearOscBufEnd` | `void` | - |
| `function/f_osc.c` | `OscSciIoInit` | `void` | - |
| `function/f_osc.c` | `OscControlWriter` | `Uint16` | - |
| `function/f_osc.c` | `OscControlRead` | `Uint16` | - |
| `function/f_osc.c` | `OscSciFunction` | `void` | - |
| `function/f_p2p.c` | `P2PDataDeal` | `void` | - |
| `function/f_plc.c` | `InvTranTabCfg` | `Uint16` | - |
| `function/f_plc.c` | `InvRecTabCfg` | `Uint16` | - |
| `function/f_plc.c` | `FuncCodeRwAtrrib` | `Uint16` | - |
| `function/f_plc.c` | `PlcDataFramDeal` | `void` | - |
| `function/f_plc.c` | `InvTranTabCfg` | `Uint16` | - |
| `function/f_plc.c` | `InvRecTabCfg` | `Uint16` | - |
| `function/f_plc.c` | `PlcDataFramDeal` | `void` | - |
| `function/f_profibus.c` | `ProfibusRcvDataDeal` | `void` | - |
| `function/f_profibus.c` | `ProfibusStartDeal` | `Uint16` | - |
| `function/f_profibus.c` | `getDPControlCmd` | `void` | - |
| `function/f_profibus.c` | `setDPStatusInfo` | `void` | - |
| `function/f_profibus.c` | `UpdateProfibusCommFormat` | `void` | - |
| `function/f_profibus.c` | `ProfibusSendDataDeal` | `void` | - |
| `function/f_profibus.c` | `ProfibusCommErrCheck` | `Uint16` | - |
| `function/f_profibus.c` | `ProfibusCommErrCheck` | `Uint16` | - |
| `function/f_profibus.c` | `ProfibusRcvDataDeal` | `void` | - |
| `function/f_profibus.c` | `ProfibusSendDataDeal` | `void` | - |
| `function/f_profibus.c` | `ProfibusStartDeal` | `Uint16` | - |
| `function/f_profibus.c` | `UpdateProfibusCommFormat` | `void` | - |
| `function/f_runSrc.c` | `UpdateRunCmd` | `void` | - |
| `function/f_runSrc.c` | `setTimeRun` | `void` | - |
| `function/f_runSrc.c` | `runTimeCal` | `void` | - |
| `function/f_runSrc.c` | `RunSrcDeal` | `void` | - |
| `function/f_runSrc.c` | `UpdateBenchTime` | `void` | - |
| `function/f_runSrc.c` | `RunSrcUpdateFrq` | `void` | - |
| `function/f_runSrc.c` | `AccDecTimeCalc` | `void` | - |
| `function/f_runSrc.c` | `DormantDeal` | `void` | - |
| `function/f_runSrc.c` | `AccCalc` | `void` | - |
| `function/f_runSrc_accDecFrq.c` | `AccDecFrqCalc` | `void` | - |
| `function/f_runSrc_accDecFrq.c` | `LineChangeCalc` | `void` | - |
| `function/f_torqueCtrl.c` | `TorqueCalc` | `void` | - |
| `function/f_torqueCtrl.c` | `UpdateTorqueAim` | `void` | - |
| `function/f_torqueCtrl.c` | `AccDecTorqueCalc` | `void` | - |
| `function/f_torqueCtrl.c` | `TorqueLimitCalc` | `Uint16` | - |
| `function/f_torqueCtrl.c` | `TorqueCalc` | `void` | - |
| `function/f_torqueCtrl.c` | `UpdateTorqueAim` | `void` | - |
| `function/f_torqueCtrl.c` | `TorqueLimitCalc` | `Uint16` | - |
| `function/f_ui.c` | `UpdateDisplayBuffer` | `void` | - |
| `function/f_ui.c` | `WinkDeal` | `void` | - |
| `function/f_ui.c` | `DisplayScanPrepare` | `void` | - |
| `function/f_ui.c` | `DisplayScan` | `void` | - |
| `function/f_ui.c` | `KeyScan` | `void` | - |
| `function/f_ui.c` | `KeyBordTest` | `void` | - |
| `function/f_ui.c` | `KeyProcess` | `void` | - |
| `function/f_ui.c` | `InitSpiaGpio` | `void` | - |
| `function/f_ui.c` | `InitSetSpiForFunc` | `void` | - |
| `function/f_ui.c` | `UpdateMenuModeDisplayBuffer` | `void` | - |
| `function/f_vf.c` | `vfSeparateDeal` | `void` | - |
| `function/f_vf.c` | `vfSeparateDeal` | `void` | - |
| `function/include/f_funcCode_disp.c` | `UpdateU0Data` | `void` | - |
| `function/main.c` | `main` | `void` | - |
| `function/main.c` | `cpu_timer0_isr` | `void` | - |
| `function/main.c` | `ADC_Over_isr` | `void` | - |
| `function/main.c` | `EPWM1_zero_isr` | `void` | - |
| `function/main.c` | `EPWM1_TZ_isr` | `void` | - |
| `function/z_func.c` | `main` | `void` | - |
| `function/z_func.c` | `ramInit` | `void` | - |
| `function/z_func.c` | `ramInit` | `void` | - |
| `function/z_func.c` | `MemCopy` | `void` | - |
| `function/z_func.c` | `cpu_timer0_isr` | `void` | - |
| `function/z_func.c` | `InitSetAdc` | `void` | - |
| `function/z_func.c` | `qsqrt` | `Uint16` | - |
| `function/z_func.c` | `sqrt64` | `Uint32` | - |
| `motor/MotorCarrier.c` | `CalCarrierWaveFreq` | `void` | - |
| `motor/MotorCarrier.c` | `AsynPWMAngleCal` | `void` | - |
| `motor/MotorCarrier.c` | `SynPWMAngleCal` | `void` | - |
| `motor/MotorCarrier.c` | `SoftPWMProcess` | `void` | - |
| `motor/MotorCarrier.c` | `CalOutputPhase` | `void` | - |
| `motor/MotorCarrier.c` | `CalDeadBandComp` | `void` | - |
| `motor/MotorCarrier.c` | `ImSvcDeadBandComp` | `void` | - |
| `motor/MotorCarrier.c` | `ImFvcDeadBandComp` | `void` | - |
| `motor/MotorCarrier.c` | `PmFvcDeadBandComp` | `void` | - |
| `motor/MotorCurrentTransform.c` | `UVWToAlphBetaAxes` | `inline` | - |
| `motor/MotorCurrentTransform.c` | `AlphBetaToDQ` | `void` | - |
| `motor/MotorCurrentTransform.c` | `DQToAmpTheta` | `void` | - |
| `motor/MotorCurrentTransform.c` | `ChangeCurrent` | `void` | - |
| `motor/MotorDataExchange.c` | `ParSend05Ms` | `void` | - |
| `motor/MotorDataExchange.c` | `ParGet05Ms` | `void` | - |
| `motor/MotorDataExchange.c` | `ParSend2Ms` | `void` | - |
| `motor/MotorDataExchange.c` | `ParGet2Ms` | `void` | - |
| `motor/MotorDataExchange.c` | `ParSendTune` | `void` | - |
| `motor/MotorDebug.c` | `DebugSaveDeal` | `void` | - |
| `motor/MotorDebug.c` | `SaveDebugData16` | `void` | - |
| `motor/MotorDebug.c` | `SaveDebugData32` | `void` | - |
| `motor/MotorDebug.c` | `ResetDebugBuffer` | `void` | - |
| `motor/MotorEncoder.c` | `InitRtInterface` | `void` | - |
| `motor/MotorEncoder.c` | `InitSetQEP` | `void` | - |
| `motor/MotorEncoder.c` | `JudgeABDir` | `Uint` | - |
| `motor/MotorEncoder.c` | `JudgeUVWDir` | `Uint` | - |
| `motor/MotorEncoder.c` | `JudgeRTDir` | `Uint` | - |
| `motor/MotorEncoder.c` | `GetUvwPhase` | `void` | - |
| `motor/MotorEncoder.c` | `VCGetFeedBackSpeed` | `void` | - |
| `motor/MotorEncoder.c` | `GetSpeedMMethod` | `void` | - |
| `motor/MotorEncoder.c` | `GetSpeedMTMethod` | `void` | - |
| `motor/MotorEncoder.c` | `SpeedSmoothDeal` | `void` | - |
| `motor/MotorEncoder.c` | `GetMDetaPos` | `void` | - |
| `motor/MotorEncoder.c` | `GetMTTimeNum` | `void` | - |
| `motor/MotorEncoder.c` | `GetRotorTransPos` | `void` | - |
| `motor/MotorEncoder.c` | `RotorTransSamplePos` | `void` | - |
| `motor/MotorEncoder.c` | `RotorTransCalVel` | `void` | - |
| `motor/MotorImParEst.c` | `RsIdentify` | `void` | - |
| `motor/MotorImParEst.c` | `RrLoIdentify` | `void` | - |
| `motor/MotorImParEst.c` | `SetPwmForRrLoId` | `void` | - |
| `motor/MotorImParEst.c` | `InitRrLoVariable` | `void` | - |
| `motor/MotorImParEst.c` | `EnableTimer` | `void` | - |
| `motor/MotorImParEst.c` | `RrLoInPeriodAndCmpInt` | `void` | - |
| `motor/MotorImParEst.c` | `RrWaitAddGetUdc` | `void` | - |
| `motor/MotorImParEst.c` | `RrGetVoltReBegin` | `void` | - |
| `motor/MotorImParEst.c` | `RrLoCalVaule` | `void` | - |
| `motor/MotorImParEst.c` | `ImCalKpKi` | `void` | - |
| `motor/MotorImParEst.c` | `LmIoIdentify` | `void` | - |
| `motor/MotorImParEst.c` | `InitLmIoVaribale` | `void` | - |
| `motor/MotorImParEst.c` | `LmIoCalValue` | `void` | - |
| `motor/MotorImParEst.c` | `LmIoInPeriodInt` | `void` | - |
| `motor/MotorImParEst.c` | `ChgParForEst` | `void` | - |
| `motor/MotorInfoCollect.c` | `GetCurExcursion` | `void` | - |
| `motor/MotorInfoCollect.c` | `GetUDCInfo` | `void` | - |
| `motor/MotorInfoCollect.c` | `GetCurrentInfo` | `void` | - |
| `motor/MotorInvProtect.c` | `InvDeviceControl` | `void` | - |
| `motor/MotorInvProtect.c` | `SoftWareErrorDeal` | `void` | - |
| `motor/MotorInvProtect.c` | `TemperatureCheck` | `void` | - |
| `motor/MotorInvProtect.c` | `OutputPhaseLoseDetect` | `void` | - |
| `motor/MotorInvProtect.c` | `OutputLoseAdd` | `void` | - |
| `motor/MotorInvProtect.c` | `OutputLoseReset` | `void` | - |
| `motor/MotorInvProtect.c` | `InputPhaseLoseDetect` | `void` | - |
| `motor/MotorInvProtect.c` | `ControlFan` | `void` | - |
| `motor/MotorInvProtect.c` | `OverLoadProtect` | `void` | - |
| `motor/MotorInvProtect.c` | `CBCLimitCurProtect` | `void` | - |
| `motor/MotorInvProtect.c` | `SetCBCEnable` | `void` | - |
| `motor/MotorInvProtect.c` | `SetCBCDisable` | `void` | - |
| `motor/MotorInvProtect.c` | `LoadLoseDetect` | `void` | - |
| `motor/MotorInvProtect.c` | `BufferResProtect` | `void` | - |
| `motor/MotorMain.c` | `InitForMotorApp` | `void` | - |
| `motor/MotorMain.c` | `Main0msMotor` | `void` | - |
| `motor/MotorMain.c` | `Main05msMotor` | `void` | - |
| `motor/MotorMain.c` | `Main2msMotorA` | `void` | - |
| `motor/MotorMain.c` | `Main2msMotorB` | `void` | - |
| `motor/MotorMain.c` | `Main2msMotorC` | `void` | - |
| `motor/MotorMain.c` | `Main2msMotorD` | `void` | - |
| `motor/MotorMain.c` | `SendDataPrepare` | `void` | - |
| `motor/MotorMain.c` | `RunCaseRun2Ms` | `void` | - |
| `motor/MotorMain.c` | `PrepareParForRun` | `void` | - |
| `motor/MotorMain.c` | `TurnToStopStatus` | `void` | - |
| `motor/MotorMain.c` | `RunCaseStop` | `void` | - |
| `motor/MotorMain.c` | `CalRatioFromVot` | `void` | - |
| `motor/MotorMain.c` | `ADCOverInterrupt` | `void` | - |
| `motor/MotorMain.c` | `PWMZeroInterrupt` | `void` | - |
| `motor/MotorPWM.c` | `InitSetPWM` | `void` | - |
| `motor/MotorPWM.c` | `OutPutPWMVC` | `void` | - |
| `motor/MotorPWM.c` | `OutPutPWMVF` | `void` | - |
| `motor/MotorPWM.c` | `DeadBandComp` | `void` | - |
| `motor/MotorPWM.c` | `SendPWM` | `void` | - |
| `motor/MotorParChange.c` | `SystemParChg2Ms` | `void` | - |
| `motor/MotorParChange.c` | `RunStateParChg2Ms` | `void` | - |
| `motor/MotorParChange.c` | `SystemParChg05Ms` | `void` | - |
| `motor/MotorParChange.c` | `ChangeMotorPar` | `void` | - |
| `motor/MotorParChange.c` | `InvCalcPower` | `void` | - |
| `motor/MotorParEst.c` | `RunCaseGetPar` | `void` | - |
| `motor/MotorParEst.c` | `PrepareParForTune` | `void` | - |
| `motor/MotorParEst.c` | `EndOfParIdentify` | `void` | - |
| `motor/MotorPmsmMain.c` | `PG_Zero_isr` | `void` | - |
| `motor/MotorPmsmMain.c` | `IPMCheckInitPos` | `void` | - |
| `motor/MotorPmsmMain.c` | `PmChkInitPosRest` | `void` | - |
| `motor/MotorPmsmMain.c` | `RunCaseIpmInitPos` | `void` | - |
| `motor/MotorPmsmMain.c` | `SynCalRealPos` | `void` | - |
| `motor/MotorPmsmMain.c` | `SynCalLdAndLq` | `void` | - |
| `motor/MotorPmsmMain.c` | `IPMCalAcrPIDCoff` | `void` | - |
| `motor/MotorPmsmMain.c` | `PrepPmsmCsrPrar` | `void` | - |
| `motor/MotorPmsmMain.c` | `PmFluxWeakDeal` | `void` | - |
| `motor/MotorPmsmMain.c` | `PmDecoupleDeal` | `void` | - |
| `motor/MotorPmsmParEst.c` | `InitSetPosTune` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynTuneInitPos` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynInitPosDetect` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SetIPMPos` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SetIPMPos_ABZRef` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynCalLabAndLbc` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynInitPosDetCal` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynInitPosDetSetTs` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynInitPosDetSetPwm` | `void` | - |
| `motor/MotorPmsmParEst.c` | `DetectZeroPosOnce` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynTunePGZero_No_Load` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynTunePGZero_Load` | `void` | - |
| `motor/MotorPmsmParEst.c` | `SynTuneBemf` | `void` | - |
| `motor/MotorPublicCal.c` | `RunCaseShortGnd` | `void` | - |
| `motor/MotorPublicCal.c` | `RunCaseLowPower` | `void` | - |
| `motor/MotorPublicCal.c` | `CBCLimitCurPrepare` | `void` | - |
| `motor/MotorPublicCal.c` | `MaxUVWCurrent` | `Uint` | - |
| `motor/MotorPublicCal.c` | `HardWareErrorDeal` | `void` | - |
| `motor/MotorPublicCal.c` | `HardWareOverUDCDeal` | `void` | - |
| `motor/MotorPublicCal.c` | `RunCaseDcBrake` | `void` | - |
| `motor/MotorPublicCal.c` | `BrakeResControl` | `void` | - |
| `motor/MotorSVC.c` | `ResetSVC` | `void` | - |
| `motor/MotorSVC.c` | `SVCCalFlux_380` | `void` | - |
| `motor/MotorSVC.c` | `SVCCalRotorSpeed` | `void` | - |
| `motor/MotorSVC.c` | `SVCCalRotorSpeed_380` | `void` | - |
| `motor/MotorSpeedCheck.c` | `PrepareForSpeedCheck` | `void` | - |
| `motor/MotorSpeedCheck.c` | `RunCaseSpeedCheck` | `void` | - |
| `motor/MotorSpeedCheck.c` | `SpeedCheckSUB1` | `void` | - |
| `motor/MotorSpeedCheck.c` | `SpeedCheckUdcRiseDeal` | `void` | - |
| `motor/MotorSpeedCheck.c` | `SpeedCheckCycleOver` | `void` | - |
| `motor/MotorSpeedCheck.c` | `SpeedCheckCycleSUB2` | `void` | - |
| `motor/MotorVCMain.c` | `ResetParForVC` | `void` | - |
| `motor/MotorVCMain.c` | `ResetAsrPar` | `void` | - |
| `motor/MotorVCMain.c` | `ResetCsrPar` | `void` | - |
| `motor/MotorVCMain.c` | `CalIMSet` | `void` | - |
| `motor/MotorVCMain.c` | `CalTorqueLimitPar` | `void` | - |
| `motor/MotorVCMain.c` | `PrepareAsrPar` | `void` | - |
| `motor/MotorVCMain.c` | `CalUdcLimitIT` | `void` | - |
| `motor/MotorVCMain.c` | `VcAsrControl` | `void` | - |
| `motor/MotorVCMain.c` | `CalWsAndSynFreq` | `void` | - |
| `motor/MotorVCMain.c` | `CalWsAndSynFreq_380` | `void` | - |
| `motor/MotorVCMain.c` | `PrepImCsrPara` | `void` | - |
| `motor/MotorVCMain.c` | `PrepareCsrPara` | `void` | - |
| `motor/MotorVCMain.c` | `VCCsrControl` | `void` | - |
| `motor/MotorVCMain.c` | `SvcCalOutVolt` | `void` | - |
| `motor/MotorVCMain.c` | `SvcCalOutVolt_380` | `void` | - |
| `motor/MotorVCMain.c` | `CalcABVolt` | `void` | - |
| `motor/MotorVCMain.c` | `VCCsrControl_380` | `void` | - |
| `motor/MotorVF.c` | `VfVarInitiate` | `void` | - |
| `motor/MotorVF.c` | `VFOVUdcLimit` | `void` | - |
| `motor/MotorVF.c` | `CalOutVotInVFStatus` | `int` | - |
| `motor/MotorVF.c` | `CalTorqueUp` | `void` | - |
| `motor/MotorVF.c` | `VFWsTorqueBoostComm` | `void` | - |
| `motor/MotorVF.c` | `VFAutoTorqueBoost` | `void` | - |
| `motor/MotorVF.c` | `VFWSCompControl` | `void` | - |
| `motor/MotorVF.c` | `VFOverMagneticControl` | `void` | - |
| `motor/MotorVF.c` | `VFSpeedControl` | `void` | - |
| `motor/MotorVF.c` | `VfOverCurDeal` | `void` | - |
| `motor/MotorVF.c` | `VfOverUdcDeal` | `void` | - |
| `motor/MotorVF.c` | `VfFreqDeal` | `void` | - |
| `motor/MotorVF.c` | `VfOscIndexCalc` | `void` | - |
| `motor/MotorVF.c` | `HVfDeadBandComp` | `void` | - |
| `motor/MotorVF.c` | `HVfOscDampDeal` | `void` | - |
| `motor/MotorVF.c` | `HVfCurReDecomp` | `void` | - |
| `motor/ZeroPosCtl.c` | `GetPGPos` | `u32` | - |
| `motor/ZeroPosCtl.c` | `ZeroPosJudge` | `void` | - |
| `motor/ZeroPosCtl.c` | `ZeroPosControl` | `void` | - |
| `public/function_header/f_funcCode_disp.c` | `UpdateU0Data` | `void` | - |
| `subprogram/SubPrg.c` | `atan` | `int` | - |
| `subprogram/SubPrg.c` | `PID` | `void` | - |
| `subprogram/SubPrg.c` | `PIDLongRegulate` | `void` | - |
| `subprogram/SubPrg.c` | `PID32` | `void` | - |
| `subprogram/SubPrg.c` | `BurrFilter` | `void` | - |
| `subprogram/SubPrg.c` | `SlipFilter` | `void` | - |
| `system/DSP2803x_CpuTimers.c` | `InitCpuTimers` | `void` | - |
| `system/DSP2803x_CpuTimers.c` | `ConfigCpuTimer` | `void` | - |
| `system/main.c` | `main` | `void` | - |
| `system/main.c` | `cpu_timer0_isr` | `void` | - |
| `system/main.c` | `ADC_Over_isr` | `void` | - |
| `system/main.c` | `EPWM1_zero_isr` | `void` | - |
| `system/main.c` | `EPWM1_TZ_isr` | `void` | - |
| `system/mainInit_2803x.c` | `InitSysCtrl` | `void` | - |
| `system/mainInit_2803x.c` | `InitInterrupt` | `void` | - |
| `system/mainInit_2803x.c` | `SetInterruptEnable` | `void` | - |
| `system/mainInit_2803x.c` | `InitPeripherals` | `void` | - |
| `system/mainInit_2803x.c` | `copy_prg` | `void` | - |
| `system/mainInit_2803x.c` | `InitFlash` | `void` | - |
| `system/mainInit_2803x.c` | `KickDog` | `void` | - |
| `system/mainInit_2803x.c` | `DisableDog` | `void` | - |
| `system/mainInit_2803x.c` | `EnableDog` | `void` | - |
| `system/mainInit_2803x.c` | `InitPll` | `void` | - |
| `system/mainInit_2803x.c` | `InitPeripheralClocks` | `void` | - |
| `system/mainInit_2803x.c` | `InitPieCtrl` | `void` | - |
| `system/mainInit_2803x.c` | `rsvd_ISR` | `void` | - |
| `system/mainInit_2803x.c` | `InitPieVectTable` | `void` | - |
| `system/mainInit_2803x.c` | `InitSetGpio` | `void` | - |
| `system/mainInit_2803x.c` | `InitSetAdc` | `void` | - |
