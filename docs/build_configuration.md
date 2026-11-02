# 构建配置 (build_configuration)

## CCS 工程文件

- 工程: `CCs/md380.pjt` (Code Composer Project File Version 2.0)
- CPU 家族: `TMS320C28XX`; 工程类型: `Executable`
- 配置: `Debug`, `Release`
- 输出格式: COFF (CCS 3.3 时代工具链)

## 编译器选项

- **Debug** `Compiler`: `-g -q -o2 -i"$(Proj_dir)\..\public\28035_header" -i"$(Proj_dir)\..\public\motor_header" -i"$(Proj_dir)\..\public\function_header" -d"_DEBUG" -d"DSP2803X" -d"LARGE_MODEL" -ml -mt -v28`
- **Release** `Compiler`: `-pdsw225 -o3 -fr"$(Proj_dir)\Release" -i"$(Proj_dir)\..\public\28035_header" -i"$(Proj_dir)\..\public\motor_header" -i"$(Proj_dir)\..\public\function_header" -d"DSP2803X" -d"LARGE_MODEL" -ml -v28`
- **Debug** `Linker`: `-q -c -ecode_start -m".\Debug\md380_C.map" -o".\Debug\md380_C.out" -stack0x400 -w -x -l"rts2800_ml.lib"`
- **Release** `Linker`: `-c -ecode_start -m".\Release\MD380_Release.map" -o".\Release\MD380_Release.out" -stack0x400 -w -x -l"rts2800_ml.lib"`

## 链接器选项

- **Debug** (`CCs/Debug.lkf` 第 1 行):
  `-z -q -c -ecode_start -m"./Debug/md380_C.map" -o"./Debug/md380_C.out" -stack0x400 -w -x -i"C:/CCStudio_v3.3PLA/C2000/xdais/lib" -i"C:/CCStudio_v3.3PLA/bios_5_33_05/packages/ti/bios/lib" -i"C:/CCStudio_v3.3PLA/bios_5_33_05/packages/ti/rtdx/lib/c2000" -i"C:/CCStudio_v3.3PLA/C2000/cgtools/lib" -l"rts2800_ml.lib"`

- **Release** (`md380.pjt` Linker Settings):
  `-c -ecode_start -m".\Release\MD380_Release.map" -o".\Release\MD380_Release.out" -stack0x400 -w -x -l"rts2800_ml.lib"`

## 头文件搜索路径

1. `$(Proj_dir)\..\public\28035_header`  (DSP 设备头文件)
2. `$(Proj_dir)\..\public\motor_header`  (电机层头文件)
3. `$(Proj_dir)\..\public\function_header` (功能层头文件)

## 预定义宏

- Debug: `_DEBUG`, `DSP2803X`, `LARGE_MODEL`
- Release: `DSP2803X`, `LARGE_MODEL`

## 语言 / 模型选项

- `-v28` 目标 C28x (Debug/Release)
- `-ml` 大内存模型 (Debug/Release)
- `-mt` 仅 Debug 出现 (老工具链选项)
- `-pdsw225` 仅 Release 出现 (抑制诊断 225)

## 链接输入清单 (权威, 共 59 个 .obj)

1. `DSP2803x_CpuTimers.obj`
2. `DSP2803x_GlobalVariableDefs.obj`
3. `DSP2803x_usDelay.obj`
4. `f_canlink.obj`
5. `f_comm.obj`
6. `f_comm_DP.obj`
7. `f_common.obj`
8. `f_debug.obj`
9. `f_dspcan.obj`
10. `f_eeprom.obj`
11. `f_error.obj`
12. `f_fcDeal.obj`
13. `f_frqSrc.obj`
14. `f_frqSrc_pid.obj`
15. `f_funcCode.obj`
16. `f_interface.obj`
17. `f_invPara.obj`
18. `f_io.obj`
19. `f_main.obj`
20. `f_menu.obj`
21. `f_modbus.obj`
22. `f_motorFc.obj`
23. `f_osc.obj`
24. `f_p2p.obj`
25. `f_plc.obj`
26. `f_profibus.obj`
27. `f_runSrc.obj`
28. `f_runSrc_accDecFrq.obj`
29. `f_torqueCtrl.obj`
30. `f_ui.obj`
31. `f_vf.obj`
32. `main.obj`
33. `MotorCarrier.obj`
34. `MotorConstant.obj`
35. `MotorCurrentTransform.obj`
36. `MotorDataExchange.obj`
37. `MotorDebug.obj`
38. `MotorEncoder.obj`
39. `MotorImParEst.obj`
40. `MotorInfoCollect.obj`
41. `MotorInvProtect.obj`
42. `MotorMain.obj`
43. `MotorParChange.obj`
44. `MotorParEst.obj`
45. `MotorPmsmMain.obj`
46. `MotorPmsmParEst.obj`
47. `MotorPublicCal.obj`
48. `MotorPWM.obj`
49. `MotorSpeedCheck.obj`
50. `MotorSVC.obj`
51. `MotorVar.obj`
52. `MotorVCMain.obj`
53. `MotorVF.obj`
54. `ZeroPosCtl.obj`
55. `ASMSubPrg.obj`
56. `SubPrg.obj`
57. `DSP2803x_CodeStartBranch.obj`
58. `DSP2803x_CSMPasswords.obj`
59. `mainInit_2803x.obj`

## 参与链接的 .cmd

- `E:\XDS\md380_C_org11\public\common\DSP28035Peripheral.cmd`
- `E:\XDS\md380_C_org11\public\common\F28035.cmd`

## 已知工程配置异常 (原样记录, 未修改)

- Debug 编译选项含 `-o2`, Release 含 `-o3`。这两个**是优化级别**:
  在 `cl2000` 里 `-o` 是 `-O` / `--opt_level` 的别名 (实测 `-oN` 与 `-ON`
  目标文件 canonical 摘要完全相同; `-o=<路径>` 报 `bad argument to option -O`)。
  因此原工程 Debug 按 O2、Release 按 O3 优化, 优化**是生效的**。
  (早期文档误判为"`-o` 是输出文件名, 故未开启优化", 见 `docs/rewrite_decisions.md` D5。)
- `-mt` 只出现在 Debug, `-pdsw225` 只出现在 Release, 两者配置不对称。
- Release 链接器 `-o".\Release\MD380_Release.out"`, 而目录中产物名为
  `MD380_Release_20130110_Chint.out`, 说明 Release 产物曾被改名归档。
