等价性证据 (equivalence_evidence)

本文件汇总 "新工程 == 原工程" 的**可执行证据**。所有数字均来自工具实际运行输出,
不是推断。

## 0. 工具链

| 项 | 值 |
|---|---|
| 编译器 | TI C2000 CGT `v25.11.1.LTS` |
| 路径 | `E:\ti\c2000\cgt-c2000_25.11.1.LTS\ti-cgt-c2000_25.11.1.LTS\bin` |
| 目标 | TMS320F28035 / C28x, `-v28 -ml -mt` |
| 目标文件格式 | **COFF** — 必须显式加 `--abi=coffabi` |
| 解析工具 | `dis2000`(反汇编) / `ofd2000`(目标文件) / `hex2000`(映像) |

> 新 CGT 默认产出 EABI。EABI 会去掉符号前导下划线、换用 `rts2800_ml_eabi.lib`,
> 且把已初始化全局量放进 `.data`(原 `.cmd` 无 `.data` 归置 => 会被丢进 OTP)。
> 因此 `--abi=coffabi` 是硬性前提。

## 1. 证据层级总览

| 层级 | 手段 | 覆盖 | 结果 |
|---|---|---|---|
| A | 逐模块反汇编文本比对 (`dis2000 -1`) | 含 `.text` 的模块 | **8 逐行相同, 49 不同 (改写)** |
| B | 目标文件内容级比对 (段+重定位+符号表) | **全部 59 个模块** | **10 一致, 49 不同 (改写)** |
| C | 整机映像比对 (`hex2000` + SHA256) | Debug + Release | **因改写而不同 (差异有界, 见第 4 节)** |
| D | **宿主差分测试** (逐字段比对全部状态) | 改写模块 | **见第 5 节** |
| E | **目标端副作用检查** (告警 + 外部符号集合) | 改写模块 | **见第 5 节** |

层级 A 只对含机器码的模块有效; 纯数据模块 (无 `.text`) 的反汇编产物只有文件头,
因此 **"空 == 空" 不被当作证据**, 这类模块由层级 B 覆盖。

**关于改写**: 按"函数名保留 / 功能不变 / 实现适当变"的要求改过实现的模块,
**不受**"逐字节相同"这条契约约束 (登记在 `docs/rewritten_modules.json`),
改由层级 D/E 提供行为等价证据。注意契约的方向是"**没改写过的必须相同**",
而**不是**"改写过的一定不同" —— 实测已有改写模块编译产物与基准逐字节相同,
那是最强的一档结果, 不是异常。
当前改写模块: `src/motor/ZeroPosCtl.c`, `src/function/f_p2p.c`, `src/function/f_common.c`, `src/function/f_motorFc.c`, `src/function/f_invPara.c`, `src/function/f_plc.c`, `src/function/f_modbus.c`, `src/function/f_torqueCtrl.c`, `src/function/f_vf.c`, `src/function/f_frqSrc.c`, `src/function/f_runSrc.c`, `src/function/f_frqSrc_pid.c`, `src/function/f_canlink.c`, `src/function/f_profibus.c`, `src/motor/MotorDataExchange.c`, `src/function/f_fcDeal.c`, `src/function/f_error.c`, `src/function/f_menu.c`, `src/system/DSP2803x_CpuTimers.c`, `src/motor/MotorInfoCollect.c`, `src/motor/MotorParEst.c`, `src/function/f_dspcan.c`, `src/motor/MotorPublicCal.c`, `src/motor/MotorSpeedCheck.c`, `src/motor/MotorCarrier.c`, `src/function/f_ui.c`, `src/function/f_main.c`, `src/function/f_interface.c`, `src/function/f_eeprom.c`, `src/motor/MotorEncoder.c`, `src/motor/MotorCurrentTransform.c`, `src/motor/MotorDebug.c`, `src/subprogram/SubPrg.c`, `src/motor/MotorInvProtect.c`, `src/motor/MotorMain.c`, `src/system/main.c`, `src/system/mainInit_2803x.c`, `src/motor/MotorSVC.c`, `src/motor/MotorPWM.c`, `src/function/f_debug.c`, `src/function/f_comm_DP.c`, `src/motor/MotorPmsmMain.c`, `src/motor/MotorPmsmParEst.c`, `src/motor/MotorParChange.c`, `src/function/f_runSrc_accDecFrq.c`, `src/function/f_osc.c`, `src/motor/MotorImParEst.c`, `src/motor/MotorVCMain.c`, `src/motor/MotorVF.c`, `src/function/f_comm.c`, `src/function/f_io.c`

### 1.1 当前改写模块

| 模块 | 变换 | 差分用例 | 不一致 | 负向对照 | `.text` 变化 | 外部符号 |
|---|---|---:|---:|---:|---:|---|
| `src/motor/ZeroPosCtl.c` | 分支极性反转、提取局部量、Min 宏改为显式边界比较、硬件读序保持不变 | 243,472 | **0** | 7 / 7 抓到 | +8 | 一致 |
| `src/function/f_p2p.c` | if/else 分支互换、switch 展开为 if/else 链、提取同类型局部量、&& 两侧互换、改为提前返回 | 2,541,160 | **0** | 7 / 7 抓到 | +5 | 一致 |
| `src/function/f_common.c` | 循环由 while(length--) 改为 length==0 提前返回 + do/while(--length)、参数改为 const 指针局部量 pData; 两个位反转辅助函数改为调换各步先后顺序 | 17,039,736 | **0** | 12 / 12 抓到 | -1 | 一致 |
| `src/function/f_motorFc.c` | motorSn 的 if/else 改为三目表达式、M1 分支四条互不相干赋值重排、else 分支把索引表项提为 Uint16 局部量 nInc、编码器强制模式改为三目、极对数算式的首项提为 Uint32 局部量 frqDiv | 260,000 | **0** | 14 / 14 抓到 | +2 | 一致 |
| `src/function/f_invPara.c` | 四路电机的「同一值写四处 + 存四处」扇出改为 inc[4] 偏移表 + 嵌套循环; 四段逐字重复的额定参数赋值块提取为 SET_MOTOR_RATING_PARA 宏; 加减速时间分档改三目; 转矩提升 if/else 阶梯改三目链; 参数校验 if 改三目; UpdateInvType 把 volLevel 提为局部量 level; 电机选择 if/else 链改 switch; 数据点格式的两个布尔判据提为局部量; 定点搜索的 for+break 改 while; 索引算式提出 scaled 局部量; SaveTuneData 把 sizeof 改为可移植的元素个数写法并把 addBase 提到循环外 | 68,300 | **0** | 22 / 22 抓到 | +1 | 一致 |
| `src/function/f_plc.c` | InvTranTabCfg / InvRecTabCfg 把 datah/datal 提为局部量 addr/data、慢表下标改为就地 addr -= INV_SLOW_TAB_ADDR 复用、InvRecTabCfg 的 else{ if } 折成 else if; FuncCodeRwAtrrib 的「可接受高位半字节」空 if 体改为 switch(highH)、组号调整改 switch、禁用索引判定提为局部量 blocked 并拆成「所有模式共同」+「按 rwMode 追加」两段; PlcDataFramDeal 用 isFast 标志替代两处 InvTrTab == InvTrFastTab 指针比较、旋转索引的更新改为 pIndex 指针 + idxLimit | 325,966 | **0** | 38 / 38 抓到 | +23 | 一致 |
| `src/function/f_modbus.c` | ModbusRcvDataDeal 把三组 (高字节 << 8) + 低字节 里反复出现的两个帧字提为 highByte/lowByte 局部量复用; ModbusStartDeal 把「帧间隔未到」提前返回、并把两支逐字相同的 else if (CAN_OPEN && 地址1 / 地址 == commSlaveAddress) 合并成一条 || 判据; UpdateModbusCommFormat 把整除结果提为 frameSpaceTime 局部量、并重排三次字段写入; ModbusSendDataDeal 用提前返回替代 if/else 链、把 pwdPass/write 两支共同的地址回显提到合并分支之前、协议判据反转并把恒真的 else if (commProtocol == 0) 写成 else; ModbusCommErrCheck 改为提前返回并把阈值提为 overTimeLimit 局部量 | 414,979 | **0** | 47 / 47 抓到 | -8 | 一致 |
| `src/function/f_torqueCtrl.c` | 8 个魔数提为文件作用域宏 (TORQUE_PU_SHIFT / TORQUE_PU_BASE / RATING_TORQUE / TORQUE_LIMIT_CUR_PERCENT / TORQUE_LIMIT_CUR_PERCENT_BASE / TORQUE_LIMIT_SPD_ERR / TORQUE_LIMIT_TICKS / TORQUE_RAMP_STEP): 其中 RATING_TORQUE 原本在函数体中间定义、TORQUE_LIMIT_TIME / DO_CALC_PERIOD 的除法在 6 处逐字重复; 局部量改名 (tmp→torqueSet, tmp1→torqueAi2, torquePu→torqueFullScale, accDecTime→rampTicks, ret→limitActive); UpdateTorqueAim 的源选择 if/else 两支互换 (转矩环分支提前, 读起来是「转矩环用转矩环的源」); AI1/AI2/AI3 的「先算下标 tmp 再取值」两行合并为一行内联下标; TorqueLimitCalc 的电流门限算式提为 curLimit 局部量 (原来整个算式写在 if 条件里); 删掉 && 判据两侧的多余空行与一层多余括号 | 76,663 | **0** | 31 / 31 抓到 | +0 | 一致 |
| `src/function/f_vf.c` | 4 个魔数提为文件作用域宏 (VF_AI_SCALE_SHIFT / VF_COMM_SCALE_BASE / VF_HALF_SEPARATE_NUM / VF_ACC_TICKS): 其中 >> 15 在 AI 与脉冲两支里逐字重复、/ 10000 与频率加权系数 2 各只有一个出处、TIME_UNIT_VF_VOL_ACC_TIME / VF_CALC_PERIOD 的除法原来直接嵌在赋值行里; 局部量 tmp 改名为 srcValue (它装的是某个来源的原始设定值); 门控条件从「常量在左、比较方向混排、缩进混用 4 空格 / 7 空格 / 制表符+3 空格」规范为「变量在左、逐条加括号、缩进对齐」; 删掉紧贴函数定义的重复前置声明 void vfSeparateDeal(void); 与声明后从未使用的 extern Uint16 outVoltageDisp;; 删掉半分离分支里的一行多余空行; 两条过长的定标赋值按运算符换行 | 56,089 | **0** | 38 / 38 抓到 | +0 | 一致 |
| `src/function/f_frqSrc.c` | 频率给定链。改写手法: 变量改名 (31 组 204 处) + 魔数具名 (9 个文件作用域宏) + 同类型局部量按用途拆名 (一个 tmp 在三处分别是 upperFrqCalc / frqAimAbs / frqSrcValue) + if/else 合并与提前返回 + 共享结果改三元表达式 (GetFrqX / GetFrqY 两处)。改写由 build/_rewrite_frqsrc.py 按一张 47 条『替换表』生成, 每条都带期望命中次数, 计数不符即中止 —— 不是手抄。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_frqSrc/README.md。 | 47,949 | **0** | 129 / 129 抓到 | -2 | 一致 |
| `src/function/f_runSrc.c` | 运行指令与运行状态机 (T1 里 .text 最大的一块, 2852 字)。改写手法: 变量改名 (61 组 233 处, 只动函数内局部量与文件内 static, **一个全局名都没动**) + 魔数具名 (29 个文件作用域宏, 含掉电/休眠/时基/定标各一组) + 结构改写 (32 处): StopRunCtrl 的『共享结果先赋值再分支』改三元、ShutDownRunCtrl 停机过程两支合同一结果改三元、DormantDeal 两处『零则立即, 否则计时』合并成一个 || 判据、UpdateRunCmd 重复的显示状态赋值上提 + 空 else-if 删除 + 两处 jog 清零合并、AccDecTimeCalc 段内 if/else 改三元 + 档位常量具名、UpdateBenchTime 时基常量具名。改写由 build/_rewrite_runsrc.py 按一张『替换表』生成 (每条带期望命中次数, 计数不符即中止), 不是手抄; 该脚本还会自算活/死代码并双向断言。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_runSrc/README.md。 | 49,144 | **0** | 161 / 161 抓到 | -5 | 一致 |
| `src/function/f_frqSrc_pid.c` | PID 频率源 (T1 加减速与 PID 的一块, .text 1707 -> 1718 字, 差 +11)。整个驱动器的 PID 频率给定由它算出: 每拍按功能码选定给定源与反馈源, 跑增量式 PID, 滤波后输出频率; 状态跨拍保留 (整个 PID_FUNC 块、两个低通、一个给定斜坡、三个计时器、一个状态枚举、一个标志)。改写手法: 变量改名 (47 组 322 处, 只动函数内局部量与**文件内 static**, 一个全局名都没动) + 魔数具名 (23 个文件作用域宏) + 结构改写 (4 处)。改写由 build/_rewrite_frqsrcpid.py 按一张「替换表」生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入等于冻结基准, 再自算活/死代码并双向断言。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_frqSrc_pid/README.md。 | 21,411 | **0** | 176 / 176 抓到 | +11 | 一致 |
| `src/function/f_canlink.c` | CAN_LINK / PLC 卡的 CAN 驱动 (451 行, 12 个对外函数 + 3 个对外全局量, .text 703 -> 662 字)。改写手法: 变量改名 (21 组 183 处 —— 两个**文件内** static sBaud/sSourID -> s_canBaudIndex/s_canStationId, 三个**函数内** static count/timeOutErr/initFlag -> s_tranRetryCount/s_recTimeoutCount/s_canInitFlag, 以及 16 个局部量与形参) + 魔数具名 (17 个文件作用域宏) + 结构改写 (5 处) + 排版统一 (154 行的 279 个制表符展开成 4 空格)。改写由 build/_rewrite_canlink.py 按一张 47 条『替换表』生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再自算活/死代码并双向断言, 最后核对成员访问次数与 15 个对外符号的出现次数。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_canlink/README.md。 | 20,175 | **0** | 138 / 138 抓到 | -41 | 一致 |
| `src/function/f_profibus.c` | DP (PROFIBUS-DP) 从站帧协议 (473 行, 7 个对外函数 + 9 个对外全局量, .text 676 -> 684 字)。改写手法: 变量改名 (35 组 155 处) + 魔数具名 (18 个文件作用域宏) + 结构改写 (2 处) + 排版统一 (32 行 47 个制表符展开成 4 空格)。两处结构改写是: (1) 把 ProfibusSendDataDeal 里那段 EEPROM 索引锁存提成 `static void DpLatchEepromIndex(void)`; (2) 把 PZD 循环里复用的两个临时量 group/grade 去掉, 把 `/ 100` `% 100` 直接写进 GetGradeIndex 的实参。改写由 build/_rewrite_profibus.py 按一张替换表生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再自算活/死代码并双向断言, 最后核对成员访问次数与 16 个对外符号的出现次数。差分测试: 冻结基准与改写后各跑 n=20000, 都是 **118120 用例 / 0 处不一致**, 且七个入口的分相计数完全相同。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_profibus/README.md。 | 118,120 | **0** | 106 / 106 抓到 | +8 | 一致 |
| `src/motor/MotorDataExchange.c` | 电机层 <-> 功能层之间的参数搬运层 (268 行, 5 个对外函数 + 7 个对外数据, .text 由 build_and_compare.py 实测)。五个入口里每一条语句都是 `lhs = rhs;`, 没有分支、没有算术、没有硬件访问。改写手法: (1) 魔数具名 —— 7 个 `NUM_*` 宏换成带方向的 `F2M_*` / `M2F_*`, 并把原名 `NUM_2ms_FUNC_TO_MOTOR` 被当成『字数』与『第二段起始下标』两个角色用的地方拆成 `..._WORDS` 与 `..._BASE`, 且 `_BASE` 定义成**引用** `_WORDS` 而不是再写一个字面量, 两处不会再各自漂移; (2) 结构改写 —— ParSend2Ms 的第二段 30 条与 ParGet2Ms 的调试段 40 条, 原来每条都写`数组[基址宏 + k]`, 改成函数开头取一个局部指针 (`int *ext` / `int *dbg`) 后写 `p[k]`, 只改访问写法, 下标与目标字段完全不变; (3) 排版统一 —— 202 行里的制表符展开成 4 空格。改写由 build/_rewrite_mde.py 按一张替换表生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再断言源码里没有 `#if` (本模块没有条件编译, 整份文件都参与编译), 最后断言输出零制表符、纯 ASCII、无注释、无旧宏名残留。差分测试: 冻结基准与改写后各跑 n=20000, 用例数与不一致数见 docs/_difftest/MotorDataExchange.json。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/MotorDataExchange/README.md。 | 25,233 | **0** | 107 / 107 抓到 | -41 | 一致 |
| `src/function/f_fcDeal.c` | 功能码 (参数) 的初始化 / 清记录 / 恢复出厂 / 掉电保持 / 互限夹取 (419 行, 6 个函数 + 3 个导出数据)。改写手法: (1) 魔数具名 —— `2 - 1`(夹取遍数)、`0xFFFF0000UL`/`0x0000FFFFUL`/`>> 16`(掉电保持拆 plcTime 高低字)、`0x222`(清记录时 errorFrqUnit 的定值)、`6`(清记录单点个数)、`1140`(T1140 机型额定电压)、`0`/`1`(SVC 模式禁止/允许) 全部换成带名字的宏; (2) 判据抽函数 —— `GetFuncCodeInit` 里 5 个判据各自抽成 static 谓词 IsInitExceptSeries / IsInitExceptSingle / IsCalibrateCurveCode / IsAccDecTimeCode, 取值那一层抽成 GetCalibrateCurveInit, 主链从『两段 30 行的布尔表达式』变成『一行一个谓词』; (3) 结构改写 —— ClearRecordDeal 的 12 行手抄改成两张 const 表 + 一个 6 次循环, LimitOtherCodeDeal 的 60 行内层循环体抽成 static Uint16 LimitOneCode(Uint16), 外层只剩『扫描表 + 两遍 + 遍历清单』; (4) 局部量改名 i/j/k -> entry/scan/pass、init -> value、tmp -> clamped; (5) 排版 —— `AO_NUMBER-1`/`REM_NUM-1` 补空格, 3 行制表符展开成 4 空格。改写由 build/_rewrite_fcdeal.py 按一张 8 条替换表生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再断言两块死代码与全部 9 处条件编译栈改写后仍在, 最后断言输出零制表符、纯 ASCII、无注释、6 个导出符号齐全、括号配平。差分测试: 冻结基准与改写后各跑 n=20000, 用例数与不一致数见 docs/_difftest/f_fcDeal.json。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_fcDeal/README.md。 | 38,923 | **0** | 125 / 125 抓到 | +31 | 一致 |
| `src/function/f_error.c` | 故障管理: 欠压检测与 EEPROM 落盘握手、故障码择优与等级仲裁、故障现场保存、故障复位状态机、自动复位 (真值判据 + 功能码间隔判据) 与故障等级查表 (889 行, 13 个函数 + 18 个导出数据 + 4 个文件内 static)。改写手法: (1) 模块私有常量集中到文件头部一块 —— 原来 3 个 define 散在 GetNewError 之前、`ERROR_SCENE_NUMBER` 藏在 ErrorSceneSave 里; 并给 13 个魔数具名 (16/4/0xF 是电机侧位图的切分, 4 是无故障时的等级, 10 是自动复位清零等待的倍数, 5 是 OC 类自动复位的最小间隔, 100 是 devChk/osChk 的时间单位, 0x0FFF 与 <<4 是 errorFrqUnit 的打包); (2) 抽 static 助手 5 个 —— `UpdateError` 的三层嵌套 + 空语句占位改成『提前 return + RollbackError()』, `GetNewError` 里 6 处『取属性 -> 比等级 -> 换故障码』抽成 `ConsiderError(code, &errorLevel)`, OC/OV 的按加减速分档抽成 `ClassifySpeedFault`, `ErrorSceneSave` 的 8 个现场字段写入抽成 `SaveErrorScene()`, `GetErrorAttribute` 的『取数字位 -> 加 1』抽成 `GetActionLevel(group, index)`; (3) 结构改写 —— `getMotorError` 的 `if(...){for...} else {...}` 改成全零提前 return + 循环内 continue 守卫, 三个自动复位函数与 `ErrorSceneSave`/`GetNewErrorAttribute` 改成提前 return 风格; (4) 局部量改名 —— 4 个文件内 static (devTcnt/osTcnt/motorSnOld/diFuncErrRstOld) 改成 devChkTicker/osChkTicker/motorSnPrev/errorResetInOld, `errAutoRstSpaceTime`(局部) 改成 spaceTime, 删掉从未使用的 `int16 i;`; (5) 排版 —— 59 行制表符按 4 列制表位展开, 去掉 `if ( x )` 式空格与 ` #endif` 前的空格。改写由 build/_rewrite_ferror.py 按一张 21 条替换表生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再断言五块死代码与全部 7 处条件编译栈改写后仍在, 最后断言输出零制表符、纯 ASCII、无注释、31 个导出符号齐全、旧标识符清干净、括号配平。差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数与 16 个分阶段计数逐一相同, 见 docs/_difftest/f_error.json。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_error/README.md。 | 33,762 | **0** | 204 / 204 抓到 | -50 | 一致 |
| `src/function/f_menu.c` | 菜单 / 编辑 / 显示这一层 (2815 行基准 -> 2797 行交付, 73089 -> 75014 字节; 246 行死代码 + 2569 行活代码)。内容: `menu[]` 函数指针表驱动的多级菜单状态机 (0/1/2/3 级 + 口令提示/输入 + U0 用户组), 功能码的增减与进入改值 (ModifyFunccodeUpDown / ModifyFunccodeEnter), 上下限与回转夹取 (LimitDeal / OverTurnDeal / NoSameDeal / LimitOverTurnDeal), 12 个显示缓冲刷新 (UpdateMenu0/1/2/3DisplayBuffer / UpdateErrorDisplayBuffer / UpdateTuneDisplayBuffer / UpdateMenuPwdHint|InputDisplayBuffer / UpdateDisplayBufferAttribute|VisualIoStatus|VisualDiFunc), 分级与组切换 (UpdataFuncCodeGrade / GroupUpDown / DealUserMenuModeGroupGrade / DealCheckMenuModeGroupGrade), 运行量换算 (DispDataDeal / MotorDebugFcDeal / cycleShiftDeal / ValidateTuneCmd)。导出 125 个外部符号 = **70 个函数 + 55 个数据** (46 个 .ebss 标量/结构体 + 9 个 .econst 表), 另加 71 个未定义外部引用。改写手法: (1) 模块私有常量集中到文件头部一块 —— 原文件**没有任何 #define 常量表**, 魔数全散在函数里; 本轮给 18 个具名 (5 是数字位数, 1/2/3/4 是 operateDigit 的个/组/口令档, 100 是用户菜单 group/grade 打包, 16 是每组字母数, 10 是十位步进, 3 是 winkFlag 位基址, 0x01/0x08/0xF8 是 winkFlag 三档, 4/65535 是口令位数与上限, 0x000FFFFF/0x00FF 是位图掩码, 0x1040 是 PLC 视图属性, 0x000F 与 >>4 是 errorFrqUnit 的十进制位, 1<<14 与 >>15 是 PID 显示定标, 10000 是转速显示单位, 5/10 是脉冲频率换算, >>12 是转矩电流定标); (2) 抽两个 `static` 助手 —— `HandleQuickMenuSwitch()` 是四个 MenuXOnEnter 共用的『快速菜单切换』前导, `SetWinkFlag(level)` 是三处 `winkFlag = 1 << (3 + operateDigit)`; (3) 控制流正常化 —— Menu0OnUpDown / Menu2OnShift / UpdateMenu0DisplayBuffer / UpdateMenu2DisplayBuffer / ModifyFunccodeUpDown / MenuPwdInputOnEnter 等改成『提前 return + 扁平守卫』, Menu3OnEnter 的嵌套 if 合并成一个合取式, Menu2OnUpDown 的 if/else 尾巴互换, MenuOnQuick 的 else 分支改成提前 return; (4) 局部量改名 —— 16 个旧名清干净 (data1/upper1/lower1/currentGroupDispF/currentGroupDisp0/tmp1/tmp2/gradeTmp/checkMenuModeFcIndex/errorCodeOld4Menu0/tuneCmdOld4Menu0/menu0Number/attributeMenu0/bitDisp/bMinus/dataTmp), 20 个新名 (dataSigned/upperSigned/lowerSigned/gradePrev/searchFcIndex/errorCodePrev/tuneCmdPrev/dispNumber/dispAttribute/dispIndex/bNegative/absData/dataPrev/pDispBit/dispPara/dispParaNum/groupDispF/groupDisp0/letterIndex/letterDigit), 含两个**函数内 static** (errorCodeOld4Menu0/tuneCmdOld4Menu0 -> errorCodePrev/tuneCmdPrev); (5) 排版 —— 26 行制表符按 4 列制表位展开, 去掉装饰性空行与冗余的匿名作用域, 补全单语句分支的花括号。改写由 build/_rewrite_fmenu.py 按一张 **76 条替换表**生成 (72 条函数级覆盖 50 个函数 + 4 条文件级, 每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再断言 13 块死代码整块逐字节仍在、11 条宿主补丁锚点命中次数不变、条件编译行的多重集改写前后相等, 最后断言输出零制表符、纯 ASCII、无注释、125 个导出符号齐全、16 个旧标识符清干净、两个新助手确实是 `static`。差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数 **106865** 与 16 个分阶段计数逐一相同, mismatches=0, 见 docs/_difftest/f_menu.json。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_menu/README.md。 | 107,013 | **0** | 447 / 447 抓到 | +9 | 一致 |
| `src/system/DSP2803x_CpuTimers.c` | 把三个定时器各自重复一遍的初始化序列抽成文件内静态函数 InitOneTimer(定时器, 寄存器块), 三处调用它; 0xFFFFFFFF 提为具名常量 CPU_TIMER_PRD_MAX; ConfigCpuTimer 把 Timer->RegsAddr 提成一个局部指针 Regs, 后续不再逐次重解引用。T2 首个模块。 | 20,312 | **0** | 24 / 24 抓到 | -27 | 一致 |
| `src/motor/MotorInfoCollect.c` | ADC 面向的采集层三个入口 (GetCurExcursion / GetUDCInfo / GetCurrentInfo)。改写手法: (1) 控制流正常化 —— `EnableCount = (EnableCount>200)?200:EnableCount;` 的三元夹取写成 `if (EnableCount > 200) { EnableCount = 200; }`; 两处嵌套 `if` 收尾改提前 return (`if (Count < 32) return;`); 两个 `-32768 == m_ErrIx` 的饱和保护由负字面量比较写成正字面量在左的形式; 长表达式按物理含义断行 (扣偏置 / 采样定标 / 母线定标各占一行); (2) 排版 —— 制表符与行内多空格对齐统一成 4 空格, 去掉装饰性空行。**未做**的是把全局量提成局部指针: 实测那样在 C28x 上对静态分配的全局量是**劣化** —— 直接寻址 `MOV AL,@0x2c` 是 1 字, 换成指针要 `MOVL XARn,#addr` + 栈上溢出 + 调用后重载, 三个函数合计 .text 从 297 涨到 367 (+70 字), 且 GetUDCInfo/GetCurrentInfo 都在 ADC 中断里。所以本轮只改表达式与控制流。 | 515,204 | **0** | 44 / 44 抓到 | +1 | 一致 |
| `src/motor/MotorParEst.c` | 电机参数辨识的调度层三个入口 (RunCaseGetPar / PrepareParForTune / EndOfParIdentify) 加六张 const 流程表。改写手法: (1) RunCaseGetPar —— 把 `switch(gGetParVarable.ParEstContent[gGetParVarable.ParEstMstep])` 的选择子先读进局部量 `m_stepNow`, switch 分派在快照上; 8 路 case 与 default 的顺序、目标函数、`ParSendTune()` 的收尾位置一字未动。(2) PrepareParForTune —— 字段搬运仍是逐条赋值 (未合并、未改序), 只是去掉了列对齐用的多空格; 表选择 switch 保持 5 路 + default; 搬运循环 `for(m_index=0; ...) ParEstContent[m_index] = *(m_PIdentifyFlow + m_index);` 改写成块内两个游走指针 `*m_pDst = *m_pSrc; m_pDst++; m_pSrc++;`。(3) EndOfParIdentify —— `gGetParVarable.QtEstDelay` 先读进局部量 `m_delay`, 两个分支 (`== 0` / `>= 5`) 复用同一次读; `TUNE_FINISH != StatusWord` 与 `IDC_FVC_CTL == gMainCmd.Command.bit.ControlMode` 都写成常量在左; 所有 if/else 补花括号。**没有**改任何判据常量、写入字段、调用顺序或写入次序。 | 20,786 | **0** | 64 / 64 抓到 | +10 | 一致 |
| `src/function/f_dspcan.c` | DSP 片内 eCAN 的五个入口 (InitdspECan / InitTranMbox / InitRecMbox / eCanDataTran / eCanDataRec)。改写手法: (1) InitdspECan —— 四段顺序执行的 `if (con == N)` 保持顺序 if 而非改写成 else-if 链 (con=0 会把 con 置 1, 紧接的 con==1 块**必须**继续执行, 改成 else-if 会整段跳过); 邮箱清零循环由 `MsgCtrlPi[i<<2] = 0` 改成游走指针 `*MsgCtrlPi = 0; MsgCtrlPi += 4;`; CCE 判据写成 `== 0` / `== 1` (CCE 是 1 位字段, 与原来的 `!= 1` / `!= 0` 等价)。(2) InitTranMbox —— **`eCanTranEnFlag |= 1ul << mbox;` 一字未动地保留**: 它移的是 mbox 而 InitRecMbox 移的是 id = mbox & 0x1f, 这个不对称是原行为, 不是要修的 bug; 数据写入改成 `dataPi[0]` / `dataPi[1]` 显式下标。(3) InitRecMbox —— `(mbox & 0x80) == 0x80` 简化为 `mbox & 0x80` (单比特), `(mbox & 0x40) == 0x40` 同理。(4) eCanDataTran —— `(flag & bit) != bit` 改成 `!(flag & bit)`。(5) eCanDataRec —— 外层判据倒置成早退 `return CAN_MBOX_EMPTY;`, 两处相同的 `CANRMP.all = 1ul<<mbox` 提到 CANRML 读之前 (CANRMP 与 CANRML 是不同寄存器, 重排等价)。**没有**改任何判据常量、寄存器写入值、错误码或写入次序。 | 29,356 | **0** | 80 / 80 抓到 | -24 | 一致 |
| `src/motor/MotorPublicCal.c` | 与机型/调制方式无关的公共保护与制动模块, 八个入口: RunCaseShortGnd / RunCaseLowPower / CBCLimitCurPrepare / MaxUVWCurrent / HardWareErrorDeal / HardWareOverUDCDeal / RunCaseDcBrake / BrakeResControl。改写手法: (1) 两个 switch 的选择子 `gMainStatus.SubStep` 先读进局部量 `m_subStep`, 分派在快照上 —— case 体里对 SubStep 的写因此不再改变被选中的分支 (原代码 switch 表达式本来就只求值一次, 这是**把隐式的一次求值写成显式**); (2) MaxUVWCurrent 的两级三目取大写开: `m_IU = (m_IU >= m_IV)?m_IU:m_IV;` -> `if (m_IU < m_IV) { m_IU = m_IV; }`; (3) 单 bit 掩码判据 `(all & M) == M` 写成 `(all & M) != 0`; (4) `(gBrake.Flag & 0x01) == 0` 写成 `!(gBrake.Flag & 0x01)`; (5) 去掉注释与被 `/* */` 包起来的旧制动占空比算法死块。**没有**改任何判据常量、寄存器写入值、写入顺序或调用顺序。 | 217,924 | **0** | 98 / 98 抓到 | -6 | 一致 |
| `src/motor/MotorSpeedCheck.c` | 异步机/永磁机共用的**转速搜索**状态机, 六个入口: PrepareForSpeedCheck / RunCaseSpeedCheck / SpeedCheckSUB1 / SpeedCheckUdcRiseDeal / SpeedCheckCycleOver / SpeedCheckCycleSUB2。改写手法: (1) 三目赋值展开成 if/else —— 第 49 行 `SpeedCheck = (SpeedLast<0) ? (-SpeedMaxPu) : SpeedMaxPu` 与第 54 行的 Speed5hz 版本各展开成一对分支; (2) 单 bit 位域的等号判据改写成非零判据: `Case4Sig1.bit.CHECK_GET == 1` -> `!= 0` (第 104 行), `Case4Sig2.bit.DelayOver == 1` -> `!= 0` (151), `AlmostOver == 1` -> `!= 0` (162); (3) 赋值语句的列对齐多空格去掉。**没有**改任何判据常量、写入字段集合、写入顺序、调用顺序; `#if 0` 包起来的『按电流反算 CurDelta』那一支**原样保留**在源码里 (它决定了第 145 行恒执行 `CurDelta = 5125`, 见下面 invariants 里 S64 那条)。 | 33,909 | **0** | 131 / 131 抓到 | -17 | 一致 |
| `src/motor/MotorCarrier.c` | 载波频率与 PWM 角度 / 死区补偿, 六个入口: CalCarrierWaveFreq / AsynPWMAngleCal / SynPWMAngleCal / SoftPWMProcess / CalOutputPhase / CalDeadBandComp。改写手法: (1) 四处 `m_MaxFc = (m_MaxFc>N)?N:m_MaxFc;` 的三目钳位展开成 `if (m_MaxFc > N) { m_MaxFc = N; }`; (2) `gFcCal.FcBak = (gFcCal.FcBak > gBasePar.FcSet) ? gBasePar.FcSet : gFcCal.FcBak;` 与 `(gFcCal.FcBak<10) ? 10 : gFcCal.FcBak` 同样展开成 if; (3) 同步/异步分派 (`ModuleApply == 1` -> `!= 1`) 与 AsynPWMAngleCal 的分频判据 (`FcApply > 61440` -> `<= 61440`) 都**倒置**, 分支体随之互换; (4) SoftPWMProcess 的 `m_Coff = 3277; if (30 < FcSetApply) m_Coff = 1966;` 合成一个三目, `gPWM.SoftPWMCoff = gPWM.SoftPWMCoff + 29517*GetTime();` 写成 `+=`; (5) SynPWMAngleCal 的频点表扫描由 `while (AbsFreq >= T[m_Index]) { m_Index++; if (m_Index >= 10) break; }` 改成把上界并入循环条件 `while ((m_Index < 10) && (AbsFreq >= T[m_Index]))`; (6) CalDeadBandComp 的分段判据倒置成 `FreqReal > 40000` 先行; (7) CalOutputPhase 去掉空的 `else { ; }` 分支。**没有**改任何判据常量、写入字段、写入顺序或调用顺序; 两处 `DINT ... EINT` 之间的语句一条没动、一条没挪。**刻意保留**了两处原样死代码, 并在这里记下来而不是顺手清掉: 末尾 `#if 0` 的三个空死区函数, 以及 CalDeadBandComp 里 `#if 1/#else` 的备用死区算法 (与 MotorSpeedCheck 保留 `#if 0` 那一支同一处置)。另外 `if (gInvInfo.InvTypeApply >= 19) { m_TempLim = 70; }` 是一个**恒等分支** (两支都赋 70), 同样**原样保留**: 删掉它会让'以后有人把上一行改成别的值'这件事改变含义 —— 留着是原行为的一部分, 不是没看见。 | 32,441 | **0** | 106 / 106 抓到 | +7 | 一致 |
| `src/function/f_ui.c` | 键盘/显示刷新, 十个入口: UpdateDisplayBuffer / WinkDeal / DisplayScanPrepare / DisplayScan / KeyScan / KeyBordTest / KeyProcess / InitSpiaGpio / InitSetSpiForFunc / UpdateMenuModeDisplayBuffer。改写手法: (1) DisplayScanPrepare 里 `spiData.in.key >> 7` 被读两次 (一次写 keyValue, 一次判 keySelType), 提成一个具名局部量 `m_keyIn` —— **必须**放在 RX 循环之后: `spiData.in` 与 `spiData.out` 虽是 struct 的两个成员 (不是 union), 但 RX 循环里 `spiData.in.all[j] = SpiaRegs.SPIRXBUF;` 会改写整个 `spiData.in` (含 `.key`), 提到循环之前就变成了读旧值; (2) 两处三目赋值展开成 if/else —— `(!(spiData.in.key >> 7)) ? (keySelType = 0) : (keySelType = 1);` 与 `(tcnt >= KEY_SEL_TEST_TIME) ? (tcnt = 0) : (tcnt++);`; (3) 单比特判据 `== 1` 改成 `!= 0` (keyChecking / keySelType); (4) `if (!x)` 改成 `if (x == 0)` (errorCodeOld / scanStep / keySelStatus / tcnt / keyChecking / bLight[scanStep] / bKeyEsc / bKeyProc / keyTemp.funcCnt / keyPreCtrl); (5) DisplayScanPrepare 里 8 行制表符缩进改 4 空格; (6) `[menuModeTmp-1]`、`(1<<(...))` 补齐运算符两侧空格。**没有**改任何判据常量、写入字段、写入顺序或调用顺序。 | 84,331 | **0** | 52 / 52 抓到 | -1 | 一致 |
| `src/function/f_main.c` | 函数侧顶层调度, 十八个入口: InitForFunctionApp / InitDspFunc / InitFuncCode / FuncVariableInit / Main2msFunction / Main0msFunction / Main05msFunctionEachStart / Main05msFunctionEachEnd / Main05msFunctionA / Main05msFunctionB / Main05msFunctionC / Main05msFunctionD / CodeRunMonitor / GetNumberDigit / GetNumberDigit1 / GetNumberDigit2 / SoftVersionDeal / getMomeryValue。改写手法: (1) GetNumberDigit 的两支**极性反转 + 函数体对调** (`if (!mode) { GetNumberDigit1(...); } else { GetNumberDigit2(...); }` 变成 `if (mode != 0) { GetNumberDigit2(...); } else { GetNumberDigit1(...); }`); (2) 位段裸判据补成显式比较 `!= 0` (`pAttribute->attribute.bit.upperLimit` / `.bit.lowerLimit` / `dspStatus.bit.runEnable` / `dspStatus.bit.uv`); (3) `if (!x)` 改成 `if (x == 0)` (errorEeprom / funcCodeRwModeTmp 两处 / pAttribute->attribute.bit.upperLimit); (4) 裸整型判据补成 `!= 0` (errorEeprom / errorFuncCode / digit[4..1]); (5) SoftVersionDeal 里 `funcCode.code.funcSoftVersion` 与 `funcCode.code.motorSoftVersion` 各被读两次, 提成两个**同宽 (Uint16)** 具名局部量 m_funcSoftVersion / m_motorSoftVersion —— 三个字段在 include/function/f_funcCode.h 里都是 `Uint16`, 提出来不改变任何一次比较或赋值的宽度; 读出时机在两处写 productVersion / funcSoftVersion **之后**、分支写 softVersion **之前**, 中间没有任何写路径; (6) Main05msFunctionEachEnd 的 `ticker++; if (ticker >= 4) { ticker = 0; }` 合成 `if (++ticker >= 4)`, 与本文件 CodeRunMonitor 里已有的写法统一; (7) getMomeryValue 的两条语句合成声明处初始化; (8) 28 行制表符缩进改 4 空格 (其中第 652 行是"3 空格 + 制表符"的混排, 单独按周围同级缩进摆平); (9) 4 处运算符两侧补空格 (`[fcIndex+1]` / `%10` / `* 3413` / `[FNUM_PARA-1]`); (10) GetNumberDigit 里 `return 5;` 的缩进笔误 (12 空格) 摆成同级的 8 空格。**没有**改任何判据常量、写入字段、写入次序或调用次序。 | 77,173 | **0** | 155 / 155 抓到 | +5 | 一致 |
| `src/function/f_interface.c` | 核心/功能两侧的数据交换层, 三个 2ms 搬运函数 + 三个周期回调。改写手法: (1) **删掉全部 `#if 1 ... #else ... #endif` 与 `#elif 1` 的死分支** (5 处 memcpy 的逐字段 `#else` 展开、cf[] 的 40 项展开、uf[] 的 30 项展开) —— 那些分支一次都没被编译过, 删掉对产物零影响, 也让"哪些语句真在活路径上"一眼可读; (2) 未被任何负向对照锚点覆盖的 幻数提成具名宏 (`OUT_FDB_FRQ_DISP_FILTER_TIME 30` / `FRQ_DISP_CLAMP 32000` / `OUT_CURRENT_DISP_DEADBAND 15` / `SPEED_DIR_STABLE_TIME 2500`) —— **只提取, 不改值**; (3) `checkSpeedFdbDir` 的第二条判据把 `checkFlag == ...` 从末尾挪到开头 (与同一函数里第三条 判据的写法统一), `&&` 内的项次序不动; (4) `UpdateDataFunc2Core0p5ms` 末尾的 `#if 1` 块保留 原注释结构; (5) 全文件把若干处错位缩进与运算符两侧空格归一。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。 | 205,651 | **0** | 208 / 208 抓到 | +0 | 一致 |
| `src/function/f_eeprom.c` | EEPROM 驱动层: I2C 位缓存区搬运 / 中断源分发 / 两次握手 / 读写状态机 / EepromDeal 大状态机。改写手法: (1) **两个状态枚举改成 #define + Uint16** (enum FUNCCODE_WRITE_STATUS / enum FUNC_CODE_RW_STATUS) —— 原文件里这两个枚举只被当作整型用, 改成宏后所有枚举常量仍是整型字面量, 判值与写入字段不变; (2) **三处重试惯用法 `while (f() != OK) { if (++t >= 100) break; }` 改成计数 for** —— 原写法是"先调再判", 新写法是"判完再调", 调用次数上界同为 100 且都在第一次成功后立即退出, 寄存器副作用逐次一致; (3) **唯一 `#elif 1` 死臂改成 `#else`** (10 处), 多臂链 (InitSetI2ca 的 4 选 1) 保留原 `#elif 1` 不动, 否则预处理会断; (4) 函数级 `Uint16 tmp = 0;` 的初始化下沉到 for 的初始化子句; (5) 前导空行、include 次序、宏对齐、声明分组归一。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。 | 103,745 | **0** | 175 / 175 抓到 | +15 | 一致 |
| `src/motor/MotorEncoder.c` | 编码器/测速层: QEP 初始化、UVW/AB/RT 三种判向、UVW 相位、VC 反馈、M 法/T 法测速、转速平滑、M 法增量、MT 计时、旋变位置与速度。改写手法: (1) **删掉 InitRtInterface 里 `#ifdef TMS320F2808` 的死分支** (本工程是 28035) 并把 `#else` 展开 —— 那些行一次都没被编译过; (2) **删掉 RotorTransCalVel 里的 `#if 0 ... #else ... #endif`**, 只留活臂 (两臂的 `DetaPos` 那一行逐字相同, 所以它才是“唯一锚点”问题的现场); (3) **GetRotorTransPos 的 12 步手工展开改成 `for(mShift=15; mShift>=4; mShift--)` 循环**, 把唯一那句 `asm(" RPT #5 || NOP ");` 放进 `if(mShift > 4)` 里 —— 源码里只剩 1 处 asm, 但**动态执行次数仍是 11 次**, 与改写前逐次相同, 目标端延时不变 (这是 .text 缩小 90 字的主因, 不是行为变化); (4) 三处有界计数 `if(cnt<32767) cnt++;` 改成钳位三目; (5) `else if` / `else` 的返回链改成平铺 `if` + 末尾 `return`; (6) **GetUvwPhase 里三段逐字相同的 ADC 门限判断提成一个文件级 static 函数** `UvwLogicLevel(Uint adcValue, Uint activeLevel)` (static, 不进外部符号表); (7) 全文件缩进与运算符两侧空格归一。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。`Ulong mUlong;` **刻意保留** —— 它只在死臂里被引用, 删掉会让目标端告警集合变化 (两侧各 1 条告警正是它)。 | 76,063 | **0** | 117 / 117 抓到 | -90 | 一致 |
| `src/motor/MotorCurrentTransform.c` | 定子电流 Clarke/Park/幅值-相角变换层: UVW->AlphBeta (Q15 缩放)、AlphBeta->DQ (Park)、DQ->幅值+相角、以及把三者和滤波串起来的 ChangeCurrent。改写手法: (1) `UVWToAlphBetaAxes` 的两个乘积经 `llong m_AlphBeta[2]` 局部数组中转, 并把 `>>15` 改写为"先 `*2L` 再 `>>16`" —— `floor(2n/65536) == floor(n/32768)` 对正负 n 都成立 (23170/32768 = 46340/65536), 数值逐位相同; (2) `AlphBetaToDQ` 的 M/T 两个结果先各自算进 `llong m_M / m_T`, 再统一右移 15; T 由 `(-((llong)m_sin*Alph)) + ((llong)m_cos*Beta)` 改写为 `((llong)m_cos*Beta) - ((llong)m_sin*Alph)` —— 64 位下加法可交换且不会溢出, 逐位相同; (3) `DQToAmpTheta` 先把 M/T 取成 `long` 局部量再平方相加, 去掉一层冗余括号; (4) `ChangeCurrent` 把 `(gPhase.IMPhase>>16)` 提成 `int m_Angle` (带显式强转), 并把 `UVWToAlphBetaAxes` 调用挪到它之后 (两者互不读写对方的数据, 顺序无关); (5) 全文件缩进归一、局部变量声明归位。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。**两处刻意不动**: (a) `ChangeCurrent` 里的 `temp` 保持 `int` —— 目标端 `int` 是 16 位, `VoltPhaseApply - Theta` 在 16 位里回绕, 改成 `long` 在宿主上仍 0 不一致 但目标端回绕点变了 (这正是"宿主等价、目标端不等价"的标准陷阱); (b) `void inline` 关键字保留 —— 去掉它会让 `UVWToAlphBetaAxes` 变成外部符号, target_check 的符号集合检查立刻报差异 (实测符号集合两侧都是同样 14 个)。 | 70,932 | **0** | 23 / 23 抓到 | +23 | 一致 |
| `src/motor/MotorDebug.c` | 上位机调试数据记录层: `gDebugBuffer[1000]` 环形缓冲 + `gDebugPoint`/`gDebugFlag` 的写入、回卷、半区回退与清零, 以及按 `TestData12/13/14/15/17` 和 `gMainCmd.Command.bit.Start` 决定"这一拍要不要记、记几条曲线、是否复位缓冲" 的 `DebugSaveDeal`。改写手法: (1) 删掉 `#ifdef TMS320F2808 -> 5000` 这条死分支和外层 `#ifdef MOTOR_SYSTEM_DEBUG` 的 `#else -> C_DEBUG_BUFF_SIZE 1` 死分支 —— 本文件第 10 行自己 `#define MOTOR_SYSTEM_DEBUG`, 而本工程是 28035 (`TMS320F2808` 全工程从未定义), 两条分支预处理后均不可达, 宏值恒为 1000; (2) `switch(gTestDataReceive.TestData12)` 的 1/2/default 三档改写为 `if/else if/else` 链, `save` 的三处取值逐个相同; (3) `saveIndexP` 的 `==1/==2/==3/==4` 四条并列分支改写为 `int * pVDTable[4]` 局部数组 + `for(i = 0; i < (Uint)saveIndexP; i ++) { SaveDebugData16(*pVDTable[i]); totalN ++; }` —— 原四条分支各自"存一个通道、加一个计数", 与循环逐项展开逐条相同; `saveIndexP` 取 0 或 >4 时两条路径都不保存、`totalN` 都为 0 (原代码四个 if 都不进; 新代码被 `saveIndexP >= 1 && saveIndexP <= 4` 挡住); (4) `for(i = 0; i < 32; i++) if(saveIndexFunc & (1L<<i)) totalN++;` 改写为 `Ulong m_Mask = saveIndexFunc; while(m_Mask){ totalN += (Uint)(m_Mask & 1L); m_Mask >>= 1; }` —— `saveIndexFunc` 是 `Ulong` (目标端 32 位), 置位数逐位相同, 且不再写 `1L<<31` 这种对长整型做 31 位左移的边界表达; (5) `gTestDataReceive.TestData17/2` 与 `C_DEBUG_BUFF_SIZE/2` 提成 `Uint m_HalfPeriod` / `Uint m_Half` 局部量 (目标端 `Uint` 是 16 位, 500 与 498 都在范围内); (6) 局部变量声明归位、缩进归一、`SaveDebugData32` 里的半区回退写成单行 `if`。`#pragma DATA_SECTION(gDebugBuffer, "debug_buffer")` 原样保留。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。 | 758,330 | **0** | 34 / 34 抓到 | -47 | 一致 |
| `src/subprogram/SubPrg.c` | 电机控制公共算法库, 6 个纯函数 (`atan` / `PID` / `PIDLongRegulate` / `PID32` / `BurrFilter` / `SlipFilter`), 不碰外设、不碰全局量, 全部输入输出都走 struct。改写手法全部是**保持求值语义的结构变换**: (1) `atan`: x==0 的两级写法收成提前返回 (`return (y<0)?(-16384):16384;`), 尾部的 `if(x<0) m_Angle = m_Angle + 32768;` 改写为 `m_Angle = qatan(m_Input) + ((x < 0) ? 32768 : 0);` —— 同为 int 加法, 加 0 不改变值; (2) `PID`: 把 `m_KpSat`/`m_KiSat` 两个界与 `m_Max`/`m_Min` 的求值顺序重排为 "先两个界、再两个限幅基量、再 KP 链、再 KI 链" (这些语句互不依赖, 且都只是移位, 不受 `SETOVM` 的 OVM 标志影响, `SETOVM`/`CLROVM` 的包裹范围原样保留); 抗积分饱和的 `if(A&&B)...else if(C&&D)...` **保持原形状** —— 改成两个独立 if 不等价, 因为第一个分支会改 `pid->Total`, 从而影响第二个分支的 `pid->Total < 0` 判据; (3) `PIDLongRegulate`: `<<4` 补上与 `*` 的优先级括号 (优先级本来就如此), `m_OutKi` 提到分支前算 (分支不读它); (4) `PID32`: `m_OutKi` 提到分支之前算 (分支只读 `m_OutKp`); (5) `BurrFilter`: 末尾两条顺序执行的钳位 `if` 改写为两条**独立**的三元 (`Err = (Err>Max)?Max:Err` 与 `if(Err>Max) Err=Max;` 逐分支相同; 不能合成 else-if, 因为 Max 与 Min 可能反序, 合成后第二个钳位会被跳过); (6) `SlipFilter`: 历史搬移循环从 `for(i=0;i<15;i++) d[i]=d[i+1]` 改成 `for(i=1;i<16;i++) d[i-1]=d[i]` (同为整体左移一格), `(16-1)` 写成 `15`。**没有**改任何判据常量、移位位数、饱和界、写入字段、写入次序或类型宽度。原工程的 bug 原样保留: `PIDLongRegulate` 的输出上限判据读的是**输入** `pid->Out` 而不是刚算出的 `m_Out` (变异 PL4 就是把它改成 `m_Out`, 已验证 CAUGHT)。 | 517,311 | **0** | 54 / 54 抓到 | -4 | 一致 |
| `src/motor/MotorInvProtect.c` | 执行机构保护模块: 软件错误处理、五张温度表的温度检查、输出缺相、输入缺相、风机控制、变流/电机两侧过载积分、CBC 逐周期限流、负载丢失与缓冲电阻过热。改写只做**结构变换**: 算式一个字不改, 只动控制流与访问路径 —— (1) InvDeviceControl 删掉一个空 `{ }` (原注释被剥掉后留下的壳), 调用顺序原样; (2) SoftWareErrorDeal: gError 经局部指针 pErr, 折掉一个外层已保证的非零判断, 过压/欠压改"过压提前 return"; (3) TemperatureCheck: 两处 if/else 赋值收成三元, 二分查找 `while` 改 `for` (自增表达式等价, 循环体次数不变), 命中判定的 `else if` 改 `if` (前一分支必 break); (4) OutputPhaseLoseDetect: 最值由两组嵌套三元改成"先取 U 再逐相折叠"; (5)/(10) 各结构体统一经局部指针访问; (6) InputPhaseLoseDetect: 尾部 if/else 按德摩根律取反; (7) ControlFan: 尾部 if/else-if 改"命中即 return"; (8)/(9) 预警位与 CBC 三相标志累加的 if/else 收成三元; (11) LoadLoseDetect: 九项 `||` 守卫先算进局部量再判, m_Limit 下限的 if 化; (12) BufferResProtect: 过热置位改 `|= (cond) ? ... : 0`, **先判旧值再自减的顺序保留**。 | 61,694 | **0** | 157 / 157 抓到 | +139 | 一致 |
| `src/motor/MotorMain.c` | MotorMain: the 2 ms / carrier / ADC-ISR task layer (15 entry points). Structural rewrite only, no arithmetic touched. (1) Every switch rewritten as an if / else-if chain over the SAME case order with the default arm last (Main2msMotorB first switch: 7 arms; Main2msMotorB second switch: 8 arms; RunCaseRun2Ms: 6 arms; ADCOverInterrupt: 7 arms). (2) Ternary -> if/else: Main2msMotorB's gOutVolt.VoltPhaseApply (16384 / -16384) and CalRatioFromVot's gRatio clamp. RunCaseRun2Ms' '?:' on gFeisu.SpeedLast becomes a temp + if/else that still performs exactly one write to SpeedLast. (3) Guard-clause / early-return: Main0msMotor returns early when PGMode != 0; RunCaseStop's ShortGnd if/else loses the else because the if arm returns; Main2msMotorB's 'MotorType != PM' branch is inverted to test the positive first. (4) SendDataPrepare's outer 4-term guard is inverted to if(!(...)) so the compute arm reads first and the zero arm is the else. (5) Repeated side-effect-free reads factored into a local at the point of use, with no call between the two uses: gBasePar.FcSetApply (RUN_SYNC_TUNE arm, type Uint) and gVFPar.FreqApply (ASYNC_VF/SYNC_VF arm of RunCaseRun2Ms, type int). (6) Magic numbers given block-scope const names: 80 (KP scale divisor), 16384 (half-Q15), 50 / 1500 / 4096 (InitForMotorApp defaults), 8192 / 287 (CalRatioFromVot). No file-scope object was added. (7) Locals renamed: m05HzPu -> m_Hz50Pu, tempU -> m_PosSum, mAiCounter -> m_SampleCnt, mTotal1..3 -> m_Sum1..3, mRatio -> m_VoltRatio, m_Ratio -> m_RatioTmp; every local keeps its original type. | 22,517 | **0** | 256 / 256 抓到 | -7 | 一致 |
| `src/system/main.c` | 整机入口: main() 的 0.5ms/2ms/0ms 调度主循环 + 三个中断服务程序 (`ADC_Over_isr` / `EPWM1_zero_isr` / `EPWM1_TZ_isr`)。改写手法: (1) **删死代码**: `#if 0 ... #endif` 里的 `cpu_timer0_isr` (整块不可达), 以及正文里 10 处 `#ifdef CPU_TIME_DEBUG ... #endif` 计时段 —— 工程两种构建(project/md380.pjt 的 Debug/Release 选项) 都只定义 `_DEBUG / DSP2803X / LARGE_MODEL`, `CPU_TIME_DEBUG` 从未定义, 这些块预处理后一行不剩。(`gCpuTime` 的 `Det05msClk` / `tmp0Ms` 两个写在 #ifdef 外面, 是活代码, 原样保留。) (2) 主循环 `while(1)` 改写成等价的 `for(;;)`; (3) 相位分配的四级 `if((m_LoopFlag & 0x03) == k) ... else if ...` 链改写为 `m_Phase = m_LoopFlag & 0x03;` + `switch(m_Phase)` 的 case 0/1/2/3 —— 数值被 0x03 掩过, switch 的四支与原链逐支对应, 函数调用序列逐条相同; (4) 三个 ISR 只做空白归一 (原文件制表符/空格混用), **语句序列一动没动** —— 每个 ISR 都是一串不可重排的副作用 (EALLOW/EDIS 配对、清中断标志、开/关中断、取时间、调回调、写 PIEACK), 任何重排都会改变目标端行为; (5) extern 声明块保持原样 (本模块对外接口)。**没有**改任何判据常量 (C_TIME_05MS / 65535 / 0x03)、写入字段、调用次序或类型宽度。 | 200,120 | **0** | 38 / 38 抓到 | -2 | 一致 |
| `src/system/mainInit_2803x.c` | 整机 bring-up: 16 个入口, 全是"往外设寄存器写常量 + 挂 128 个中断向量 + 调 4 个后续初始化"。改写手法全部是**结构变换**, 表达式、常量、字段、类型一个字没动: (1) 同一个 EALLOW/EDIS 块内的互不依赖的寄存器写**重排** (InitFlash 的 4 个等待寄存器、InitPll 的 7 个 CLKCTL 位与 MCLKOFF/DIV 一对、InitPeripheralClocks 的 4 个时钟寄存器、InitSetGpio 的 13 个 GPIO 寄存器、InitSetAdc 的 4 个上电位与 14 个 SOC配置、SetInterruptEnable 的 3 个 PIEIER、InitInterrupt 的 5 个向量挂载); (2) `IER = 0x0000; IFR = 0x0000;` 两句对调 (紧邻且中间无事件); (3) copy_prg 的长度调整改括号 `(Uint)crp->size - 1` -> `(Uint)(crp->size - 1)`; (4) 两个指针自增循环改成下标循环 (`*(p++) = v` -> `p[i] = v`): InitPieCtrl 的 PIEIER/PIEIFR 交错清零、InitPieVectTable 的 128 格向量填充; (5) `while (c) { }` -> `while (c);`。 | 20,283 | **0** | 69 / 69 抓到 | +4 | 一致 |
| `src/motor/MotorSVC.c` | SVC (定子电压定向) 磁链观测器与转速估算器, 4 个入口: ResetSVC / SVCCalFlux_380 / SVCCalRotorSpeed / SVCCalRotorSpeed_380。改写手法: (1) ResetSVC 的 20 个清零赋值按目标对象重新分组 (原顺序是 FluxR/FluxS/SVCSpeed/MainCmd/低通/comp 交错), 全部写互不重叠的对象; (2) `gVCPar.SvcMode` 原本在 SVCCalFlux_380 里读两次, 缓存到 m_SvcMode 再判; (3) 两条支路里逐字重复的纯子表达式提成局部量: `((long)gMotorExtPer.LM*5000L/(long)gBasePar.FullFreq01)` -> m_LmScale, `((long)gIMTSetApply.M>>12)` -> m_MSet, `((long)gIMTSetApply.T>>12)` -> m_Tset; (4) Alpha / Beta 两条支路原来共用一个 m_Long 顺序执行, 拆成 m_LongA / m_LongB 使两条支路互不依赖 (每条链都以一次新赋值开头, 旧值不被读); (5) `m_LowVolt = 300; if(InvTypeApply>=22) m_LowVolt = 80;` 改成条件表达式; (6) I0 / (M>>12) 两路除数先选进 m_Div 再统一做一次除法; (7) `m_IntData` 的两条三元表达式改成 if / else if / else; (8) FirstCnt 的 `if(...){...return;} else if(...) else` 改成顺序 if; (9) 平方和表达式去掉冗余括号。**所有常量、移位量、掩码、字段、类型、运算顺序一个字没动。** | 502 | **0** | 171 / 171 抓到 | -52 | 一致 |
| `src/motor/MotorPWM.c` | PWM 输出级: 5 个入口 InitSetPWM / OutPutPWMVC / OutPutPWMVF / DeadBandComp / SendPWM。这是全工程第一个直接写外设寄存器的模块 (EPwm1/2/3Regs), 也是第一个带 `#if 0` 死代码块的模块。改写手法: (1) InitSetPWM 里三个通道逐字重复的 12 条公共寄存器写 (TBPRD/TBPHS/TBCTL/HSPCLKDIV/CMPCTL/CMPA/AQCTLA/AQSFRC/AQCSFRC/DBCTL/DBFED/DBRED) 提成 SetPwmTimerBase(pReg, tbCtl), 三处 TZSEL/TZCTL/TZEINT 提成 SetPwmTripZone(pReg), 三处 `#ifdef TMS320F2808` 收成一个 C_TZ_SEL 宏加一处 `#ifndef`; (2) OutPutPWMVC 三相正弦的三段相同算式提成 OutputOnePhase(), 原来靠 `phase = phase - 21845;` 连改两次, 改成把 `phase - 21845` 与 `phase - 21845 - 21845` 当实参传 (trunc16 对减法同态, 逐位相同); (3) OutPutPWMVF 的 `m_pUVW = (Ulong *)&gPWM.U; *(m_pUVW + sel) = len;` 三字指针游走改成 SetPhaseLength() 的显式 switch; (4) DeadBandComp 的三相死区补偿 + __IQsat 提成 DeadBandCompOne(); (5) SendPWM 的三相"清标志 -> 判零 -> 改 AQCTLA/CMPCTL" 提成 SendOneChannel(); (6) `phase = (phase > 65531)?65531:phase;` 改成 if; 嵌套 if 的悬挂体补齐大括号。**所有常量、移位量、掩码、比较运算符、iTrig 五张表的初值一个字没动。** | 217,585 | **0** | 174 / 174 抓到 | -12 | 一致 |
| `src/function/f_debug.c` | 示波器/调试采样模块。9 个外部入口, 但 8 个是空体: UniversalDebug32 / PowerOnDebug / InitKeyDebug / InitI2cDebug / ChgAttr / DealKeyDebug / TestI2cDebug 的函数体整块在 `#if 0` 里, InitDebug 只是调用其中三个空函数。实测 .text 102 字, 其中 UniversalDebug 占 95 字 (93%)。改写手法: (1) UniversalDebug 的六路 switch (case 1..5 + default) 改成两张 static 查表 gScopeBuf[] / gScopeLen[] —— 守卫 `sn >= SCOPE_NUMBER` 已把 sn 限在 [0,6), switch 的六支恰好覆盖这六个值且每支只赋那一对值, 原 default 分支不可达 (它做的事等价于"保留初值", 而表里第 0 项就是初值那一对); (2) 上升沿清数组的 `for (j = scopeNum-1; j >= 0; j--)` 改成递减 `while (k != 0U) { k--; ... }`, 去掉 int16 循环变量, 边界与写入顺序都相同; (3) 下标边界与回绕那段把 `scopeIndex[sn]` 读进局部 idx 再写回, 消掉`*(pScope + scopeIndex[sn])` 里的重复全局读和 `++scopeIndex[sn]` 的读-改-写嵌套 (安全性依赖两条已核对的事实: pScope 指向 scope0..5, 与 scopeIndex[] 不别名; matlabDebug[] 全工程只有初始化式一处写入, 所以 *matlabDebug[sn] 也指不到 scopeIndex[])。**所有常量、比较运算符、`#if` 配置阶梯、matlabDebug[] 与 scopeInterval[] 的初始化式一个字没动; 八个空体函数与全部 `#if 0` 块整块保留。** | 150,226 | **0** | 34 / 34 抓到 | -42 | 一致 |
| `src/function/f_comm_DP.c` | DP (显示面板) SCI 通讯模块。14 个外部入口: 两个中断 `SCI_DP_RXD_isr` / `SCI_DP_TXD_isr`, 一个 inline 发送启动 `CommDpStartSend`, 收发帧处理 `CommDpRcvDataDeal` / `CommDpSendDataDeal` / `SciDpDeal` / `ErrAgainSend`, 初始化 `InitSetSciDp` / `UpdateSciDpFormat` / `InitSciaGpioDp` / `InitScibGpioDp` / `InitSciDpBaudRate`, 参数 `GetDataByteNum` / `DpDataDeal`。改写手法 (共 9 个差异块): (1) **两处逐语句相同的 8 句通道复位序列** (RXD 中断的 else 臂、`CommDpRcvDataDeal` 的 CRC 错误超限臂) 抽成 `static void ResetDpChannel(struct SCI_DATA_DP *p)` —— 这是本模块唯一真正重复的代码, 原来两份连空格数量都一致; (2) RXD 中断里 4 句的"收满一帧"序列抽成 `static void CompleteDpFrame(p, length)`, 顺带把 `(dataByteNum + 4)` 提成局部 `frameLen` (原式在同一个 if 里求值三次, 且 `dataByteNum` 是全局, 提取同时消掉一处宿主/目标整型宽度的潜在分歧); (3) **三处两行的 CRC 落盘** (高位先写、低位后写) 抽成 `static void StoreCrc(Uint16 *pBuf, Uint16 loIndex, Uint16 crc)`, 三处的下标表达式各不相同 (dataByteNum+3/+2、+2/+1), 由参数给出; (4) `InitScibGpioDp` 改为直接调用 `InitSciaGpioDp` —— 两个函数体**逐语句、逐常量、逐寄存器相同** (已逐行核对: 同一组 GPAPUD/GPAQSEL2/GPAMUX2 位、同一对 PieVectTable 赋值、同一句 IER |= M_INT9、同一对 PIEIER9 使能), 只差缩进; (5) `CommDpRcvDataDeal` 把 `dataByteNum + 2` 提成 `crcLow`, 三处分散的`+3` / `+2` / `+2` 改写成 `crcLow+1` / `crcLow` / `crcLow`, 让"CRC 低位下标"这个名字只出现一次; (6) `InitSetSciDp` / `UpdateSciDpFormat` 把 `p->pSciRegs` 提成 `volatile struct SCI_REGS *pReg` —— 写寄存器序列与写顺序一个字节没动; 小改: `ErrAgainSend` 的 `for` 改递减 `while`、`GetDataByteNum` 的 `dataByteNums[dpParameter.dpDataFormat - 1]` 提成局部 `format`。**所有常量、比较运算符、`#if` 配置阶梯、`dataByteNums[]` 初始化式、五个条件编译死区整块保留。** | 20,991 | **0** | 106 / 106 抓到 | -10 | 一致 |
| `src/motor/MotorPmsmMain.c` | PMSM 初始定位 / 零位检测 / Ld-Lq 辨识 / 弱磁 / 解耦模块。10 个入口: EQEP 零位中断 `PG_Zero_isr`, 初始位置标定 `IPMCheckInitPos` / `PmChkInitPosRest` / `RunCaseIpmInitPos`, 位置折算 `SynCalRealPos`, 电感辨识 `SynCalLdAndLq`, 电流环系数 `IPMCalAcrPIDCoff` / `PrepPmsmCsrPrar`, 弱磁 `PmFluxWeakDeal`, 解耦 `PmDecoupleDeal`。改写手法 (共 9 个差异块, 12470 -> 11342 字节): (1) `PG_Zero_isr` 的 ABZ/UVW 两条零位判据抽成 `static int ZeroErrCount(void)` —— 原来 `errZFlag` 是 ISR 里的局部变量, 在两条支路里各自 `++`, 最后 `if(errZFlag)`; 改写后由函数返回计数, `if(ZeroErrCount())`。`GetUvwPhase()` 的调用位置 (在 UVW 支路的第一条判据之后、第二条之前) 一字未动; (2) `PG_Zero_isr` 的 `if(RunStep != STATUS_GET_PAR) {...} else {...}` 改成早退加落空, 消掉一层嵌套 —— 两支互斥且都不在结尾之后, 等价; (3) 两处逐字相同的 QEP 回绕修正 (QEP 累计与 ABZ 累计) 抽成 `static void QepTotalWrap(long * pTotal)`, `mSign` 随之搬进辅助函数; (4) `IPMCheckInitPos` 的活代码从三层嵌套 `if/else` 改成三条早退, `abs(SpeedTemp) < 100` 取反成 `>= 100`; (5) `RunCaseIpmInitPos` 的 `case 1` 改成先判错再干活的早退形式; (6) `RunCaseIpmInitPos` 的 `case 2` 里那条 `if(abs(PowerOffPosDeg - RotorPos) < 3641) { }` 空语句整条删除 —— 两支都是空语句序列, 条件只读两个字段无副作用, 可证明等价 (见 dead_code); (7) `SynCalRealPos` 删掉第二次重复的 `m_4Pluse = ((long)PulseNum)<<2` —— 两次之间只有 `GetQepCnt()` 一个宏 (`(*EQepRegs).QPOSCNT`), 无副作用; (8) `PrepPmsmCsrPrar` 的 `if/else` 改成 `if { ...; return; }` 加落空, 并删掉从未被引用的局部 `int Nf`; (9) `PmFluxWeakDeal` 里两处逐字相同的 IdSet 钳位抽成 `static void FluxWeakIdClamp(int maxCur)`, 删掉函数末尾多余的 `return;`。所有算术表达式逐字符保留: `(long)x<<15`、`>>7`、`% (PulseNum<<2)`、`(m_CntMod<<14)/(int)PulseNum`、`m_Angle += 43691`、`(3*total+x)/4` 形式的 `Filter4` 调用、`__IQsat` 的四个限幅常量, 一个字符都没改 —— 这个模块的危险全在 16/32 位截断点上, 动表达式就是动语义。 | 113,392 | **0** | 175 / 175 抓到 | +27 | 一致 |
| `src/motor/MotorPmsmParEst.c` | PMSM 参数辨识/初始位置辨识模块。13 个入口: 初始化 `InitSetPosTune`、初始位置脉冲等待 `SynTuneInitPos`、初始位置检测状态机 `SynInitPosDetect`、位置写回 `SetIPMPos` / `SetIPMPos_ABZRef`、Ld/Lq 相位计算 `SynCalLabAndLbc` / `SynInitPosDetCal`、载波/脉宽配置 `SynInitPosDetSetTs` / `SynInitPosDetSetPwm`、零位检测 `DetectZeroPosOnce`、空载/带载零位辨识 `SynTunePGZero_No_Load` / `SynTunePGZero_Load`、反电势辨识 `SynTuneBemf`。模块自有 4 个文件级对象 (`gIPMZero` / `gIPMInitPos` / `gEstBemf` / `gPmParEst`)。改写手法 (整体 1133 -> 1094 行, 30798 -> 30891 字节): (1) 全部 9 处 `switch`/`case` 分派改写为等值 `if/else if` 链, 各 case 体逐字搬入; 基准里 case 之间发生**贯穿**的 (`SynInitPosDetect` 的 `case 3|4`、`SynTunePGZero_No_Load` 的 `case 7|default`、`SynTunePGZero_Load` 的 `case 3|4`、`SynTuneBemf` 的 `case 1|4`) 合成一条 `(A || B)` 判据, 语义仍是"两个取值走同一段代码"; 无体的 `case`/`default` 不生成分支; (2) `SetIPMPos` / `SetIPMPos_ABZRef` 里两处**逐字符相同**的 QEP 计数式抽为 `static Uint QepCountFromPos(Uint)`, 表达式与 `Uint` 截断点一字未改, 调用点仍在 `DINT` 之前 (临界区长度不变); 同时删掉 `m_Pos` 这个只是形参副本、两次读之间无写入的局部量; (3) `SynCalLabAndLbc` 的两处 `? :` 赋值拆成 `if/else` (两臂同左值同类型); (4) `SynInitPosDetCal` 三条 `Cur[0/2/4] += Cur[0/2/4+6]` 合成一个步长 2 的循环 —— 读集合 {6,8,10} 与写集合 {0,2,4} 不相交, 属全域等价; (5) `SynTunePGZero_No_Load` 的 `case 1` 把 `if(...){...; break;}` 加尾随 `if/else` 改为 `if/else` 嵌套 (两条路径在 `DetectCnt==0` 时都跳过尾随 `if`); (6) `SynTunePGZero_Load` 的 `case 1` 把 `if(PGMode==0){ ; }` 的空臂取反为 `if(PGMode != 0)`, 写保留; `case 4` 两条唯一语句逐字符相同的 `IdSubStep++` 臂合并为一条 `||` 判据; `case 5` 把三条并列判据共有的前半条件 `PGType == PG_TYPE_UVW` 提为外层守卫; (7) `DetectZeroPosOnce` 把 `DetectCnt == 2000` 的两支极性反转; (8) `SynInitPosDetSetPwm` 直接使用形参 (形参在本函数内只读), 删除副本; (9) `SynInitPosDetSetTs` **一字未改** —— 6 条 EPwm 寄存器写的顺序是硬件时序的一部分。**D19 硬边界: 所有处于 16 位截断点上的算术表达式 (移位量、截断宽度、运算顺序、显式/隐式转换) 逐字符不动**, 本模块没有把"两行各自截断"合并成"一行只截断一次"。 | 20,145 | **0** | 219 / 219 抓到 | -104 | 一致 |
| `src/motor/MotorParChange.c` | 参数变更/派生量计算模块。5 个入口: 逆变器档位与整定 `SystemParChg2Ms`、运行态派生量 `RunStateParChg2Ms`、编码器模式重配 `SystemParChg05Ms`、电机参数归一化 `ChangeMotorPar`、功率/转矩反算 `InvCalcPower`。**本模块不自有任何文件级对象** —— 它读写的 33 个全局量全部属于别的模块, 写的 3 组外设是芯片的, 所以外部符号集合天然稳定。改写手法 (441 -> 526 行, 14855 -> 16719 字节): (1) `SystemParChg2Ms` 的六段电压带 `if/else if` 链改成 6 行常量表 `s_InvBand[]` + 线性扫描, 220T 档"强制 GPType=1"用表里的 `m_GpForce` 表达; (2) 死区档位先算出下标 (`m_DeadIdx`, 400 档特判为 13) 再统一查表, 1140V 档仍走 `DBTIME_1140V*DeadTimeSet/10` 公式; (3) `FreqUint` 的三段 `if/else` 改成 3 行系数表 `s_FreqCoeff[][]` + 下标钳位 (`FreqUint` 是 2 位位域, 取值 0..3, 钳到 2); (4) `RunStateParChg2Ms` 的过压上限五段链改成 5 行常量表 `s_OvBand[]` + 扫描; (5) 三处 `((Ullong)x * FullFreq01 + (1<<14)) >> 15` 抽成 `s_FreqToReal()`, 两处 `*18641>>10` / 三处 `*180>>15` 抽成 `s_PosScale()` / `s_DegScale()`; (6) `ChangeMotorPar` 里三处 `(x*Current)/Votage*18597>>14` 抽成 `s_ResScale()`, 四处 `((L<<11)+BaseL)>>1/BaseL` 抽成 `s_IndScale()`; (7) `SystemParChg05Ms` 的 QEINT 两个分支合成"先算一个值再写一次"; (8) `InvCalcPower` 的 `StartStop` 门控先取一次标志位 `m_Run`; (9) 三个无用局部量 (`SystemParChg2Ms` 的 `m_Long`、`InvCalcPower` 的 `temp`) 与 `#if 0` 死块删除。**D19 硬边界: 所有处在 16 位截断点上的算术表达式逐字符不动** —— 移位量、截断宽度、运算顺序、显式/隐式转换一律保持原样, 例如 `gMotorExtInfo.Rpm * gMotorExtInfo.Poles * 6830L` 里前两个 `Uint` 的乘法在目标端是 16 位、会回绕, 换个写法就改结果; `(gMotorInfo.Votage * 10)` 同理。 | 20,207 | **0** | 194 / 194 抓到 | -93 | 一致 |
| `src/function/f_runSrc_accDecFrq.c` | 加减速频率规划模块。4 个入口: 主入口 `AccDecFrqCalc(accTime, decTime, mode)` (f_runSrc.c 的 9 处活调用)、两条 S 曲线状态机 `SCurveOridinaryCalc` / `SCurveSpecialCalc`、直线插补 `LineChangeCalc`(只经 `frqLine.calc` 函数指针调用)。`LOCALF`/`LOCALD` 在 `include/function/f_debug.h:27-33` 的 `#if 1` 分支里分别展开成**空**与 `extern`, 所以两条 S 曲线函数在目标端是外部链接的。改写手法 (615 -> 686 行, 15379 -> 15833 字节): (1) 六项 `&&` 守卫链改成 `s_AccDecUseRun()` 提前 return, **短路顺序逐项保持**; (2) ACC/DEC 分叉抽成 `s_AccDecIsAcc(frq0)`; (3) `accDecBenchmark` 的 if/else-if 改成 `s_BenchOverride()` 里的 switch; (4) S 曲线降级判据抽成 `s_CurveDowngrade(mode, accDecTime)`; (5) 三路派发改成 `switch (mode)`, `#if` 结构照原样保留; (6) 三处 `while (tmp >= 1L << 18) { tmp >>= 1; q[x]--; }` 抽成 `s_Fit18(int64, Uint16*)`; (7) 段斜率三分支改 switch, `frqTmpFrac` 的 if/else 抽成 `s_Q15Rescale(int64, Uint16)`; (8) `SCurveSpecialCalc` 的四路拐点选择改成"先取有效源再判符号"; (9) ACC/DEC 计数步进抽成 `s_TickUp()` / `s_TickDown()`; (10) `START_S` 的分子/分母写两遍抽成 `s_SbStep()`, `LINE` 写两遍抽成 `s_SbLine()`; (11) 上冲/下冲钳位抽成 `s_SbClamp(frq0, old)`; (12) `LineChangeCalc` 的嵌套 if 改成提前 return。**逐字节保持的截断点**: `Uint16 lineTime = 2000 - startTime - endTime;` 的截断、`benchFrq` 的三条赋值路径 (`ABS_INT32(frqCurAim)` / `maxFrq` / `100*decNumber[frqPoint]`, 后两条都截到 `Uint16`)、所有 `int64` 中间量 (`tmp3`/`tmp1`/`remainder`/`rem1`)、`(tmp*tmp)` 与 `tickerAll2*B*b` 在 **int32** 宽度上的乘法、两个入口分支里的 `Uint32 u`。 | 539,530 | **0** | 198 / 198 抓到 | -6 | 一致 |
| `src/function/f_osc.c` | 上位机示波器模块 (SCI 命令通道 + 波形采集回传)。3 个外部链接入口: `OscSciFunction`(f_main.c:714 主循环每拍一次)、`OscControlRead` / `OscControlWriter`(只被本模块的 FC_READ_FC_DATA / FC_WRITE_FC_DATA 分支调, 因 `f_osc.c:21-22` 先给原型且 FC_CODE_CONTROL_EN 为 1, 所以是外部链接, driver 能直接驱动)。其余 14 个函数全是 static。改写手法 (747 -> 746 行, 15954 -> 14416 字节): (1) `InitOsc` 的 9 条逐字段赋值改成表驱动 —— `static const DSP_OSC_CON_DATA OscCongDefault` + 整体赋值; (2) `InitSciOSC` 里逐个 `PIEIER9.bit.INTxN = 0/1` / `PIEIFR9.bit.INTxN = 0` 改成位掩码 RMW (`PIEIER9.all &= ~OSC_PIE_RX_MASK` 等, 其他位不受影响); `BautRateCfg[baudRate-1]` 只算一次存进 `baud`; (3) `SciDataTX` / `sciaTxFifoIsr` / `sciaRxFifoIsr` 里 4 条展开的 `*buf++` 推/取改成 `for (i = 0; i < OSC_SCI_CHUNK; i++)` 循环; (4) `SciDataRxDeal` 的 FC_CHANNL_SEL: 4 组「赋值 + 条件自增」改成先把 P1..P4 收进 `sel[4]`、循环数出 `sum`、再一次性写 4 个通道 + chSum; (5) `SciDataRxDeal` 的 CRC 校验把内联拼接提进 `crcFrame` 再比 (**拼接表达式逐字保留, 含 `(Uint16)` 转换的位置**); (6) `OscDataQcq` / `OscDataTxDeal` 的 A/B 两段几乎逐字重复的分支改成`buf = (bufSel == 0) ? &OscDataBufA : &OscDataBufB` + 单份主体, 回绕动作 `bufSel = (bufSel == 0) ? 1 : 0` (对 bufSel 的**全部取值**都与原写法同值); (7) `GetOscData` 的 4 段重复通道块改成 `addr[4]` + 循环, OSC_GET_CHECK 宏原样保留; (8) `ClearOscBufEnd` 的 8 条赋值改成 `buf[2]` + 双层循环 (k=1..4 -> 下标 63/62/61/60); (9) `OscSciIoInit` 的两条 GPAPUD 位写改成 `GPAPUD.all &= ~((Uint32)0x3 << 28)` (GPADAT_BITS 里 bit28=GPIO28、bit29=GPIO29); (10) `SciDataRx` 的超时判据 `(count > timeOut) && (timeOut != 0)` 交换成 `(timeOut != 0) && (count > timeOut)` (两侧都无副作用); 采样节拍 `if (count == interval) { ... }` 改成 `if (count != interval) return;` + 尾部 count=0; (11) `OscControlRead` 去掉被立刻覆盖的 `oscReturn = 0` 死初始化。**逐字节保持的截断点**: `CONTROL_FRAME_P3 = err;` **不掩码** (uint8 在 C28x 上 是 16 位, 整个 err 留在 P3), `rwPI += chSum << 1`、`len -= 4`、`frame++`、`chSum` 自增、`SciTxBufStr.len = len - 4` 的全部 uint8 宽度。 | 29,123 | **0** | 201 / 201 抓到 | -73 | 一致 |
| `src/motor/MotorImParEst.c` | 异步电机参数辨识模块 (Rs -> Rr/L0 -> Lm/I0 -> 电流环 PI)。15 个外部链接入口全部无参数: `RsIdentify` / `RrLoIdentify` / `LmIoIdentify` (三台状态机, 靠 `gGetParVarable.IdSubStep` 推进, 段间由 `ParEstMstep` 交接), `ChgParForEst`, `LmIoInPeriodInt` (MotorMain.c:104 周期中断), `RrLoInPeriodAndCmpInt` (**唯一的中断入口**, 14 点采样窗口, 向量表在 MotorMain.c:756), `SetPwmForRrLoId` / `InitRrLoVariable` / `EnableTimer` / `RrWaitAddGetUdc` / `RrGetVoltReBegin` / `RrLoCalVaule` (Rr 段自调), `ImCalKpKi` / `InitLmIoVaribale` / `LmIoCalValue` (Lm 段)。模块**不定义任何全局量** —— 目标端 nm2000 给出 41 个未定义符号 (3 个编译器运行时 / 7 个外设结构体 / 6 个被调者 / qsin / qsqrt / 23 个全局量), 这决定了测试台的形状 (整幅输入映像, 见 invariants)。改写手法 (832 行 23560 字节 -> 827 行 20476 字节, .text 1942 -> 1920): (1) `RsIdentify` case 1 的电压类型三段 if/else-if/else 改成 switch (1140V=5 / 690V=3 / default); (2) case 4 的两道独立门槛 `>=512` / `>=1024` 改成嵌套 (1024 蕴含 512, 两个体都照旧执行); (3) case 3/5 的三次「先算 m_UData1 再覆盖」改成先选**分子因子** (1024/4096/3500) 再算一次, `Comper += if/else` 改成三元; (4) case 6 的逐位右移双变量循环改成「先数移位次数、再一次性移位」(Ulong 右移 k 次 == 一次右移 k 位); (5) `RrLoIdentify` case 2/3 里两处**逐字相同**的 13 行等待块抽成 `static void RrLoWaitTick(void)` (static 在两侧 TU 各有副本, 不会撞名也不会被链接器折叠); (6) case 3 的三处 `WaitCnt=20; RrGetVoltReBegin();` 收成一个 `m_Repulse` 标志; R2 系数的两段 if/else-if 改成系数变量 `m_R2Coff` + 一次条件乘法 (0 表示不改, 逐字对应原文的「两个分支都不进」); (7) `SetPwmForRrLoId` 的 6 组 `#ifdef DSP_CLOCK100` / `#ifdef DSP_CLOCK60` 收成 2 个宏 `IM_PWM_CLK_DIV` / `IM_PWM_HSPCLK_DIV`, 6 处 CLKDIV 写操作引用宏; (8) `InitRrLoVariable` 的 for 清零改成 while; (9) `LmIoIdentify` 的 case1->case2 隐式 fallthrough 改成前置 if + switch 从 case 2 起 (IdSubStep==1 时先 InitLmIoVaribale + 自增, 再落进 case 2 —— 与原 fallthrough 逐拍同值); case 3 的三处 `break`(跳出 switch) 改成 `m_Hold` 标志, `maxFrqErr` 提到函数作用域 (类型仍是 int, 赋值表达式逐字保留); (10) `LmIoInPeriodInt` 的复合 `if((a)||(b)) return;` 拆成两个独立早退; (11) `RrLoCalVaule` 两个**同区间**的 for (一个累 m_IsUdcVoltDiff、一个累 m_IsZeroVoltDiff) 合并成一次遍历; (12) `ChgParForEst` 的 PM / 非 PM 两条几乎逐字的 if/else-if/else 链改成一次算 `m_FcSet` + `m_HighRange` 标志, 两条支共用 (非 PM 支第三档就是 30, PM 支第三档是「保住原有的 >=30」—— 这个不对称正是必须用标志而不能用 `m_FcSet` 直接赋值的理由)。**刻意没动的六个函数**: `EnableTimer` / `RrGetVoltReBegin` (纯寄存器写序列, 没有可重写的控制流, 重排只会在硬件上引入时序风险而不改变任何可观测结果 —— 那是假的改写), `RrLoInPeriodAndCmpInt` (ISR, 14 点窗口 / __IQsat 饱和 / CBCLimitCurPrepare 的调用时机 / DINT 与 PIEACK 的先后每一条都对着硬件, 收益为零风险最高), `ImCalKpKi` / `LmIoCalValue` (稠密定点算术, `L0*LM` 的 16 位截断与 `(m_ComIm*m_ComIm)>>13` 的溢出边界都是改一个字就变语义的地方), `RrWaitAddGetUdc` (超时循环圈数可观测, 改成 for 会多读一次完成标志)。 | 24,299 | **0** | 356 / 356 抓到 | -22 | 一致 |
| `src/motor/MotorVCMain.c` | 重复片段提为 static 帮助函数 (ResetAsrTotals / ResetVoltOut / PrepareTorqueLimitPid / CalAsrKi / PrepareUdcPid / VcLowFreqHold / SnapshotImTs / LimitVoltAmp / ClearVoltWhenNotRun / SetVoltPhase)、两处 if/else 取反改提前返回、<<7/<<8 与 <<9/<<10 的双支改为先定移位再统一计算、插值系数两处除法合并为 CalAsrKi | 86,945 | **0** | 304 / 304 抓到 | -81 | 一致 |
| `src/motor/MotorVF.c` | VVVF (恒压频比) 控制模块。16 个外部链接入口: VfVarInitiate / VFOVUdcLimit / CalOutVotInVFStatus / CalTorqueUp / VFWsTorqueBoostComm / VFAutoTorqueBoost / VFWSCompControl / VFOverMagneticControl / VFSpeedControl / VfOverCurDeal / VfOverUdcDeal / VfFreqDeal / VfOscIndexCalc / HVfDeadBandComp / HVfOscDampDeal / HVfCurReDecomp, 全部无参数 (CalOutVotInVFStatus 收一个 int freq)。改写手法 (1020 -> 963 行): (1) VfVarInitiate 的正/负相序 if/else 改成三元; (2) VFOVUdcLimit 把 `if(A && B) { 计数 } else { 清零 }` 改成 De Morgan 的提前返回 (`if(!A || !B) { 清零; return; }`), 计数与钳位留在尾部; (3) CalOutVotInVFStatus 的尾段 `if(|f|==0){...} else if(|f|>=FreqPer){...}` 改成两个独立提前返回, 两处钳位各自 `return ((a > b) ? b : a)`; (4) CalTorqueUp 的 `if(skip){...} else { 主体 }` 改成提前返回, 且把`(VFTorqueUp==0)&&(VFOvShock==0)` 按 De Morgan 取反成 `(VFTorqueUp!=0)||(VFOvShock!=0)`, 把斜面支提到 PID 支之前 (两支互补, 逐输入同支), `m_VoltUp = 0` 提到函数头、`detVfVoltUp` 的写入提出 else (skip 支显式写 0); (5) VFWsTorqueBoostComm 的 `Coff > 1400` 支改成提前返回, 角度符号的 if 改成三元; (6) VFAutoTorqueBoost 的三档 m_LowerLimit 改成嵌套三元, 内层 `先赋 EsVolt 再按 <950 覆盖` 改成三元, PID.Deta 的三支 (>>5 无掩码 / >>3 &0xFFFC / >>1 &0xFFF0) 改成`(>>m_Shift) & m_Mask` 并令无掩码支的 m_Mask = -1L (long 上恒等); (7) VFWSCompControl 的 `if(speed_CON && VFWsComp){ if(DelayTime>100){...} else DelayTime++ } else DelayTime=0` 改成三段 if/else-if/else (门取反提前、自增支提中); (8) VFOverMagneticControl 的 m_UDCDesired 改成三元; (9) VfOverCurDeal 的 SubStep/m_AddStep、MaxStepLow/MaxStepHigh、CurLim、m_MaxStep 四处 if/else 改成三元 (CurLim 的 `15 == InvTypeApply ? 170 : ocPoint` 先选操作数再统一 `(long) <<12 / 100`); (10) VfOverUdcDeal 的 m_Add1/m_Add2 改成三元, `if(m_DetaU<0) += m_Add2 else += m_Add1` 改成三元增量, 并删掉一段**死代码** (见 dead_code 第一条); (11) VfFreqDeal 的 m_Dir 与 `m_Deta = -m_Deta; if(FreqApply==0) m_Deta=0` 改成三元; (12) HVfDeadBandComp 的三段 `CompU / CompV / CompW` 逐字重复块改成`int *comp[3]` + 一个三拍循环 (每拍末尾 `phase -= 21845`, 与原写法逐拍一致). | 32,503 | **0** | 179 / 179 抓到 | -27 | 一致 |
| `src/function/f_comm.c` | SCI/LIN 串行通讯层。23 个外部入口: `SciDeal`(主循环轮询), 两个 SCI 中断 `SCI_RXD_isr` / `SCI_TXD_isr`, 两个 LIN 中断包装 `Lina_Level0_ISR` / `Lina_Level1_ISR`, 报文分发 `CommRcvDataDeal` / `CommSendDataDeal` / `SciErrCheck` / `commStatusDeal` / `commErrorDeal` / `CommDataReRcv` / `CommDataSend` / `CommStartSend`, funcCode 远程读写 `CommWrite` / `CommRead` / `CommRwFuncCode`, 以及八个真外设编程入口 `InitSciaGpio` / `InitSetScia` / `UpdateSciFormat` / `resetLinSci` / `closeRTX` / `setRxConfig` / `setTxConfig`。改写手法 (1197 -> 1172 行, 27640 -> 27329 字节): (1) `SciDeal` 的错误标志分派取反改提前返回, 30 s 密码超时门槛收成宏 `COMM_PWD_HOLD_TICK`; (2) `SCI_RXD_isr` 的两层嵌套 if 合成一个 `&&` (`StartDeal` 仍先求值, `rcvFlag` 仍只在它为真时读); (3) `Lina_Level0_ISR` 的 if/else-if/else 改 `switch` (11 / 12 / default); (4) `CommRcvDataDeal` 的 `if(!slaveAddr){...}else{...}` 拍平成 `if / else if (CRC) / else`, 命令字三分支改 `switch`, 尾部 `if (sciFlag.bit.write) {...}` 改提前返回; (5) `SciErrCheck` 的八段 if/else-if 优先级链改成顺序提前返回 (`operateErr` 局部量随之消失); (6) `CommSendDataDeal` 把 `sendNumMax - 2` 只算一次存进 `crcAt`, 第二字节写 `crcAt + 1`; (7) `CommWrite` 的八个地址常量 if/else-if 链改 `switch (addr)`, 三处逐字重复的 AO/HDO 钳位块 抽成 `static Uint16 commSetAo(Uint16, Uint16)`, `canOpen` 翻转改三元 + 取反; (8) `CommRead` 的 0x1000 支两臂互换 (先判在界内), 三段等值地址改 `switch`, 状态字正反转两臂与键盘判据各改三元, `(i+low)==DISP_OUT_CURRENT` 与 `commReadCurrentPoint` 合成一个 `&&`; (9) `CommRwFuncCode` 的组号计算改 `switch (highH)`, `gradeAdd` 改三元, 三层 OR 的禁写判据拆成顺序提前返回, 写支的三段 else-if 改独立 if + return, paraInit 的五段 `else if (... == data)` 链改 `switch (data)`, 尾段 EEPROM 判据改提前返回; (10) `commStatusDeal` 拆掉 `SCI_RECEIVE_OK -> SCI_SEND_DATA_PREPARE` 的**隐式 fallthrough**, 把准备发送的体抽成 `static void commSendPrepare(void)` 供两处调用; (11) `CommDataReRcv` 的第二个 `if (rcvNum >= rcvNumMax)` 改成 `if (rcvNum < rcvNumMax) return;`, `rcvNum++` 拆成显式两步; `CommDataSend` 同理改提前返回; (12) `InitSetScia` 与 `UpdateSciFormat` 里**逐字相同**的「CAN_OPEN 选 0, 否则选 commParity」两段收成 `static void setLinParity(void)`; (13) `UpdateSciFormat` 把 `commProtocolSec` 缓存进 `protoSec` (仅限四次 `GetNumberDigit1` 之前 —— 该函数只写 `digit[0..4]`, 对 `funcCode` 是纯的; `UpdateCommFormat` 之后的那次读仍读全局)。**刻意没动的八个入口**: `CommStartSend` (一行寄存器写), `resetLinSci` / `closeRTX` / `setRxConfig` / `setTxConfig` (纯寄存器写序列), `commErrorDeal` (四行赋值 + 一对 EALLOW/EDIS), `Lina_Level1_ISR` (一行 PIEACK), `InitSciaGpio` (两组 EALLOW/EDIS 里的 20 个位域写, 重排只会在硬件上引入时序风险而不改变任何可观测结果 —— 那是假的改写)。 | 24,766 | **0** | 128 / 128 抓到 | +5 | 一致 |
| `src/function/f_io.c` | IO 抽象层: 数字输入 / 数字输出 / 模拟量 / 脉冲 / 折线 / 低通 / 温度。34 个入口: `DiCalc` / `DoutDeal` / `DoCalc` / `AiCalc` / `AoCalc` / `AoCalcChannel` / `PulseInCalc` / `PulseInSample` / `PulseOutCalc` / `FMPDeal` / `UpdateFmpAoValue` / `LineCalc` / `LpfCalc` / `TemperatureDeal` / `SensorDeal` / `AfterDeal` / `GetTemperature` / `GetTemperatureCalc` / `Di2Bin` / `Do2Bin` / `getDiHwStatus` / `setDOStatus` / `InitDIGpio` / `InitDOGpio` 以及 9 个死入口。改写手法 (3188 -> 2516 行, 95688 -> 57668 字节): (1) `DiCalc` 的去抖 / 虚拟 DI / AI-as-DI / 延时四个循环改升序 (每个 `i` 只碰自己那一位), 新增 `static Uint16 ioPulseSourceConflict(void)` 与 `static void ioSetOrClear(Uint32 *, Uint32, Uint16)`, **末级 `fc1[i] -> pDiFunc` 的循环必须保持倒序** (见 invariants); (2) `DoutDeal` 的 41 段分支: `Dout2/38/41` 抽出 `static Uint16 ioErrorOutputActive(void)` (三处判据逐字相同), `Dout3/25` 抽出 `static void ioFrqFdtArrive(Uint16 *, Uint16, Uint16)`, `Dout1/6/7/19/36` 的 `if (x) f = 1;` 改 `|=` / 三元, `Dout4/26/27` 把 `band` / `now` / `aim` 提到条件外各算一次, `Dout8/9/10` 两段 if 合成一个 `||`, `Dout34` 改提前返回 + `limit` 只算一次; (3) `DoCalc` 新增 `static Uint16 ioDoFuncFlag(Uint16)`, `doCommIndex[]` 改 `static const`, 循环改升序; (4) `TemperatureDeal` / `SensorDeal` / `AfterDeal` / `GetTemperature` 改提前返回, `period` 提成局部量, `GetTemperature` 的 if/else-if 改 `switch`, `AfterDeal` 两处 if 改三元; (5) `GetTemperatureCalc` 的 `for + break` 改 `while`; (6) `AiCalc`: `aiMin` 的「第一轮为 `AI_MIN_VOLTAGE_HW*10`、其后为 0」改成局部标志 `aiLowFirst`, 折线搜索 `for` 改 `while`, 嵌套 if/else 拍平; (7) `AoCalcChannel`: `EPwmRegs` / `pECapRegs` 在声明处用三元选边, `paraIndex` 提出来复用; (8) `PulseInCalc`: `pulseSum` / `lengthSum` 各只算一次, `if (0 == captureTicker)` / `if (!p->t)` 统一成 `0 ==` 风格; (9) `PulseInSample`: 四个 `ts` 先各自累加到 `tsSum`, 末尾一次性加进 `capturePeriodSum`; (10) `PulseOutCalc`: 新增 `static Uint16 ioHdoCompareTrack(...)`, `HDOCTR = (HDOCTR > 292) ? 0xFFFF : (Uint16)(HDOCTR * 224);` 之类改三元; (11) `UpdateFmpAoValue`: 用局部 `value` / `limit` 累到最后统一写, `FRQ_SET`/`FRQ_AIM`/`SPEED` 三支合并、`OUT_CURRENT`/`OUT_CURRENT_1` 两支合并; (12) `LineCalc`: 删掉 `#if 0` / `#elif 1` / `#elif 1` 三层只留活的那一种, `if (p->mode == 1)` 提到最前; (13) `LpfCalc`: 删掉 `#if 0` 那层, 删掉**写了不用**的 `int32 out;`, `if (!p->t)` 改提前返回; (14) `InitDIGpio` / `InitDOGpio` / `setDOStatus` 的 `? :` 改 if/else, `getDiHwStatus` 的 `|=` 改 `=` 并加显式 `(Uint16)` 转换, `Di2Bin` / `Do2Bin` 的条件合成一个局部 `changed`; (15) 顶部新增四个宏 `AI_AS_DI_LOW` / `AI_AS_DI_HIGH` / `MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA` / `LINE_SPEED_CALC_PERIOD`。**刻意没动的九个死入口**: `PulseOutDeal`、`InitSetEcap2/3/4`、`InitECap2Gpio/3Gpio/4Gpio` —— 它们不在 `.lkf` 的构建清单里, 改了既无证据也无收益, 逐字保留。 | 56,213 | **0** | — | -113 | 一致 |

## 2. 层级 A — 逐模块反汇编

- 编译: **118 / 118** 个目标文件成功 (原/新各 59), 失败 **0**
- 有代码模块: **8 相同 / 49 不同**
- 纯数据模块 (本条不适用, 转由层级 B 覆盖): `DSP2803x_GlobalVariableDefs`, `MotorVar`

不一致模块:

- `DSP2803x_CpuTimers`: first diff line 3: 'TEXT Section .text (Little Endian), 0x72 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x57 words at 0x00000000 '
- `f_canlink`: first diff line 3: 'TEXT Section .text (Little Endian), 0x2bf words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x296 words at 0x00000000 '
- `f_comm`: first diff line 3: 'TEXT Section .text (Little Endian), 0x66a words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x66f words at 0x00000000 '
- `f_comm_DP`: first diff line 3: 'TEXT Section .text (Little Endian), 0x1dc words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x1d2 words at 0x00000000 '
- `f_common`: first diff line 3: 'TEXT Section .text (Little Endian), 0x4d words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x4c words at 0x00000000 '
- `f_debug`: first diff line 3: 'TEXT Section .text (Little Endian), 0xa2 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x78 words at 0x00000000 '
- `f_dspcan`: first diff line 3: 'TEXT Section .text (Little Endian), 0x25c words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x244 words at 0x00000000 '
- `f_eeprom`: first diff line 3: 'TEXT Section .text (Little Endian), 0x51c words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x52b words at 0x00000000 '
- `f_error`: first diff line 3: 'TEXT Section .text (Little Endian), 0x4cb words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x499 words at 0x00000000 '
- `f_fcDeal`: first diff line 3: 'TEXT Section .text (Little Endian), 0x300 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x31f words at 0x00000000 '
- `f_frqSrc`: first diff line 3: 'TEXT Section .text (Little Endian), 0x9b6 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x9b4 words at 0x00000000 '
- `f_frqSrc_pid`: first diff line 3: 'TEXT Section .text (Little Endian), 0x6ab words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x6b6 words at 0x00000000 '
- `f_invPara`: first diff line 3: 'TEXT Section .text (Little Endian), 0x378 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x379 words at 0x00000000 '
- `f_io`: first diff line 3: 'TEXT Section .text (Little Endian), 0x11ab words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x113a words at 0x00000000 '
- `f_main`: first diff line 3: 'TEXT Section .text (Little Endian), 0x45a words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x45f words at 0x00000000 '
- `f_menu`: first diff line 3: 'TEXT Section .text (Little Endian), 0x119b words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x11a4 words at 0x00000000 '
- `f_modbus`: first diff line 3: 'TEXT Section .text (Little Endian), 0x123 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x11b words at 0x00000000 '
- `f_motorFc`: first diff line 3: 'TEXT Section .text (Little Endian), 0x7d words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x7f words at 0x00000000 '
- `f_osc`: first diff line 3: 'TEXT Section .text (Little Endian), 0x36a words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x321 words at 0x00000000 '
- `f_p2p`: first diff line 3: 'TEXT Section .text (Little Endian), 0x9a words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x9f words at 0x00000000 '
- `f_plc`: first diff line 3: 'TEXT Section .text (Little Endian), 0x1fe words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x215 words at 0x00000000 '
- `f_profibus`: first diff line 3: 'TEXT Section .text (Little Endian), 0x2a4 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x2ac words at 0x00000000 '
- `f_runSrc`: first diff line 3: 'TEXT Section .text (Little Endian), 0xcc0 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0xcbb words at 0x00000000 '
- `f_runSrc_accDecFrq`: first diff line 3: 'TEXT Section .text (Little Endian), 0x60c words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x606 words at 0x00000000 '
- `f_torqueCtrl`: first diff line 7: '00000002   7640   LCR          0x00008f' vs '00000002   7640   LCR          0x00008e'
- `f_ui`: first diff line 3: 'TEXT Section .text (Little Endian), 0x48f words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x48e words at 0x00000000 '
- `main`: first diff line 3: 'TEXT Section .text (Little Endian), 0x61 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x5f words at 0x00000000 '
- `MotorCarrier`: first diff line 3: 'TEXT Section .text (Little Endian), 0x39d words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x3a4 words at 0x00000000 '
- `MotorCurrentTransform`: first diff line 3: 'TEXT Section .text (Little Endian), 0xdb words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0xf2 words at 0x00000000 '
- `MotorDataExchange`: first diff line 3: 'TEXT Section .text (Little Endian), 0x4eb words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x4c2 words at 0x00000000 '
- `MotorDebug`: first diff line 3: 'TEXT Section .text (Little Endian), 0x10f words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0xe0 words at 0x00000000 '
- `MotorEncoder`: first diff line 3: 'TEXT Section .text (Little Endian), 0x5e7 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x58d words at 0x00000000 '
- `MotorImParEst`: first diff line 3: 'TEXT Section .text (Little Endian), 0x796 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x780 words at 0x00000000 '
- `MotorInfoCollect`: first diff line 3: 'TEXT Section .text (Little Endian), 0x129 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x12a words at 0x00000000 '
- `MotorInvProtect`: first diff line 3: 'TEXT Section .text (Little Endian), 0x612 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x69d words at 0x00000000 '
- `MotorMain`: first diff line 3: 'TEXT Section .text (Little Endian), 0x5fc words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x5f5 words at 0x00000000 '
- `MotorParChange`: first diff line 3: 'TEXT Section .text (Little Endian), 0x5bf words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x562 words at 0x00000000 '
- `MotorParEst`: first diff line 3: 'TEXT Section .text (Little Endian), 0x167 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x171 words at 0x00000000 '
- `MotorPmsmMain`: first diff line 3: 'TEXT Section .text (Little Endian), 0x3e3 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x3fe words at 0x00000000 '
- `MotorPmsmParEst`: first diff line 3: 'TEXT Section .text (Little Endian), 0x98c words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x924 words at 0x00000000 '
- `MotorPublicCal`: first diff line 3: 'TEXT Section .text (Little Endian), 0x2ef words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x2e9 words at 0x00000000 '
- `MotorPWM`: first diff line 3: 'TEXT Section .text (Little Endian), 0x254 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x248 words at 0x00000000 '
- `MotorSpeedCheck`: first diff line 3: 'TEXT Section .text (Little Endian), 0x2e7 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x2d6 words at 0x00000000 '
- `MotorSVC`: first diff line 3: 'TEXT Section .text (Little Endian), 0x38d words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x359 words at 0x00000000 '
- `MotorVCMain`: first diff line 3: 'TEXT Section .text (Little Endian), 0x8e7 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x896 words at 0x00000000 '
- `MotorVF`: first diff line 3: 'TEXT Section .text (Little Endian), 0x9c9 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x9ae words at 0x00000000 '
- `ZeroPosCtl`: first diff line 3: 'TEXT Section .text (Little Endian), 0x44 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x4c words at 0x00000000 '
- `SubPrg`: first diff line 3: 'TEXT Section .text (Little Endian), 0x23b words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x237 words at 0x00000000 '
- `mainInit_2803x`: first diff line 3: 'TEXT Section .text (Little Endian), 0x192 words at 0x00000000 ' vs 'TEXT Section .text (Little Endian), 0x196 words at 0x00000000 '

比对细节: `dis2000` 必须 `cd` 进目标目录并用**裸文件名**调用 ——
否则输出第 2 行会回显传入路径, 造成假差异; 同时必须断言产物非空。

## 3. 层级 B — 目标文件内容级比对 (覆盖全部 59 个模块)

**方法**: 把两侧源码复制到**同一个暂存路径**后编译, 使目标文件里内嵌的源文件
绝对路径完全一致; 再按 COFF 结构取摘要比对:

1. 每个段: 名字 / 大小 / 是否装载 / 标志 / 重定位条数 / **段内容 SHA256** / **重定位区 SHA256**
2. 符号表 (18 字节/项, 跳过 aux 与 `.file`): 名字 / value / scnum / type / sclass
3. 忽略: 文件头 4 字节时间戳、文件内偏移、字符串表本身

**为什么不直接 `cmp` 两个 `.obj`**: 时间戳与内嵌源路径(新工程换了目录, 长度必然不同)
会污染结果 —— 实测会报出上万条与语义无关的 "差异"。

- 结果: **10 / 59 个模块内容完全一致**

不一致:

- `DSP2803x_CpuTimers` DIFFERS: {"msgs": ["段 .text: size=114/87 data=61d25a45/ad4bf9ff reloc=18920dd7/25071473 nreloc=38/11", "符号表差异: 仅A=[('_InitCpuTimers', 0, 2, 4, 2), ('_ConfigCpuTimer', 65, 2, 4, 2), ('_CpuTimer1Regs', 0, 0, 328
- `f_canlink` DIFFERS: {"msgs": ["段 .text: size=703/662 data=8b68ccf7/2239b392 reloc=a7f376dc/780d27d7 nreloc=155/135", "段 .cinit: size=24/24 data=5b6063e6/5b6063e6 reloc=27f17cbd/09312981 nreloc=6/6", "段 .cinit:_CanLinkCha
- `f_comm` DIFFERS: {"msgs": ["段 .text: size=1642/1647 data=09b8fb07/c6680abe reloc=f8b1d9f8/ea7cbb3d nreloc=360/333", "段 .cinit: size=4/4 data=8d3c7015/8d3c7015 reloc=c0ab1ee8/47283a6c nreloc=1/1", "段 .econst:_protocolF
- `f_comm_DP` DIFFERS: {"msgs": ["段 .text: size=476/466 data=df448025/8068694b reloc=02296d42/c3784525 nreloc=92/76", "段 .cinit: size=14/14 data=18193eb1/18193eb1 reloc=70fc61cc/c31f6e3c nreloc=4/4", "段 .text:retain: size=2
- `f_common` DIFFERS: {"msgs": ["段 .text: size=77/76 data=9a75cd9f/038068e8 reloc=57da1d88/2c59b030 nreloc=1/1", "符号表差异: 仅A=[('$C$L1', 10, 2, 4, 6), ('$C$L2', 31, 2, 4, 6), ('_Reverse8Bit', 52, 2, 4, 2), ('_Reverse4Bit', 3
- `f_debug` DIFFERS: {"msgs": ["段数 22 vs 26", "段 .text: size=162/120 data=acc8c2ae/94c6fe1e reloc=c64f3eaa/867c3139 nreloc=26/20", "段 .cinit:_scopeInterval: size=9/9 data=1e0f1b67/1e0f1b67 reloc=d0bc9cd7/b60ac986 nreloc=1
- `f_dspcan` DIFFERS: {"msgs": ["段 .text: size=604/580 data=6e0482e0/0cd0fa69 reloc=b9715dea/b0dffc10 nreloc=99/98", "符号表差异: 仅A=[('$C$L3', 69, 2, 4, 6), ('$C$L4', 80, 2, 4, 6), ('$C$L5', 89, 2, 4, 6), ('$C$L6', 98, 2, 4, 6
- `f_eeprom` DIFFERS: {"msgs": ["段 .text: size=1308/1323 data=796fec0b/6c984a92 reloc=40a0e994/cb92b4f6 nreloc=390/390", "符号表差异: 仅A=[('$C$L9', 123, 2, 4, 6), ('$C$L10', 127, 2, 4, 6), ('$C$L11', 133, 2, 4, 6), ('$C$L12', 1
- `f_error` DIFFERS: {"msgs": ["段 .text: size=1227/1177 data=af6f24ea/082da187 reloc=839b9c3e/a6b22202 nreloc=460/428", "段 .cinit: size=8/8 data=5ba196ba/5ba196ba reloc=b89b6264/f3dccf0c nreloc=2/2", "符号表差异: 仅A=[('_devTcn
- `f_fcDeal` DIFFERS: {"msgs": ["段数 7 vs 9", "段 .text: size=768/799 data=8a7b4e77/b7d794a9 reloc=5c2b5493/d23eeccd nreloc=175/153", "段 .econst:_limitOtherCodeIndex: size=13/6 data=d4ea33a7/f68eeee0 reloc=-/- nreloc=0/0", "
- `f_frqSrc` DIFFERS: {"msgs": ["段 .text: size=2486/2484 data=c25ce053/a8f7c626 reloc=21d28d2b/defcae08 nreloc=982/982", "符号表差异: 仅A=[('_frqDeltaDiRemainder$3', 23, 7, 32896, 6), ('$C$L221', 2226, 2, 4, 6), ('$C$L222', 2232
- `f_frqSrc_pid` DIFFERS: {"msgs": ["段 .text: size=1707/1718 data=7717d16e/5c526216 reloc=4e84517e/1df1dd51 nreloc=311/311", "段 .cinit: size=15/15 data=47822682/0cc696d1 reloc=f86adf28/2554b40f nreloc=6/6", "段 .cinit:_pidLpf: 
- `f_invPara` DIFFERS: {"msgs": ["段 .text: size=888/889 data=1cc54f99/f9cc3eea reloc=f0f66142/e97620de nreloc=269/258", "段 .cinit:_pInvTypeParaTable: size=15/15 data=7014d4b1/7014d4b1 reloc=f2a15658/1fc9e5f2 nreloc=7/7", "符
- `f_io` DIFFERS: {"msgs": ["段 .text: size=4523/4410 data=4b9599b5/5fcaf126 reloc=92d5204f/09c5df6b nreloc=1388/1251", "段 .cinit: size=21/21 data=d3518da0/233958d1 reloc=3fb8e71a/0114e4c0 nreloc=6/6", "段 .cinit:_aiLpf:
- `f_main` DIFFERS: {"msgs": ["段 .text: size=1114/1119 data=0975a7f4/b604cd2e reloc=a315a654/f860cc3c nreloc=372/371", "符号表差异: 仅A=[('$C$L44', 1097, 2, 4, 6), ('$C$L45', 1101, 2, 4, 6), ('_getMomeryValue', 1102, 2, 32896,
- `f_menu` DIFFERS: {"msgs": ["段 .text: size=4507/4516 data=9f9e404f/65c397b0 reloc=0435b38e/13f727de nreloc=1280/1275", "段 .cinit: size=5/5 data=1818e951/1818e951 reloc=bce3040b/0b1db744 nreloc=2/2", "段 .econst:_menu: s
- `f_modbus` DIFFERS: {"msgs": ["段 .text: size=291/283 data=b2280870/b6d0a75c reloc=2f333d81/150c6ab8 nreloc=90/80", "符号表差异: 仅A=[('$C$L1', 50, 2, 4, 6), ('$C$L2', 67, 2, 4, 6), ('$C$L3', 82, 2, 4, 6), ('$C$L4', 85, 2, 4, 6
- `f_motorFc` DIFFERS: {"msgs": ["段 .text: size=125/127 data=68503772/97445614 reloc=c41ffd9e/26800748 nreloc=46/46", "符号表差异: 仅A=[('$C$L1', 16, 2, 4, 6), ('$C$L2', 22, 2, 4, 6), ('$C$L3', 62, 2, 4, 6), ('$C$L4', 78, 2, 4, 6
- `f_osc` DIFFERS: {"msgs": ["段数 9 vs 10", "段 .text: size=874/801 data=16c18f2c/8fd422e2 reloc=82299140/acae687a nreloc=360/260", "段 .cinit: size=49/49 data=1efeadfd/1efeadfd reloc=61ed33e9/097b69f8 nreloc=13/13", "段 .e
- `f_p2p` DIFFERS: {"msgs": ["段 .text: size=154/159 data=552ff809/d8b8d053 reloc=0f1cdd00/0a029fb5 nreloc=67/67", "符号表差异: 仅A=[('$C$L1', 10, 2, 4, 6), ('$C$L3', 23, 2, 4, 6), ('$C$L4', 37, 2, 4, 6), ('$C$L5', 54, 2, 4, 6
- `f_plc` DIFFERS: {"msgs": ["段 .text: size=510/533 data=1933daf3/fc247a4d reloc=2bf00e95/7a0e6453 nreloc=36/28", "段 .cinit: size=8/8 data=cc6dd325/cc6dd325 reloc=bea859c5/9b6c3dfa nreloc=2/2", "段 .cinit:_InvTrFastTab: 
- `f_profibus` DIFFERS: {"msgs": ["段 .text: size=676/684 data=ff31ee47/4211e540 reloc=48c6894b/bd4a80d4 nreloc=218/220", "符号表差异: 仅A=[('$C$L16', 309, 2, 4, 6), ('$C$L17', 357, 2, 4, 6), ('$C$L18', 371, 2, 4, 6), ('$C$L19', 38
- `f_runSrc` DIFFERS: {"msgs": ["段 .text: size=3264/3259 data=7f4d7d7e/edbd2a06 reloc=25da3879/20835a24 nreloc=1375/1369", "段 .cinit: size=14/14 data=c168cf36/c168cf36 reloc=80053925/62b19e51 nreloc=3/3", "符号表差异: 仅A=[('_bM
- `f_runSrc_accDecFrq` DIFFERS: {"msgs": ["段 .text: size=1548/1542 data=2dd64846/285619de reloc=70773bc8/422e87d0 nreloc=430/371", "段 .cinit: size=5/5 data=8e18ed01/16050a3a reloc=c0c064c7/4dd4094c nreloc=2/2", "符号表差异: 仅A=[('$C$L1',
- `f_torqueCtrl` DIFFERS: {"msgs": ["段 .text: size=290/290 data=b6c68ad2/ba6b090d reloc=d2293318/f885b7f0 nreloc=95/95", "符号表差异: 仅A=[('$C$L4', 41, 2, 4, 6), ('$C$L5', 47, 2, 4, 6), ('$C$L6', 54, 2, 4, 6), ('$C$L7', 74, 2, 4, 6
- `f_ui` DIFFERS: {"msgs": ["段 .text: size=1167/1166 data=00c2a387/b4dc7d37 reloc=35b725ed/9df06fdb nreloc=414/413", "符号表差异: 仅A=[('$C$L16', 278, 2, 4, 6), ('$C$L17', 282, 2, 4, 6), ('$C$L18', 301, 2, 4, 6), ('$C$L19', 
- `main` DIFFERS: {"msgs": ["段 .text: size=97/95 data=d6600c05/b4244b47 reloc=85e74503/6a76b49c nreloc=27/27", "段 .text:retain: size=137/137 data=b3bf235b/b3bf235b reloc=f4da013e/cbbb282b nreloc=36/36", "符号表差异: 仅A=[('$
- `MotorCarrier` DIFFERS: {"msgs": ["段 .text: size=925/932 data=7b2c099f/e008ead2 reloc=1414c361/5bbef5b0 nreloc=316/318", "符号表差异: 仅A=[('$C$L3', 71, 2, 4, 6), ('$C$L4', 73, 2, 4, 6), ('$C$L5', 82, 2, 4, 6), ('$C$L6', 90, 2, 4,
- `MotorCurrentTransform` DIFFERS: {"msgs": ["段 .text: size=219/242 data=26b81157/e6dcd50e reloc=540a8f65/d08e78d9 nreloc=45/45", "符号表差异: 仅A=[('_ChangeCurrent', 113, 2, 4, 2), ('_AlphBetaToDQ', 30, 2, 32896, 2), ('_DQToAmpTheta', 89, 2
- `MotorDataExchange` DIFFERS: {"msgs": ["段 .text: size=1259/1218 data=e0ceba39/b56accf9 reloc=77b40ad0/fc799b80 nreloc=836/657", "符号表差异: 仅A=[('_ParSendTune', 1138, 2, 4, 2), ('_ParGet2Ms', 447, 2, 4, 2)] 仅B=[('_ParSendTune', 1097,
- `MotorDebug` DIFFERS: {"msgs": ["段 .text: size=271/224 data=9e642f7b/092f5b2f reloc=035d487c/667f3c5a nreloc=88/67", "段 .cinit: size=4/4 data=3d73c71a/3d73c71a reloc=f1a1fe84/2df54ff0 nreloc=1/1", "符号表差异: 仅A=[('$C$L1', 22,
- `MotorEncoder` DIFFERS: {"msgs": ["段 .text: size=1511/1421 data=c582697c/649f09cd reloc=9f5c8690/a1b6ae21 nreloc=457/433", "符号表差异: 仅A=[('$C$L1', 164, 2, 4, 6), ('$C$L2', 199, 2, 4, 6), ('$C$L3', 201, 2, 4, 6), ('$C$L4', 213,
- `MotorImParEst` DIFFERS: {"msgs": ["段 .text: size=1942/1920 data=21a24912/88b260b5 reloc=773dd1d8/d19ea637 nreloc=647/621", "段 .text:retain: size=138/138 data=4b9a1290/149dd6a7 reloc=7b9410c0/c778704d nreloc=38/38", "符号表差异: 仅
- `MotorInfoCollect` DIFFERS: {"msgs": ["段 .text: size=297/298 data=318c9918/c190c2d8 reloc=fecda8b9/92ac9e98 nreloc=100/101", "符号表差异: 仅A=[('$C$L2', 19, 2, 4, 6), ('$C$L3', 27, 2, 4, 6), ('$C$L4', 49, 2, 4, 6), ('$C$L5', 54, 2, 4,
- `MotorInvProtect` DIFFERS: {"msgs": ["段 .text: size=1554/1693 data=b749369e/4c3285f1 reloc=54937d13/296eb1f1 nreloc=478/264", "符号表差异: 仅A=[('$C$L1', 36, 2, 4, 6), ('$C$L2', 42, 2, 4, 6), ('$C$L3', 66, 2, 4, 6), ('$C$L4', 105, 2,
- `MotorMain` DIFFERS: {"msgs": ["段 .text: size=1532/1525 data=fea26d9c/82b98afb reloc=76564d6a/6b7eaeba nreloc=653/655", "符号表差异: 仅A=[('$C$L1', 113, 2, 4, 6), ('$C$L2', 121, 2, 4, 6), ('$C$L3', 123, 2, 4, 6), ('$C$L4', 135,
- `MotorParChange` DIFFERS: {"msgs": ["段数 4 vs 7", "段 .text: size=1471/1378 data=297c4b31/876b3d55 reloc=0d897674/64d610ed nreloc=571/515", "符号表差异: 仅A=[('$C$L1', 18, 2, 4, 6), ('$C$L2', 32, 2, 4, 6), ('$C$L3', 48, 2, 4, 6), ('$C
- `MotorParEst` DIFFERS: {"msgs": ["段 .text: size=359/369 data=d8ded7ae/11032723 reloc=42de9fbf/1e475f65 nreloc=154/154", "符号表差异: 仅A=[('$C$L1', 8, 2, 4, 6), ('$C$L3', 27, 2, 4, 6), ('$C$L4', 31, 2, 4, 6), ('$C$L5', 35, 2, 4, 
- `MotorPmsmMain` DIFFERS: {"msgs": ["段 .text: size=995/1022 data=6aaac148/024d8274 reloc=a89798dd/4fb6f478 nreloc=344/349", "段 .text:retain: size=170/115 data=e3829c0f/bdc96e38 reloc=e54c19d8/2fa9c483 nreloc=59/42", "符号表差异: 仅A
- `MotorPmsmParEst` DIFFERS: {"msgs": ["段 .text: size=2444/2340 data=032b9d3c/c02d73c2 reloc=84fa336b/10596368 nreloc=988/954", "符号表差异: 仅A=[('_m_Pos$1', 0, 6, 32896, 6), ('_m_Cnt$2', 1, 6, 32896, 6), ('_m_Pos$3', 2, 6, 32896, 6),
- `MotorPublicCal` DIFFERS: {"msgs": ["段 .text: size=751/745 data=2089ab71/cf18eb2b reloc=ac166782/a7f6d1c4 nreloc=314/314", "符号表差异: 仅A=[('$C$L1', 2, 2, 4, 6), ('$C$L2', 65, 2, 4, 6), ('$C$L3', 72, 2, 4, 6), ('$C$L4', 89, 2, 4, 
- `MotorPWM` DIFFERS: {"msgs": ["段 .text: size=596/584 data=3652cfdd/49ba8f90 reloc=e2b6ddb5/5744c1ae nreloc=214/134", "段 .cinit:_iTrig1_Cpwm: size=15/15 data=b50965f7/b50965f7 reloc=8a133d05/f1ae186d nreloc=1/1", "段 .cini
- `MotorSpeedCheck` DIFFERS: {"msgs": ["段 .text: size=743/726 data=c2b22a79/12afb7fe reloc=1764f6b1/c5534425 nreloc=348/345", "符号表差异: 仅A=[('$C$L3', 85, 2, 4, 6), ('$C$L4', 86, 2, 4, 6), ('$C$L5', 88, 2, 4, 6), ('$C$L6', 95, 2, 4,
- `MotorSVC` DIFFERS: {"msgs": ["段 .text: size=909/857 data=b4805f0d/26c6d481 reloc=4a3a1e8f/64a2b82b nreloc=309/282", "符号表差异: 仅A=[('$C$L1', 95, 2, 4, 6), ('$C$L2', 110, 2, 4, 6), ('$C$L3', 215, 2, 4, 6), ('$C$L4', 415, 2,
- `MotorVCMain` DIFFERS: {"msgs": ["段 .text: size=2279/2198 data=3e530858/95660a0d reloc=13109a12/706c3058 nreloc=882/796", "符号表差异: 仅A=[('$C$L1', 8, 2, 4, 6), ('$C$L2', 59, 2, 4, 6), ('$C$L3', 77, 2, 4, 6), ('$C$L4', 151, 2, 
- `MotorVF` DIFFERS: {"msgs": ["段 .text: size=2505/2478 data=53b5cfc7/976467eb reloc=b2969b2d/f5ace04b nreloc=847/811", "符号表差异: 仅A=[('$C$L1', 15, 2, 4, 6), ('$C$L2', 19, 2, 4, 6), ('$C$L3', 131, 2, 4, 6), ('$C$L4', 134, 2
- `ZeroPosCtl` DIFFERS: {"msgs": ["段 .text: size=68/76 data=1bf60fdf/679aecf6 reloc=df482e6c/a279116a nreloc=30/25", "符号表差异: 仅A=[('$C$L1', 13, 2, 4, 6), ('$C$L2', 22, 2, 4, 6), ('$C$L3', 33, 2, 4, 6), ('$C$L4', 65, 2, 4, 6)]
- `SubPrg` DIFFERS: {"msgs": ["段 .text: size=571/567 data=c17b283d/437e3543 reloc=d527b9c9/f58241ba nreloc=9/9", "符号表差异: 仅A=[('$C$L5', 132, 2, 4, 6), ('$C$L6', 147, 2, 4, 6), ('$C$L7', 243, 2, 4, 6), ('$C$L8', 259, 2, 4,
- `mainInit_2803x` DIFFERS: {"msgs": ["段 .text: size=402/406 data=a0cac8c8/762a24ba reloc=66b10456/3a1f38f8 nreloc=117/120", "段 ramfuncs: size=23/23 data=1a019628/5b0c82a8 reloc=bd13ff53/7f43dd7a nreloc=8/8", "符号表差异: 仅A=[('$C$L5

## 4. 层级 C — 整机映像逐字节比对

| 配置 | 原工程映像 | 新工程映像 | 大小 | SHA256 | 结论 |
|---|---|---|---:|---|---|
| Debug | `md380_C.hex` | `jt380_C.hex` | 287354 / 284250 | `d8afb2efc5cd4894` | 不同 (因改写, 见下) |
| Release | `MD380_Release.hex` | `jt380_Release.hex` | 287087 / 283516 | `1156cbe77441c51a` | 不同 (因改写, 见下) |

> 当前含 51 个改写模块 (`src/motor/ZeroPosCtl.c`, `src/function/f_p2p.c`, `src/function/f_common.c`, `src/function/f_motorFc.c`, `src/function/f_invPara.c`, `src/function/f_plc.c`, `src/function/f_modbus.c`, `src/function/f_torqueCtrl.c`, `src/function/f_vf.c`, `src/function/f_frqSrc.c`, `src/function/f_runSrc.c`, `src/function/f_frqSrc_pid.c`, `src/function/f_canlink.c`, `src/function/f_profibus.c`, `src/motor/MotorDataExchange.c`, `src/function/f_fcDeal.c`, `src/function/f_error.c`, `src/function/f_menu.c`, `src/system/DSP2803x_CpuTimers.c`, `src/motor/MotorInfoCollect.c`, `src/motor/MotorParEst.c`, `src/function/f_dspcan.c`, `src/motor/MotorPublicCal.c`, `src/motor/MotorSpeedCheck.c`, `src/motor/MotorCarrier.c`, `src/function/f_ui.c`, `src/function/f_main.c`, `src/function/f_interface.c`, `src/function/f_eeprom.c`, `src/motor/MotorEncoder.c`, `src/motor/MotorCurrentTransform.c`, `src/motor/MotorDebug.c`, `src/subprogram/SubPrg.c`, `src/motor/MotorInvProtect.c`, `src/motor/MotorMain.c`, `src/system/main.c`, `src/system/mainInit_2803x.c`, `src/motor/MotorSVC.c`, `src/motor/MotorPWM.c`, `src/function/f_debug.c`, `src/function/f_comm_DP.c`, `src/motor/MotorPmsmMain.c`, `src/motor/MotorPmsmParEst.c`, `src/motor/MotorParChange.c`, `src/function/f_runSrc_accDecFrq.c`, `src/function/f_osc.c`, `src/motor/MotorImParEst.c`, `src/motor/MotorVCMain.c`, `src/motor/MotorVF.c`, `src/function/f_comm.c`, `src/function/f_io.c`)。

> **整机映像与原工程不同, 但这不是"改写"的必然结果**: 其中 2 个模块
> (`f_vf`, `f_interface`) 改写后编译产物**逐字节相同**, 对映像**零贡献**。
> 差异只来自另外 49 个确实改变了指令序列的模块。也就是说"改写过 ⇒ 产物必然
> 不同"并不成立 —— 最好的改写本就该收敛到同一份机器码。

> 差异必须是**有界的**: 由 `tests/test_build_evidence.py` 卡住 (≤256 字节)。
> 改写模块的行为等价证据见第 5 节。
> 另外, 原工程 Release 的产物名沿用其自身配置 (`MD380_Release.out`), 新工程统一为
> `jt380_Release.out`。

入口点核对 (`build/Debug/jt380_C.map`):

```
ENTRY POINT SYMBOL: "code_start"  address: 003f7ff6
  BEGIN                 003f7ff6   00000002  ...  codestart
```

与原工程一致: `code_start` @ `0x003F7FF6`, 段 `codestart` 归入 `BEGIN`。

## 5. 改写模块的行为等价证据 (差分测试 + 目标端)

改写过的实现**不受**"逐字节相同"契约约束, 所以层级 A/B/C 不足以证明它。
替代证据是 **行为等价**: 同一组输入喂给基准实现与改写实现, 逐字段比对结果。

**方法**:

1. 冻结基准 `tools/difftest/cases/<case>/ref/<Mod>_ref.c` (即改写前那份"与原工程字节等价"的版本)
2. 两侧各自编译成宿主可执行文件: 模块导出的符号加 `ref_` / `new_` 前缀,
   硬件寄存器 mock **共享同一份**, 保证两侧看到相同的外设状态
3. 输入空间 = 边界值全交叉枚举 + 可复现的随机采样
4. 每次调用前后比对: 返回值、模块导出的全局状态**全字段** (`memcmp`)、硬件副作用;
   比对前把目标结构体填成哨兵值, 这样"少写一个字段"也会被抓出来

**为什么必须用精确宽度类型**: C28x 上 `int` 是 16 位, 宿主上是 32 位。
沿用原 `DataTypeDef.h` 会让 `s16`/`u16` 静默变宽, 溢出与截断位置全错。
`tools/difftest/shim/DataTypeDef.h` 把四个类型钉到 `int16_t`/`uint16_t`/`int32_t`/`uint32_t`。

**负向对照** (`tools/difftest/negative_control.py`): 往基准注入真实 bug
(上限差一、移位量错、写错字段、操作数互换、赋值漏改、判据取反、常量写错、
整机拷贝长度取错 …), 要求**全部被抓到** —— 抓不到就说明测试台对该类错误
不敏感, 此时 "0 处不一致" 没有意义。

变体**没编译起来**单独记为 `BROKEN` 并判总账失败: 否则 "全部抓到" 可能只是
"全都没跑起来" —— 这两件事必须分得开。

| 用例 | 注入的 bug | 抓到 | 结果 |
|---|---:|---:|---|
| `zeroPosCtl` | 7 | **7** | **全部抓到** |
| `f_p2p` | 7 | **7** | **全部抓到** |
| `f_common` | 12 | **12** | **全部抓到** |
| `f_motorFc` | 14 | **14** | **全部抓到** |
| `f_invPara` | 22 | **22** | **全部抓到** |
| `f_plc` | 38 | **38** | **全部抓到** |
| `f_modbus` | 47 | **47** | **全部抓到** |
| `f_torqueCtrl` | 31 | **31** | **全部抓到** |
| `f_vf` | 38 | **38** | **全部抓到** |
| `f_frqSrc` | 129 | **129** | **全部抓到** |
| `f_runSrc` | 161 | **161** | **全部抓到** |
| `f_frqSrc_pid` | 176 | **176** | **全部抓到** |
| `f_canlink` | 138 | **138** | **全部抓到** |
| `f_profibus` | 106 | **106** | **全部抓到** |
| `MotorDataExchange` | 107 | **107** | **全部抓到** |
| `f_fcDeal` | 125 | **125** | **全部抓到** |
| `f_error` | 204 | **204** | **全部抓到** |
| `f_menu` | 447 | **447** | **全部抓到** |
| `DSP2803x_CpuTimers` | 24 | **24** | **全部抓到** |
| `MotorInfoCollect` | 44 | **44** | **全部抓到** |
| `MotorParEst` | 64 | **64** | **全部抓到** |
| `f_dspcan` | 80 | **80** | **全部抓到** |
| `MotorPublicCal` | 98 | **98** | **全部抓到** |
| `MotorSpeedCheck` | 131 | **131** | **全部抓到** |
| `MotorCarrier` | 106 | **106** | **全部抓到** |
| `f_ui` | 52 | **52** | **全部抓到** |
| `f_main` | 155 | **155** | **全部抓到** |
| `f_interface` | 208 | **208** | **全部抓到** |
| `f_eeprom` | 175 | **175** | **全部抓到** |
| `MotorEncoder` | 117 | **117** | **全部抓到** |
| `MotorCurrentTransform` | 23 | **23** | **全部抓到** |
| `MotorDebug` | 34 | **34** | **全部抓到** |
| `SubPrg` | 54 | **54** | **全部抓到** |
| `MotorInvProtect` | 157 | **157** | **全部抓到** |
| `MotorMain` | 256 | **256** | **全部抓到** |
| `main` | 38 | **38** | **全部抓到** |
| `mainInit_2803x` | 69 | **69** | **全部抓到** |
| `MotorSVC` | 171 | **171** | **全部抓到** |
| `MotorPWM` | 174 | **174** | **全部抓到** |
| `f_debug` | 34 | **34** | **全部抓到** |
| `f_comm_DP` | 106 | **106** | **全部抓到** |
| `MotorPmsmMain` | 175 | **175** | **全部抓到** |
| `MotorPmsmParEst` | 219 | **219** | **全部抓到** |
| `MotorParChange` | 194 | **194** | **全部抓到** |
| `f_runSrc_accDecFrq` | 198 | **198** | **全部抓到** |
| `f_osc` | 201 | **201** | **全部抓到** |
| `MotorImParEst` | 356 | **356** | **全部抓到** |
| `MotorVCMain` | 304 | **304** | **全部抓到** |
| `MotorVF` | 179 | **179** | **全部抓到** |
| `f_comm` | 128 | **128** | **全部抓到** |
| **合计** | **6133** | **6133** | **全部抓到** |

### `src/motor/ZeroPosCtl.c`

- 变换: 分支极性反转、提取局部量、Min 宏改为显式边界比较、硬件读序保持不变

- 必须守住的不变量:

  - 函数名 GetPGPos / ZeroPosJudge / ZeroPosControl 与全局 gZeroPos 不变
  - 硬件读序保持 QEP 先、TIM 后
  - PosInit 先于 Pos 使用、后于 Pos 更新
  - 所有表达式保持原有类型宽度 (u16/s16/s32/u32)

| 项 | 结果 |
|---|---|
| 差分用例数 | **243,472** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CpuTimer1`, `_EQepRegs`, `_GetPGPos`, `_ZeroPosControl`, `_ZeroPosJudge`, `_gMainCmd`, `_gZeroPos` |
| `.text` 尺寸 | 68 → 76 (**+8**) |

### `src/function/f_p2p.c`

- 变换: if/else 分支互换、switch 展开为 if/else 链、提取同类型局部量、&& 两侧互换、改为提前返回

- 必须守住的不变量:

  - 函数名 P2PDataDeal 与全局 p2pData 不变
  - 所有算术表达式保持原有类型宽度 (Uint16/int16/int32), 不加任何强制转换
  - switch 各 case 互不贯穿, 展开为 if/else 链等价
  - 错误标志的写入条件与顺序不变

| 项 | 结果 |
|---|---|
| 差分用例数 | **2,541,160** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_P2PDataDeal`, `_errorInfo`, `_errorOther`, `_frqAimTmp`, `_frqFdb`, `_frqRun`, `_funcCode`, `_itDisp`, `_maxFrq`, `_p2pData` |
| `.text` 尺寸 | 154 → 159 (**+5**) |

### `src/function/f_common.c`

- 变换: 循环由 while(length--) 改为 length==0 提前返回 + do/while(--length)、参数改为 const 指针局部量 pData; 两个位反转辅助函数改为调换各步先后顺序

- 必须守住的不变量:

  - 函数名 CrcValueByteCalc / Reverse4Bit / Reverse8Bit 与常量表 crc16Table 不变
  - 所有表达式保持原有类型宽度 (Uint16), 未引入窄化中间量
  - 表索引仍是 (crcValue ^ *data) & 0x00FF, 掩码未放宽、高字节仍不影响结果
  - CRC 初值仍为 0xFFFF, 空缓冲区仍返回 0xFFFF
  - 未编译的 NEW_CRC==0/2 分支与 #if 0 块保持原样

| 项 | 结果 |
|---|---|
| 差分用例数 | **17,039,736** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CrcValueByteCalc`, `_Reverse4Bit`, `_Reverse8Bit`, `_crc16Table` |
| `.text` 尺寸 | 77 → 76 (**-1**) |

### `src/function/f_motorFc.c`

- 变换: motorSn 的 if/else 改为三目表达式、M1 分支四条互不相干赋值重排、else 分支把索引表项提为 Uint16 局部量 nInc、编码器强制模式改为三目、极对数算式的首项提为 Uint32 局部量 frqDiv

- 必须守住的不变量:

  - 函数名 UpdateMotorPara 与文件作用域 polePair / motorSnIndexNIncToMotor2 不变
  - motorSn 的来源与门控不变: diSelectFunc.f2.bit.motorSnDi 门控、取值来自 diFunc.f2.bit.motorSnDi
  - 所有算术表达式保持原有类型宽度 (Uint16 / Uint32), 未引入窄化中间量; polePair 仍只截断一次
  - nInc 的类型就是索引表元素类型 Uint16, 加法仍在原类型下进行
  - motorFc 与 funcCode 的读写集合不变 (目标端外部符号集合逐一相同)
  - 未编译的 #elif 1 备用定义与 #if 1 块保持原样

| 项 | 结果 |
|---|---|
| 差分用例数 | **260,000** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_UpdateMotorPara`, `_decNumber`, `_diFunc`, `_diSelectFunc`, `_errorCode`, `_funcCode`, `_motorFc`, `_motorSn`, `_motorSnIndexNIncToMotor2`, `_polePair` |
| `.text` 尺寸 | 125 → 127 (**+2**) |

### `src/function/f_invPara.c`

- 变换: 四路电机的「同一值写四处 + 存四处」扇出改为 inc[4] 偏移表 + 嵌套循环; 四段逐字重复的额定参数赋值块提取为 SET_MOTOR_RATING_PARA 宏; 加减速时间分档改三目; 转矩提升 if/else 阶梯改三目链; 参数校验 if 改三目; UpdateInvType 把 volLevel 提为局部量 level; 电机选择 if/else 链改 switch; 数据点格式的两个布尔判据提为局部量; 定点搜索的 for+break 改 while; 索引算式提出 scaled 局部量; SaveTuneData 把 sizeof 改为可移植的元素个数写法并把 addBase 提到循环外

- 必须守住的不变量:

  - 函数名 InverterTypeRelatedParaDeal / InverterTypeRelatedNoMotorParaDeal / InverterTypeRelatedNoMotorParaSaveDeal / MotorPowerRelatedParaDeal / TorqueBoostDeal / ValidateInvType / UpdateInvType / SaveTuneData 与全局 invPara 不变
  - 全部参数表数据 (invTypeParaTableT380/T220/S220/T480/T690/T1140、invTypeLimitTable、INV_TYPE_POINT_LIMIT_TABLE、INV_TYPE_BIG_ACC_DEC_START_TABLE、ovVoltageInitValue、motor1ParaIndex、motorSnIndexIncToMotor1、pInvTypeParaTable) 逐字节不变
  - 所有算术表达式保持原有类型宽度: 未把裸 int 提升改写成 int16/int32, 尤其 MotorPowerRelatedParaDeal 里 (power - lowerPower) 与 (upperPower - power) 仍是 int 提升后的比较, 没有加 Uint16 强制转换 (目标端 int 是 16 位, 加转换会改变溢出行为)
  - SaveOneFuncCode 的调用顺序逐条不变 (它按序追加进 funcCodeOneWriteIndex 日志, 顺序本身就是被观测的输出)
  - 加减速时间 8 个索引、存盘 8 个索引的先后次序不变; 这些索引互不重合, 所以扇出循环内的写/存交错不影响终态
  - 未新增任何目标文件符号: TorqueBoostDeal 一开始写成函数内 static const 表, 目标端会多出 _TORQUE_BOOST_LIMIT$2/_TORQUE_BOOST_VALUE$3 两个符号, 已改回三目链 (目标端外部符号集合因此逐一相同)
  - motorSn / motor1ParaIndex / motorSnIndexIncToMotor1 只读不写, 由 driver.c 在每次调用前后断言
  - 未编译的 #if 0 数据块与 #if F_DEBUG_RAM 备用分支保持原样

| 项 | 结果 |
|---|---|
| 差分用例数 | **68,300** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_INV_TYPE_BIG_ACC_DEC_START_TABLE`, `_INV_TYPE_POINT_LIMIT_TABLE`, `_InverterTypeRelatedNoMotorParaDeal`, `_InverterTypeRelatedNoMotorParaSaveDeal`, `_InverterTypeRelatedParaDeal`, `_MotorPowerRelatedParaDeal`, `_SaveTuneData`, `_TorqueBoostDeal`, `_UpdateInvType`, `_ValidateInvType`, `_decNumber`, `_funcCode`, `_funcCodeOneWriteIndex`, `_funcCodeOneWriteNum`, `_gParaIdToFunctionDataBuff`, `_invPara`, `_invTypeLimitTable`, `_invTypeParaTableS220`, `_invTypeParaTableT1140`, `_invTypeParaTableT220`, `_invTypeParaTableT380`, `_invTypeParaTableT480`, `_invTypeParaTableT690`, `_motor1ParaIndex`, `_motorSn`, `_motorSnIndexIncToMotor1`, `_ovVoltageInitValue`, `_pInvTypeParaTable` |
| `.text` 尺寸 | 888 → 889 (**+1**) |

### `src/function/f_plc.c`

- 变换: InvTranTabCfg / InvRecTabCfg 把 datah/datal 提为局部量 addr/data、慢表下标改为就地 addr -= INV_SLOW_TAB_ADDR 复用、InvRecTabCfg 的 else{ if } 折成 else if; FuncCodeRwAtrrib 的「可接受高位半字节」空 if 体改为 switch(highH)、组号调整改 switch、禁用索引判定提为局部量 blocked 并拆成「所有模式共同」+「按 rwMode 追加」两段; PlcDataFramDeal 用 isFast 标志替代两处 InvTrTab == InvTrFastTab 指针比较、旋转索引的更新改为 pIndex 指针 + idxLimit

- 必须守住的不变量:

  - 函数名 InvTranTabCfg / InvRecTabCfg / FuncCodeRwAtrrib / PlcDataFramDeal 与全局 InvTrFastIndex / InvTrSlowIndex / InvTrFastTab / InvTrSlowTab / InvRecFastTab / InvRecSlowTab 不变
  - 四张表、两个旋转索引的读写集合与次序不变 (目标端外部符号集合逐一相同)
  - InvTranTabCfg 失败时仍返回 COMM_ERR_PARA (不是 err), InvRecTabCfg 失败时仍返回 err; 两者的初始 err 仍是 COMM_ERR_ADDR / COMM_ERR_PARA
  - addr == 0 仍直接返回 COMM_ERR_NONE; 0x7000 段仍是「读模式一律接受、写模式仅 0x7300 窗口接受」, switch 的 default 与 0xF000 分支语义与原来的空 if/else 等价
  - 禁用索引判定仍是同一组比较, 且都是无副作用的纯比较, 所以拆成两段不改变求值结果; GetGradeIndex(group, grade) 仍只求值一次
  - 所有算术表达式保持原有类型宽度: addr -= INV_SLOW_TAB_ADDR 仍是 Uint16 就地运算, dataPi->mdl.data.datal 的三目仍以 int 求值后截断, 旋转索引比较仍是 int 提升后比较, 未加任何会改变溢出行为的强制转换
  - CanControlWriter / CanControlRead 的调用次数、实参与嵌套层次不变 (深度链 [+1]/[+2]/[+3] 仍逐层短路), 调用日志逐条比对
  - CanlinkDataTran 的 len / timeout 与帧内容不变; 帧内 msgid.bit.destSta/srcSta 的写入次序不变
  - 未新增任何目标文件符号 (只用局部量与表达式, 未引入函数内 static 或新函数)
  - 未编译的 #else 死分支 (DEBUG_F_PLC == 0 的三个空实现) 保持原样

| 项 | 结果 |
|---|---|
| 差分用例数 | **325,966** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CanControlRead`, `_CanControlWriter`, `_CanlinkDataTran`, `_FuncCodeRwAtrrib`, `_InvRecFastTab`, `_InvRecSlowTab`, `_InvRecTabCfg`, `_InvTrFastIndex`, `_InvTrFastTab`, `_InvTrSlowIndex`, `_InvTrSlowTab`, `_InvTranTabCfg`, `_PlcDataFramDeal`, `_UpdataFuncCodeGrade`, `_funcCode`, `_funcCodeGradeSum` |
| `.text` 尺寸 | 510 → 533 (**+23**) |

### `src/function/f_modbus.c`

- 变换: ModbusRcvDataDeal 把三组 (高字节 << 8) + 低字节 里反复出现的两个帧字提为 highByte/lowByte 局部量复用; ModbusStartDeal 把「帧间隔未到」提前返回、并把两支逐字相同的 else if (CAN_OPEN && 地址1 / 地址 == commSlaveAddress) 合并成一条 || 判据; UpdateModbusCommFormat 把整除结果提为 frameSpaceTime 局部量、并重排三次字段写入; ModbusSendDataDeal 用提前返回替代 if/else 链、把 pwdPass/write 两支共同的地址回显提到合并分支之前、协议判据反转并把恒真的 else if (commProtocol == 0) 写成 else; ModbusCommErrCheck 改为提前返回并把阈值提为 overTimeLimit 局部量

- 必须守住的不变量:

  - 函数名 ModbusRcvDataDeal / ModbusStartDeal / UpdateModbusCommFormat / ModbusSendDataDeal / ModbusCommErrCheck 与文件内 8 个帧字宏 (RTUslaveAddress / RTUcmd / RTUhighAddr / RTUlowAddr / RTUhighData / RTUlowData / RTUlowCrc / RTUhighCrc) 不变
  - ModbusStartDeal 的「帧间隔未到」仍只返回 1 且不写任何状态; 帧间隔到达后 rcvFrame[0] 仍被 tmp 覆盖
  - ModbusStartDeal 的 else 分支仍只写 rcvFlag、不写 rcvNum (rcvNum 只在三个命中分支里被置 1) —— 提前返回与条件合并都没有把 rcvNum = 1 提出来, 这是最容易改错的一处
  - 两个原为独立 else if 的命中分支合并成 || 后, 判据求值顺序与短路行为不变, 且两支的函数体本来就逐字相同
  - ModbusSendDataDeal 的优先级链不变: err > sciFlag.bit.pwdPass > sciFlag.bit.write > sciFlag.bit.read; 合并 pwdPass/write 时共同的两条地址回显先写、后两条按 pwdPass 再分, 未增加也未减少任何 sendFrame 写入
  - 读分支的两个协议布局不变: commProtocol 非 0 时 sendFrame[2] 写低 8 位、长度 +5、数据起点 3; 为 0 时高/低字节分写 [2]/[3]、长度 +6、数据起点 4。把 else if (commProtocol == 0) 写成 else 等价 —— Uint16 的「非 0」与「== 0」互补
  - 所有算术表达式保持原有类型宽度: (hi << 8) + lo 仍是 Uint16 提升后的 int 运算再截断 (未加任何 Uint16 强制转换), commData << 1 仍是 int 提升, (Uint32)commOverTime * 2 * TIME_UNIT_sciCommOverTime 仍是 Uint32 运算
  - ModbusCommErrCheck 仍先判使能再比阈值; 提前返回后 commTicker 只在超时分支被钳位, 且钳位值与判据用的是同一个表达式
  - UpdateModbusCommFormat 三次字段写入的次序改变不影响终态 (三个字段互不重合); 385 * 2 / baudRate + 1 的整数除法结果与原来逐值相同 (baudRate 为 0 是原工程就有的除零 UB, 未新增也未修复)
  - 未新增任何目标文件符号 (只用局部量与表达式, 未引入函数内 static 或新函数)
  - 未引入任何新的硬件访问: 本模块只读写 commRcvData / commSendData / commTicker / commProtocol / sciFlag / funcCode / sendFrame / rcvFrame / commReadData 这九个对象

| 项 | 结果 |
|---|---|
| 差分用例数 | **414,979** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ModbusCommErrCheck`, `_ModbusRcvDataDeal`, `_ModbusSendDataDeal`, `_ModbusStartDeal`, `_UpdateModbusCommFormat`, `_commProtocol`, `_commRcvData`, `_commReadData`, `_commSendData`, `_commTicker`, `_funcCode`, `_rcvFrame`, `_sciFlag`, `_sendFrame` |
| `.text` 尺寸 | 291 → 283 (**-8**) |

### `src/function/f_torqueCtrl.c`

- 变换: 8 个魔数提为文件作用域宏 (TORQUE_PU_SHIFT / TORQUE_PU_BASE / RATING_TORQUE / TORQUE_LIMIT_CUR_PERCENT / TORQUE_LIMIT_CUR_PERCENT_BASE / TORQUE_LIMIT_SPD_ERR / TORQUE_LIMIT_TICKS / TORQUE_RAMP_STEP): 其中 RATING_TORQUE 原本在函数体中间定义、TORQUE_LIMIT_TIME / DO_CALC_PERIOD 的除法在 6 处逐字重复; 局部量改名 (tmp→torqueSet, tmp1→torqueAi2, torquePu→torqueFullScale, accDecTime→rampTicks, ret→limitActive); UpdateTorqueAim 的源选择 if/else 两支互换 (转矩环分支提前, 读起来是「转矩环用转矩环的源」); AI1/AI2/AI3 的「先算下标 tmp 再取值」两行合并为一行内联下标; TorqueLimitCalc 的电流门限算式提为 curLimit 局部量 (原来整个算式写在 if 条件里); 删掉 && 判据两侧的多余空行与一层多余括号

- 必须守住的不变量:

  - 函数名 TorqueCalc / UpdateTorqueAim / AccDecTorqueCalc / TorqueLimitCalc 与全局 upperTorque / torqueAim / torqurLine / torqueSetEthDp / torqueUpperEthDp 不变
  - 所有算术表达式保持原有类型宽度: ((int32)aiDeal[...].set * torqueFullScale) >> TORQUE_PU_SHIFT 仍是 int32 乘法后算术右移; rampTicks * TORQUE_RAMP_STEP 仍是 int32 乘 (Uint16)(10/2), 强制转换留在宏定义里未被删掉; itDisp > curLimit 仍是 Uint16 提升后与 int32 的有符号比较; 未新增或删除任何强制转换
  - UpdateTorqueAim 源选择两支互换: 两支互斥且写的是同一对变量 (torqueSrc / torqueFullScale), 互换不改变取值。判据仍是 RUN_MODE_TORQUE_CTRL == runMode —— 没有改写成 RUN_MODE_SPEED_CTRL == runMode, 后者只在 runMode ∈ {0,1} 时等价, runMode > 1 时不等价
  - AccDecTorqueCalc 的三处提前返回条件、次序与副作用不变: 非转矩环 → upperTorque = torqueAim; 未运行 → upperTorque = 0; 已到目标 → 直接返回 (不碰 torqurLine)
  - torqurLine 的字段写入次序不变 (aimValue → tickerAll → maxValue → curValue → calc → 回读 curValue); 六个字段里只有 remainder 是跨调用保留的状态, 其余五个每次都被覆盖
  - TorqueLimitCalc 的两个计数器仍是函数内 static, 名字与语义不变; 增量判据 >= TORQUE_LIMIT_TICKS、钳位值 TORQUE_LIMIT_TICKS + 1、复位值 0、以及最终的 && 判据都与原来逐字对应 (TORQUE_LIMIT_TICKS 就是原来的 TORQUE_LIMIT_TIME / DO_CALC_PERIOD)
  - switch 的 default: 分支仍不初始化 torqueSet, 保持原有的「torqueSrc 越界时 torqueAim 读到不确定值」UB —— 改写**不修复**它, 修了就不再等价。差分测试把 torqueSrc 限制在可达的 0..7
  - 未新增任何目标文件符号 (只用文件作用域宏、局部量与表达式; 未引入新函数或新的函数内 static) —— 目标端外部符号集合逐一相同
  - 未编译的 #if 0 死块与 #elif 1 备用定义逐字节保持原样 (由 install 脚本断言)
  - 目标端 .text 尺寸不变 (290 → 290 字), 两侧告警数均为 0

| 项 | 结果 |
|---|---|
| 差分用例数 | **76,663** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AccDecTorqueCalc`, `_CanRxTxCon`, `_LineChangeCalc`, `_TorqueCalc`, `_TorqueLimitCalc`, `_UpdateTorqueAim`, `_aiDeal`, `_frq`, `_frqRun`, `_funcCode`, `_itDisp`, `_motorFc`, `_p2pData`, `_pulseInSet`, `_runFlag`, `_runMode`, `_torqueAim`, `_torqueSetEthDp`, `_torqueUpperEthDp`, `_torqurLine`, `_upperTorque` |
| `.text` 尺寸 | 290 → 290 (**+0**) |

### `src/function/f_vf.c`

- 变换: 4 个魔数提为文件作用域宏 (VF_AI_SCALE_SHIFT / VF_COMM_SCALE_BASE / VF_HALF_SEPARATE_NUM / VF_ACC_TICKS): 其中 >> 15 在 AI 与脉冲两支里逐字重复、/ 10000 与频率加权系数 2 各只有一个出处、TIME_UNIT_VF_VOL_ACC_TIME / VF_CALC_PERIOD 的除法原来直接嵌在赋值行里; 局部量 tmp 改名为 srcValue (它装的是某个来源的原始设定值); 门控条件从「常量在左、比较方向混排、缩进混用 4 空格 / 7 空格 / 制表符+3 空格」规范为「变量在左、逐条加括号、缩进对齐」; 删掉紧贴函数定义的重复前置声明 void vfSeparateDeal(void); 与声明后从未使用的 extern Uint16 outVoltageDisp;; 删掉半分离分支里的一行多余空行; 两条过长的定标赋值按运算符换行

- 必须守住的不变量:

  - 函数名 vfSeparateDeal 与全局 vfSeprateVolAim / vfSeparateVol / vfSeprateVolAccLine 不变 (vfSeparateVol 被 f_interface.c 引用, vfSeparateDeal 被 f_main.c 调用)
  - 所有算术表达式保持原有类型宽度与运算次序: (int32)srcValue * 功能码侧额定电压 >> VF_AI_SCALE_SHIFT 仍是 int32 乘法后**算术右移** (对负数是向下取整, 与除法不同, 故不能改成 /32768); 多段速 / PLC / PID 仍是 (int32)X * 额定电压 / maxFrq 的 int32 除法; 通讯支仍是先 (int16) 再 (int32) 的符号扩展后 / VF_COMM_SCALE_BASE; 频率加权仍是 Uint32 运算 ((Uint32)2 * ABS_INT32(frq) * 目标值) / 额定频率; 未新增或删除任何强制转换
  - 门控条件改写只是把比较方向从「常量 != 变量」换成「变量 != 常量」并给每条判据补上括号: (A && B) || C 的形状未变, 没有把 && 提到 || 外面。!= 的两个操作数互换在 C 里恒等
  - 取值来源 switch 的 9 个 case 与 default 的**语义**逐一保留: default: 仍然只是 break, 即 vfSeprateVolAim 保留上一次调用留下的值。这是**有定义的行为** (读的是文件作用域全局量), 不是 UB, 差分测试的 phase_default_carry 专门覆盖它 —— 与 f_torqueCtrl 那个读未初始化局部量的 default: 不同, 那条必须回避, 这条必须测
  - AI 通道下标仍是 funcCode.code.vfVoltageSrc - FUNCCODE_vfVoltageSrc_AI1 (只在 AI1..AI3 三支可达, 下标落在 0..2, 不会越界)
  - 整形与限幅的次序不变: 先 ABS_INT16((int16)vfSeprateVolAim) 截断取绝对值, 再在 HALF_SEPARATE 时做频率加权, 再在未运行时把 vfSeparateVol 清零并返回 (此时 vfSeprateVolAim 保留已算出的值), 最后按 motorFc 侧额定电压钳位
  - 两处额定电压仍是两个不同的量, 没有互换: 定标用 funcCode.code.motorParaM1.elem.ratingVoltage, 钳位与滤波器 maxValue 用 motorFc.motorPara.elem.ratingVoltage。差分测试的每个阶段都把两者设成不同的值
  - vfSeprateVolAccLine 的字段写入次序不变 (aimValue → tickerAll → maxValue → curValue → calc → 回读 curValue); 五个数据字段里只有 remainder 是跨调用保留的状态, 其余四个每次都在 calc 之前被覆盖
  - 未新增任何目标文件符号 (只用文件作用域宏、局部量与表达式; 未引入新函数或函数内 static) —— 目标端外部符号集合 16 个逐一相同。这一条也**限制**了改写深度: 把公共定标提成 static 辅助函数会把 _Helper 加进符号集合, 直接违反该不变式, 因此本模块不做函数拆分
  - 未编译的 #elif 1 空实现分支、预处理头 (F_DEBUG_RAM 与 DEBUG_F_POSITION_CTRL 两层开关) 与两侧的 #if/#elif/#endif 结构逐字节保持原样 (由 _check/install_vf.py 在安装时断言)
  - 目标端 .text 尺寸不变 (target_check 口径 230 → 230 字), 两侧告警数均为 0

| 项 | 结果 |
|---|---|
| 差分用例数 | **56,089** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_FrqPidSetDeal`, `_FrqPlcSetDeal`, `_LineChangeCalc`, `_UpdateMultiSetFrq`, `_aiDeal`, `_diFunc`, `_frq`, `_funcCode`, `_maxFrq`, `_motorFc`, `_pulseInSet`, `_runFlag`, `_vfSeparateDeal`, `_vfSeparateVol`, `_vfSeprateVolAccLine`, `_vfSeprateVolAim` |
| `.text` 尺寸 | 230 → 230 (**+0**) |

### `src/function/f_frqSrc.c`

- 变换: 频率给定链。改写手法: 变量改名 (31 组 204 处) + 魔数具名 (9 个文件作用域宏) + 同类型局部量按用途拆名 (一个 tmp 在三处分别是 upperFrqCalc / frqAimAbs / frqSrcValue) + if/else 合并与提前返回 + 共享结果改三元表达式 (GetFrqX / GetFrqY 两处)。改写由 build/_rewrite_frqsrc.py 按一张 47 条『替换表』生成, 每条都带期望命中次数, 计数不符即中止 —— 不是手抄。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_frqSrc/README.md。

- 必须守住的不变量:

  - 函数名 17 个与数据符号 50 个 (46 标量 + 4 聚合) 全部不变; 目标端外部符号集合逐一相同, 未新增或删除任何符号 —— 未引入新函数、未引入函数内 static, 只用文件作用域宏、局部量与表达式。这一条也**限制**了改写深度: 把公共定标提成 static 辅助函数会把符号加进符号集合, 直接违反该不变式, 因此本模块不做函数拆分
  - 所有算术表达式的类型宽度与运算次序保持: Q15 定标仍是 ((int32)X * Y + FRQ_Q15_ROUND) >> FRQ_Q15_SHIFT 的**算术右移** (对负数是向下取整, 与除法不同, 故不能改成 / 32768); 百分比定标仍是 int32 除法; 未新增或删除任何强制转换
  - 魔数具名只是把字面量换成同值宏, 宏名与出处一一对应: FRQ_PERCENT_SCALE 10000 / FRQ_PERCENT_ROUND 5000 / FRQ_PERCENT 100 / FRQ_SLOPE_SCALE 10000 / FRQ_Q15_ROUND (1 << 14) / FRQ_Q15_SHIFT 15 / FRQ_PU_HEADROOM_STEPS 20 / FRQ_SRC_SELECT_DIVISOR 10 / FRQ_PLC_SET_SCALE 1000。令牌总数 5634 -> 5630 只减不增 (减少的 4 个来自 if/else 合并与三元改造)
  - 变量改名逐条核对**成员访问**不变: 每条重命名的 .name 与 ->name 出现次数改写前后完全相同 (31 组全部通过) —— 这一条是为了钉死 p->low 这类『只改词法、却改错了对象』的错
  - frqCalcSrcOld 的**影子语义**不变: 它仍是『上一拍的计算方式』, 每次都在本拍用完后再更新, 没有提前或滞后一拍
  - 六段 #if 0 死代码逐字节不变 (以原文件行号计: 170~192 / 890~897 / 1028~1037 / 1099~1109 / 1315~1326 / 1373~1382); 由 build/_rewrite_frqsrc.py 在模型层 (保护行未被消费) 与输出层 (整段文本仍在) 双向断言, 不是靠人眼
  - 预处理结构不变: 顶部 #if F_DEBUG_RAM / #elif 1 两层开关, 以及第 915 行那处 #if (DEBUG_F_POSITION_CTRL) 后面紧跟的是**裸块** (不是 #else) —— 该开关为 0 时 if/else 整段消失只剩块, 块里的 runMode 赋值是活代码, 改写时原样保留
  - 取值来源的 switch 分支与 default 语义逐一保留: 分支没有合并、没有改成查表、没有提前返回; 每个 case 写的目标量与改写前相同
  - 与硬件的全部边界仍是那三个桩函数 (FrqPidSetDeal / GetNumberDigit1 / TorqueCalc), 调用次序与实参不变 —— 差分测试把桩调用轨迹 (谁、什么实参、什么顺序) 也纳入比对
  - 目标端 .text 尺寸 2486 -> 2484 字 (差 -2, build_and_compare.py 口径: **关优化**编译, 灵敏度最高), 两侧告警数均为 0, 符号集合相同
  - **按工程实际优化选项编译时, 改写收敛到同一份机器码**: Debug (-o2) 反汇编 2295 行、Release (-o3) 2293 行, 与基准**逐行相同, 0 处差异**; 因此 f_frqSrc 对整机映像的贡献是 **0 字节** —— '关优化下差 2 字' 是灵敏度口径下的产物, 真实交付映像没有因这个模块变化。证据: tools/difftest/o2_check.py -> docs/_difftest/f_frqSrc.o2.json

| 项 | 结果 |
|---|---|
| 差分用例数 | **47,949** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CanRxTxCon`, `_FrqAimUpDownDeal`, `_FrqPidSetDeal`, `_FrqPlcSetDeal`, `_FrqSrcDeal`, `_FrqSrcOperate`, `_FrqXyCalc`, `_GetFrq`, `_GetFrqComp`, `_GetFrqX`, `_GetFrqY`, `_GetNumberDigit1`, `_JumpFrqDeal`, `_JumpFrqEndCalc`, `_ResetUpDownFrq`, `_TorqueCalc`, `_UpdateFrqAim`, `_UpdateLimitFrq`, `_UpdateMultiSetFrq`, `_UpdateRunMode`, `_accDecFrqPrcFlag`, `_accDecTimeSrcPlc`, `_aiDeal`, `_bAntiReverseRun`, `_bFrqDigital`, `_bFrqDigitalDone4WaitDelay`, `_bPlcEndOneLoop`, `_bStopPlc`, `_benchFrq`, `_decNumber`, `_diFunc`, `_diSelectFunc`, `_dpFrqAim`, `_errorAttribute`, `_frq`, `_frqAiPu`, `_frqAim`, `_frqAimDisp`, `_frqAimOld4Dir`, `_frqAimPLCDisp`, `_frqAimTmp`, `_frqAimTmp0`, `_frqCalcSrc`, `_frqCalcSrcOld`, `_frqCurAim`, `_frqCurAimFrac`, `_frqDigitalPlc`, `_frqDigitalTmp`, `_frqDroop`, `_frqFlag`, `_frqFrac`, `_frqKeyUpDownDelta`, `_frqMax`, `_frqMin`, `_frqPlcType`, `_frqPuQ15`, `_frqRun`, `_frqTmp`, `_frqTmpFrac`, `_frqXDisp`, `_frqXy`, `_frqYDisp`, `_funcCode`, `_getUpDownLimit`, `_jumpFrq1`, `_jumpFrq2`, `_keyFunc`, `_lowerFrq`, `_maxFrq`, `_menu0DispStatus`, `_motorFc`, `_p2pData`, `_plcStep`, `_plcStepOld`, `_plcStepRemOld`, `_plcTime`, `_plcTimeRemOld`, `_presetFrqOld`, `_pulseInSet`, `_reverseTicker`, `_runCmd`, `_runCmdBind`, `_runDirPanelOld`, `_runFlag`, `_runMode`, `_runSrc`, `_runStatus`, `_startRunStatus`, `_swingStatus`, `_timeBench`, `_torqueAim`, `_uPDownFrqLower1`, `_uPDownFrqLower2`, `_uPDownFrqMax`, `_uPDownFrqMin`, `_upDownFrq`, `_upDownFrqInit`, `_upDownFrqTmp`, `_upperFrq` |
| `.text` 尺寸 | 2486 → 2484 (**-2**) |

### `src/function/f_runSrc.c`

- 变换: 运行指令与运行状态机 (T1 里 .text 最大的一块, 2852 字)。改写手法: 变量改名 (61 组 233 处, 只动函数内局部量与文件内 static, **一个全局名都没动**) + 魔数具名 (29 个文件作用域宏, 含掉电/休眠/时基/定标各一组) + 结构改写 (32 处): StopRunCtrl 的『共享结果先赋值再分支』改三元、ShutDownRunCtrl 停机过程两支合同一结果改三元、DormantDeal 两处『零则立即, 否则计时』合并成一个 || 判据、UpdateRunCmd 重复的显示状态赋值上提 + 空 else-if 删除 + 两处 jog 清零合并、AccDecTimeCalc 段内 if/else 改三元 + 档位常量具名、UpdateBenchTime 时基常量具名。改写由 build/_rewrite_runsrc.py 按一张『替换表』生成 (每条带期望命中次数, 计数不符即中止), 不是手抄; 该脚本还会自算活/死代码并双向断言。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_runSrc/README.md。

- 必须守住的不变量:

  - 本模块定义的 64 个外部符号 (19 函数 + 45 数据) 与引用的 52 个外部符号 (4 函数 + L$$DIV + 47 数据) 全部不变; 目标端目标文件的外部符号集合 115 个逐一相同, 未新增或删除任何符号 —— 未引入新函数、未引入函数内 static, 只用文件作用域宏、局部量与表达式。这一条同样**限制**了改写深度: 把重复片段提成 static 辅助函数会把符号加进符号集合, 直接违反该不变式, 因此本模块不做函数拆分
  - 所有算术表达式的类型宽度与运算次序保持: 定标一律 int32 除法或**算术右移** (右移对负数向下取整, 与除法不同, 不能互换); 未新增或删除任何强制转换; 未合并、未重排任何乘除链
  - 魔数具名只是把字面量换成同值宏, 宏名与出处一一对应: RS_PERCENT_SCALE 1000 / RS_PERCENT 100 / RS_AI_Q15_ROUND (1 << 14) / RS_AI_Q15_SHIFT 15 / RS_LOSE_LOAD_FRQ_NUM 7 / RS_TUNE_IM_FRQ_NUM 4 / RS_TUNE_IM_FRQ_DEN 5 / RS_DC_BUS_SCALE 14482 / RS_DC_BUS_SHIFT 10 / RS_POFF_THRESHOLD_SCALE 131 / RS_POFF_THRESHOLD_SHIFT 17 / RS_POFF_SLOPE_SCALE 1000 / RS_STOP_BRAKE_MIN_FRQ 50 / RS_LOWER_FRQ_STOP_TICK 2 / RS_RUN_TIME_MIN_SCALE 10 / RS_TENTH_MIN_SEC 6 / RS_RUN_TIME_TICK_MAX ((Uint16)(TIME_UNIT_MS_PER_SEC / RUN_TIME_CAL_PERIOD)) / RS_SWING_DELTA_SHIFT 5 / RS_TIME_BENCH_{0,1,2}POINT {1000,100,10} / RS_DORMANT_{HIGH,LOW,STOP,START}_FC {31,32,33,34} / RS_ACC_DEC_SEL_{1,2,3,4} {0,1,2,3}。令牌总数 8342 -> 8400 (**+58, 只增不减**): 增量全部来自这 29 条 #define 与 UpdateRunCmd 里被显式写全的那一个析取项, 不是表达式被改写 —— 本模块与 f_frqSrc 不同, 不做『令牌数只减不增』的承诺, 该数字如实记录
  - 变量改名逐条核对**成员访问**不变: 每条重命名的 .name 与 ->name 出现次数改写前后完全相同 (61 组全部通过)。这里踩过一次坑: `common` 是 `common0` 的前缀, 用 str.count 会把 `.common0` 也算进去而假报差异, 检查因此改成词边界正则 —— 见 build/_rewrite_runsrc.py 的校验 5
  - UpdateRunCmd 的错误等级分支: 原来『if(FREE_STOP||…) { if(common0) 显示错误; freeStop=1 } else if(STOP) { if(common0) 显示错误; common0=0 }』里两处**逐字相同**的显示赋值被上提为一个带完整析取的 if。上提时**必须**把第三个析取项 `((runStatus==TUNE)||(dspMainCmd1.bit.speedTrack)) && errorCode` 一起带上 —— 第一次生成漏了它, 差分测试立刻在 random 段报出 55 处 menu0DispStatus 不一致 (ref=2 new=0/1)。修正后 0 不一致; 这条留在清单里是因为它是本模块唯一一处『改写真的改错了』的实例
  - 六类 #if 0 死代码与 DEBUG_F_POSITION_CTRL==0 死代码逐字节不变: 共 74 段、393 行 (以原文件行号计, 含指令行), 其中**内容行** 288 行 / 30 段由 build/_rewrite_runsrc.py 在模型层 (保护行未被消费) 与输出层 (整段文本仍在) 双向断言。死代码区间由该脚本按 f_debug.h + defines.txt 的宏取值**机械算出**, 不手抄 —— 手抄版本曾经漏掉一处, 让一条变异锚在了死代码上 (见下条)
  - 预处理结构不变: 105 条预处理指令的顺序与内容逐一相同 (含顶部 #if F_DEBUG_RAM / #elif 1 两层开关, 以及文件里 55 处条件编译)。死代码的**指令行**允许被结构性编辑跨过 (例如把 `#if DEBUG_F_JOG` 成对指令之间的两段合并), 但输出里的指令序列必须与原文件完全相同 —— 这条也由脚本断言
  - 文件里原本混用制表符 (53 行): 活代码的 51 行统一换成 4 空格, 死代码的 2 行**原样保留制表符**并逐字节未动 (按内容序列比对, 不按行号 —— 前面插了行, 行号已经漂移)
  - 取值来源的 switch 分支与 default 语义逐一保留: 分支没有合并、没有改成查表、没有提前返回; 每个 case 写的目标量与改写前相同 (含 ERR_RUN_FRQ_* 五个分支、START_RUN_STATUS_* 状态机的落空穿透、TUNE_STEP_* 的 END 落空到 DEC)
  - 与硬件的全部边界仍是那四个桩函数 (AccDecFrqCalc / UpdateFrqAim / GetErrorAttribute / SaveTuneData), 调用次序与实参不变 —— 差分测试把桩调用轨迹 (谁、什么实参、什么顺序) 也纳入比对
  - 目标端 .text 尺寸 3264 -> 3259 字 (差 -5, build_and_compare.py 口径: **关优化**编译, 灵敏度最高), 两侧告警数均为 0, 符号集合相同
  - **按工程实际优化选项编译时, 改写收敛到同一份机器码**: Debug (-o2) 与 Release (-o3) 的反汇编与基准逐行相同、0 处差异, 证据见 docs/_difftest/f_runSrc.o2.json

| 项 | 结果 |
|---|---|
| 差分用例数 | **49,144** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AccDecFrqCalc`, `_AccDecTimeCalc`, `_DormantDeal`, `_GetErrorAttribute`, `_JogRunCtrl`, `_LoseLoadRunCtrl`, `_LowerThanLowerFrqDeal`, `_NormalRunCtrl`, `_PowerOffNoStopDeal`, `_RunSrcDeal`, `_RunSrcUpdateFrq`, `_SaveTuneData`, `_ShutDownRunCtrl`, `_StartRunCtrl`, `_StopRunCtrl`, `_SwingDeal`, `_TuneRunCtrl`, `_UpdateBenchTime`, `_UpdateFrqAim`, `_UpdateRunCmd`, `_UpdateSwingPara`, `_accDecTimeSrcPlc`, `_accFrqTime`, `_accTimeOld`, `_aiDeal`, `_bAntiReverseRun`, `_bFrqCurAimChg`, `_bRunJog`, `_bStopPlc`, `_commRunCmd`, `_curTime`, `_decFrqTime`, `_decTimeOld`, `_diFunc`, `_diSelectFunc`, `_dormantStatus`, `_dspMainCmd`, `_dspMainCmd1`, `_dspStatus`, `_dspSubCmd`, `_dspSubStatus`, `_errorAttribute`, `_errorCode`, `_errorOther`, `_frq`, `_frqAim`, `_frqAimOld4Dir`, `_frqAimTmp`, `_frqAimTmp0`, `_frqCurAim`, `_frqCurAimOld`, `_frqFdbTmp`, `_frqFlag`, `_frqFrac`, `_frqLine`, `_frqRun`, `_frqTmp`, `_frqTmpFrac`, `_funcCode`, `_funcCodeRwMode`, `_generatrixVoltage`, `_invPara`, `_invtStatus`, `_keyFunc`, `_lowerDelayTicker`, `_lowerFrq`, `_mainLoopTicker`, `_maxFrq`, `_menu0DispStatus`, `_motorCtrlTuneStatus`, `_motorFc`, `_motorSn`, `_otherStopLowerSrc`, `_otherStopSrc`, `_plcStep`, `_plcStepRemOld`, `_plcTime`, `_plcTimeRemOld`, `_powerOnStatus`, `_powerUpTimeAddupTicker`, `_runCmd`, `_runDirPanelOld`, `_runFlag`, `_runMode`, `_runSrc`, `_runStatus`, `_runStatus1Step`, `_runStatusOld4Jog`, `_runStatusOld4LoseLoad`, `_runStatusOld4POffNoStop`, `_runTimeAddupTicker`, `_runTimeCal`, `_runTimeTicker`, `_saveTuneDataFlag`, `_setRunLostTime`, `_setRunTimeAim`, `_setTimeRun`, `_shuntFlag`, `_shuntTicker`, `_startProtectSrc`, `_startRunStatus`, `_stopRemFlag`, `_stopRunStatus`, `_swingAccTime`, `_swingDecTime`, `_swingDoubleAw`, `_swingFrqLimit`, `_swingJumpFrq`, `_swingMaxFrq`, `_swingMinFrq`, `_swingStatus`, `_timeBench`, `_tuneCmd`, `_upDownFrq`, `_upperFrq` |
| `.text` 尺寸 | 3264 → 3259 (**-5**) |

### `src/function/f_frqSrc_pid.c`

- 变换: PID 频率源 (T1 加减速与 PID 的一块, .text 1707 -> 1718 字, 差 +11)。整个驱动器的 PID 频率给定由它算出: 每拍按功能码选定给定源与反馈源, 跑增量式 PID, 滤波后输出频率; 状态跨拍保留 (整个 PID_FUNC 块、两个低通、一个给定斜坡、三个计时器、一个状态枚举、一个标志)。改写手法: 变量改名 (47 组 322 处, 只动函数内局部量与**文件内 static**, 一个全局名都没动) + 魔数具名 (23 个文件作用域宏) + 结构改写 (4 处)。改写由 build/_rewrite_frqsrcpid.py 按一张「替换表」生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入等于冻结基准, 再自算活/死代码并双向断言。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_frqSrc_pid/README.md。

- 必须守住的不变量:

  - 本模块定义的 13 个外部符号 (4 函数 + 9 数据) 与引用的 20 个外部符号 (4 函数 + 16 数据) 全部不变; 目标端目标文件的外部符号集合 33 个逐一相同, 未新增或删除任何符号 —— 未引入新函数、未引入函数内 static, 只用文件作用域宏、局部量与表达式。这一条同样**限制**了改写深度: 把重复片段提成 static 辅助函数会把符号加进符号集合, 直接违反该不变式, 因此本模块不做函数拆分。两个文件内 static 改了名 (pidCalcFlagOld -> s_pidCalcFlagLast, pidChgOld -> s_pidParaChgDiLast) —— 它们是**局部符号**, 改名不进符号集合 (target_check.py 的 real_syms() 按 COFF storage class == 2 (C_EXT) 判定对外可见, 两类 static 都被排除; 该判据在 f_canlink 上由启发式改为 storage class, 见 cases/f_canlink/README.md 第 3 节)
  - 所有算术表达式的类型宽度与运算次序保持: 三处 int64 提升 (`(int64)kpGain * (...)`, `(int64)kiGain * ((pidError + pid->error) >> 1)`, `(int64)kdGain * (...)`) 与 int64 的 `deltaIRem` 累减一字未改; 定标一律 int32 除法或**算术右移** (右移对负数向下取整, 与除法不同, 不能互换); 未新增或删除任何强制转换; 未合并、未重排任何乘除链
  - 魔数具名只是把字面量换成同值宏, 宏名与出处一一对应: PID_Q15_ONE 0x7fff / PID_Q15_SHIFT 15 / PID_Q15_ROUND (1 << 14) / PID_Q16_SHIFT 16 / PID_QP_INIT 24 / PID_QI_INIT 26 / PID_QD_INIT 12 / PID_KP_NORM_LIMIT (1L << 15) / PID_KI_NORM_LIMIT (1L << 16) / PID_QP2_ALIGN 16 / PID_KI2_ALIGN 20 / PID_KI2_ALIGN_MID 14 / PID_PERMILLE 1000 / PID_PERCENT 100 / PID_PER_TEN_THOUSAND 10000 / PID_DIFF_TD_UNIT 25 / PID_DIFF_ERR_COEFF 3 / PID_FC_SET_SCALE 2097 / PID_FC_SET_SHIFT 6 / PID_COMM_SCALE 13421 / PID_COMM_SHIFT 12 / PID_DELTA_SCALE 53687 / PID_DELTA_SHIFT 14。令牌总数 2916 -> 3022 (**+106, 只增不减**): 增量**全部**来自这 23 条 #define (恰好 106 个令牌), 表达式本身一个令牌都没加。原有的 PID_ERROR_DEAD 20 未动
  - 变量改名逐条核对**成员访问**不变: 每条重命名的 `.name` 与 `->name` 出现次数改写前后完全相同 (47 组全部通过)。这里踩过一次坑: `outMax` / `outMin` / `aimValue` / `fdb` 既是局部量名**又是**结构成员名, 直接全局替换会把 `->outMax` 也改掉; 修法是给这些名字加 `(?<![.>])` 前视排除。是脚本的成员访问校验把它抓出来的 —— 见 build/_rewrite_frqsrcpid.py 校验 5
  - FrqPidSetDeal 的两处「共享结果先赋值再分支」上提: (1) 计算使能判据 `(YES == pidCalcMode) || (runCmd.bit.common)` 上提为局部量 bPidCalcEnable, 分支从「if (enable) { if (NO_CALC == pidCalcFlag) pidCalcFlag = CALC; } else { pidCalcFlag = NO_CALC; }」改成「if (!bPidCalcEnable) NO_CALC; else if (NO_CALC == pidCalcFlag) CALC;」—— 两支互斥且都对 pidCalcFlag 只写一次, 没有读后写依赖, 因此顺序调换等价; (2) 复位判据 `(!pidCalcFlag) || (frqCalcSrcOld != frqCalcSrc) || ((plcStepOld) && (!plcStep))` 上提为 bPidReset, 三个析取项一字未改, 且**没有**把它改成提前 return
  - UpdatePidFuncRefFdb 的反馈上限钳位 `if (feedback > 0x7fff) { feedback = 0x7fff; }` 改成 `feedback = GetMin(feedback, PID_Q15_ONE);`。GetMin 是 include/function/f_main.h 第 14 行的既有宏 `((x) < (y)) ? (x) : (y)`, 本文件**原本就在用** (第 542 行), 不是新引入的设施; 逐位等价, 且保持整型语义 (没有换成 fmin 或浮点比较)。宏会二次求值 x, 这里 x 是平凡变量 feedback, 无副作用
  - 死代码的**内容行**逐字节不变, 共 6 段 24 行 (以冻结基准 1-based 行号计): 第 10 行与第 16 行 (两行**未被选中**的 `#define DEBUG_F_PID             0`, 分别位于 `#if F_DEBUG_RAM` 支与 `#elif 1` 支) / 第 29~36 行 (`#if 0` 掉的 struct PID_PARA_LINE 及其定义, 8 行含第 29 行空行) / 第 473~477 行 (`#if 0` 掉的 "aimValue < 0 则清 0", 5 行含空行) / 第 525~529 行 (`#if 0` 掉的 "fdb < 0 则清 0", 5 行含空行) / 第 571~574 行 (`#elif 1` 支的空桩 FrqPidSetDeal / PidFuncCalc, 4 行含前后空行)。**冻结对象是内容行, 不含 `#if 0` / `#else` / `#endif` / `#elif 1` 这类指令行** —— 指令行允许被结构性编辑跨过 (D12 的约定), 本模块没有做这种跨指令编辑, 所以交付文件里的指令行实际上也一字未动。核对走两条独立路径: build/_rewrite_frqsrcpid.py 在模型层 (保护行未被消费) 与输出层 (整段文本仍在) 双向断言, 另有 build/_check_deadcode_frqsrcpid.py 只读基准与已交付文件按内容搜索逐段核对 (并断言零制表符、零非 ASCII)。两处 `#define DEBUG_F_PID             0` **逐字相同**, 单行文本当搜索键不唯一, 核对时必须带上相邻指令行作上下文 —— 记在这里以免误判为死代码被动过
  - 死代码判定用**条件栈**而不是区间表: 本文件顶部的编译分支是四层 #if 塔 (`#if F_DEBUG_RAM` / `#elif 1` / `#if !DEBUG_F_POSITION_CTRL` / `#elif 1`), 且第 28~39 行的 `#if 0 ... #else ... #endif` 里 **#else 支是活的** (定义 LINE_STRUCT pidParaLine) —— 一个区间表达不了这种结构, 所以 _rewrite_frqsrcpid.py 逐行维护条件栈来算活/死, 不手抄区间
  - 预处理结构与取值不变: 顶部两层开关与尾部 `#if DEBUG_F_PID / #elif 1 / #endif` 原样保留, DEBUG_F_PID 仍解析为 **1** (F_DEBUG_RAM=0 -> #elif 1 -> !DEBUG_F_POSITION_CTRL=1 -> 1), 活代码仍是第 25~569 行, 尾部空桩仍是死代码。原有 46 条预处理指令的顺序与内容逐一相同
  - 制表符: 原文件第 142/143 行 (pidLpf[PID_LPF_OUT] 的清零) 与第 430 行 (p->errorSmall) 用制表符缩进, 输出统一成空格 —— 交付文件零制表符、纯 ASCII、CRLF, 由 verify_tree.py 强制
  - 局部静态量不可达但两侧同步: pidCalcFlagOld (FrqPidSetDeal) 与 pidChgOld (PidFuncCalc) 被 cl2000 加了 `$N` 后缀, 是**局部符号**, driver 装不进去也读不出来。这不是漏洞: run_case() 保证两侧跑**完全相同**的调用序列与初态, 因此只要两份实现等价, 两个 static 就同步演化。代价是 driver 无法**复位**它们 —— pidChg 沿检测块与 CALC -> INIT_HOLD 跳转的可达性依赖前序调用, 见 cases/f_frqSrc_pid/README.md 第 5 节
  - 六条既有缺陷**有意不测**, 逐条列在 cases/f_frqSrc_pid/driver.c 头部与 README 第 6 节, 而不是悄悄绕开: (a) DI 支的 `p->Qi - tmp1 - tmp2` 是**负移量**且 Ki 分母无守卫; (b) tmp2 的判据 `Qi >= Qd` 与移量 `Qd - Qi` 方向不一致; (c) p->fdb 漏了输入上那层 `> 0x7fff` 钳位; (d) 除以 maxFrq / pidTi / pidTi2 / (x2 - x1); (e) p->outMin 未钳位, 可达 ~2^31/maxFrq; (f) UpdatePidFuncRefFdb 声明 `int32 aimValue; int32 fdb;`, 在两个 switch 的**每个 case 里都赋值、唯独 default 不赋值**, 却随后无条件读取 —— 越界的 pidSetSrc / pidFdbSrc 会把**不确定值**带进 pidFuncRef.aimValue / p->fdb。最后一条是**真缺陷但不可测** (两侧是不同函数, 栈帧残值不保证相同), 驱动因此把 pidSetSrc/pidFdbSrc 收窄到功能码表能实际产出的取值 (0..6 / 0..8)。改这些是**行为变更**, 不是改写, 故一律保持原样
  - 差分测试: 冻结基准 vs 交付源码, 21411 个用例 0 处不一致 (phase_para 200 / phase_ref 548 / phase_calc 289 / phase_setdeal 215 / stateful sequences 159 / random 20000); 负向对照 176 / 176 全部抓到, 变体未跑起来 0 (11 条边界等价变异经证明后排除, 编号留空: M046/M049/M077/M079/M084/M087/M129/M139/M142/M144/M172, 证明见 cases/f_frqSrc_pid/README.md 第 8 节)

| 项 | 结果 |
|---|---|
| 差分用例数 | **21,411** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_FrqPidSetDeal`, `_LineCalc`, `_LineChangeCalc`, `_LpfCalc`, `_PidFuncCalc`, `_UpdateMultiSetFrq`, `_UpdatePidFuncPara`, `_UpdatePidFuncRefFdb`, `_aiDeal`, `_bPidFdbLose`, `_diFunc`, `_errorOther`, `_frqAiPu`, `_frqCalcSrc`, `_frqCalcSrcOld`, `_frqFlag`, `_funcCode`, `_lowerFrq`, `_maxFrq`, `_pidCalcFlag`, `_pidError`, `_pidFdbLoseTicker`, `_pidFunc`, `_pidFuncRef`, `_pidInitTicker`, `_pidLpf`, `_pidParaLine`, `_plcStep`, `_plcStepOld`, `_pulseInSet`, `_runCmd`, `_runFlag`, `_upperFrq` |
| `.text` 尺寸 | 1707 → 1718 (**+11**) |

### `src/function/f_canlink.c`

- 变换: CAN_LINK / PLC 卡的 CAN 驱动 (451 行, 12 个对外函数 + 3 个对外全局量, .text 703 -> 662 字)。改写手法: 变量改名 (21 组 183 处 —— 两个**文件内** static sBaud/sSourID -> s_canBaudIndex/s_canStationId, 三个**函数内** static count/timeOutErr/initFlag -> s_tranRetryCount/s_recTimeoutCount/s_canInitFlag, 以及 16 个局部量与形参) + 魔数具名 (17 个文件作用域宏) + 结构改写 (5 处) + 排版统一 (154 行的 279 个制表符展开成 4 空格)。改写由 build/_rewrite_canlink.py 按一张 47 条『替换表』生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再自算活/死代码并双向断言, 最后核对成员访问次数与 15 个对外符号的出现次数。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_canlink/README.md。

- 必须守住的不变量:

  - 本模块定义的 15 个外部符号 (12 函数 + 3 数据) 与引用的 18 个外部符号 (10 函数 + 8 数据) 全部不变; 目标端目标文件的外部符号集合 33 个逐一相同, 未新增或删除任何符号 —— 未引入新函数、未引入函数内 static, 只用文件作用域宏、局部量与表达式。这一条同样**限制**了改写深度: 把重复片段提成 static 辅助函数会把符号加进符号集合, 直接违反该不变式, 因此本模块不做函数拆分。三个函数内 static 改了名 —— 它们是**局部符号** (cl2000 加 `$N` 后缀去重), 改名不进符号集合; 两个文件内 static 也是局部符号, 但 cl2000 **不给它们加 `$N`** (`_sBaud`), 这一点让 target_check.py 早期按名字形状判定『对外可见』的启发式当场假报差异, 判据已改为 COFF storage class == 2 (C_EXT), 见 cases/f_canlink/README.md 第 3 节
  - 所有算术表达式的类型宽度与运算次序保持: canId.all 仍是 Uint32 的或/移位组合 (0xff | (0xfful<<16) | (1ul<<24) | (1ul<<29) | (0ul<<30) | (1ul<<31)), 接收模板仍是 (1ul<<30) | (1ul<<31), 验收掩码仍是 (~(0xfful<<8)) & (~(7ul<<29)); funcCode.code.u3[11] 的饱和递增仍是 Uint16 与 CANLINK_COUNTER_MAX 的无符号比较; 未新增或删除任何强制转换
  - 魔数具名只是把字面量换成同值宏, 宏名与出处一一对应: CANLINK_TRAN_TIMEOUT 1000 (三处调用点的超时实参) / CANLINK_REC_TIMEOUT_MAX 4 (收帧超时阈值, 原文 `> 4`) / CANLINK_PLC_PWRON_SEC 10 / CANLINK_COUNTER_MAX 65535 (三处饱和上限) / CANLINK_INIT_DONE 0xcc / CANLINK_CODE_ERR 0xff / CANLINK_RSP_ERR_HI 0x8001 / CANLINK_RSP_PWD_OK 0x8888 / CANLINK_REMOTE_LEN 8 / CANLINK_CONTROL_LEN 4 / CANLINK_MBOX_OVR_BIT 0x40 / CANLINK_TRAN_ID_TEMPLATE / CANLINK_REC_ID_TEMPLATE / CANLINK_REC_ACCEPT_MASK / CANLINK_IDENTITY_LO 0 / CANLINK_IDENTITY_HI 0x2774 (CanLinkChara[2] 的初值表) + CANLINK_TRAN_TIMEOUT 复用为三处 `, 1000)` 实参。令牌总数 1980 -> 1989 (**+9**): 增量来自这 17 条 #define, 被结构改写删掉的 else 分支与三分支选值抵消掉大部分 —— 数字如实记录, 不承诺『只增不减』
  - 变量改名逐条核对**成员访问**不变: 每条重命名的 `.name` 与 `->name` 出现次数改写前后完全相同 (21 组全部通过)。这一条是为了钉死 `p->low` 这类『只改词法、却改错了对象』的错。这里踩过一次坑: `msgid` 与结构成员 `msgid` 同名, 直接全局替换会把 `frameBuf->msgid` 也改掉; 修法是给局部量那条加 `(?<![.>])` 前视排除
  - 死代码的**内容行**逐字节不变, 共 5 段 8 行 (以冻结基准 1-based 行号计): 第 46 行 (`#if CAN_REMOTE_EN` 掉的 `InitTranMbox(AAM_MBOX_N, ...)`) / 第 139 行 (`#if (1 == CAN_LINK_TIME_DEBUG)` 掉的 `Uint32 CanTime1, CanTime2, CanTime;`) / 第 147 行 (同开关掉的 `CanTime1 = ECANREGS.CANTSC;`) / 第 267~268 行 (同开关掉的 CanTime2/CanTime 相减) / 第 448~450 行 (`#elif 1` 支的空桩 `void CanlinkFun(void){}` 及其前后空行)。**冻结对象是内容行, 不含 `#if` / `#elif` / `#endif` 这类指令行**; 核对由 build/_rewrite_canlink.py 在模型层 (保护行未被消费) 与输出层 (整段文本仍在) 双向断言
  - 制表符: 原文件 154 行用制表符缩进, 活代码全部展开成 4 空格; 死代码那 5 行**原样保留制表符**并逐字节未动 —— 所以『输出零制表符』这个断言在 f_canlink 上不成立, 脚本的断言改成『输出制表符总数 == 冻结死代码行的制表符总数』。交付文件零非 ASCII、CRLF, 由 verify_tree.py 强制
  - CanlinkDataTran 的重试计数语义不变: 发送成功清 0 后返回 SUCC, 超时清 0 后返回 TIMEOUT, 忙碌时递增后返回 BUSY; 判据仍是 `++count >= timeOut` (改写成等价的 `if (++count < timeOut) return BUSY;` 提前返回, 边界与副作用次序逐条对应)。这个 static 不可达也读不出来 —— **但 driver 把它读出来了**: probe_tran_residue() 在每个可能留下残留的 phase 之后发一次全失败、小 timeOut 的调用, 让残留值本身成为被比对的返回值。工程里**所有**调用点都传 timeOut = 1000 (f_canlink.c 两处 + f_plc.c 一处), 阈值这么高时『成功后清零』与『超时后清零』在真实程序里不可分辨, 所以不这么做的话三条只扰动 count 的变异会全数通过
  - CanControlWriter 的 eeprom 锁存语义不变: `if (eeprom) commRcvData.commCmdSaveEeprom = SCI_WRITE_WITH_EEPROM;` 在 CommWrite 之前置位、之后无条件清成 NO_EEPROM。参数 `eeprom` 的**唯一**对外效果就是 CommWrite 进入时看到的那个锁存值 —— 这不是测试台的臆造: 真实 f_comm.c 里 CommWrite -> CommRwFuncCode 正是读 commRcvData.commCmdSaveEeprom 来决定要不要 SaveOneFuncCode (f_comm.c:960)。所以 mock 的 CommWrite/CommRead 多记两个词 (锁存值 + sciFlag.all), 否则 CanControlWriter(addr,data,0) 与 (addr,data,1) 从外面看完全一样
  - CanlinkFun 的波特率/站号选择由『三个分支各写一对赋值』折成『两个局部量 wantBaud/wantId + 一次比较』: 原三分支互斥且覆盖同一组目标量 (s_canBaudIndex / s_canStationId), 每个分支内两个赋值都是常量, 因此折算不改变取值; 判据仍是 funcCode.code.plcEnable / p2pEnable / p2pTypeSel / commBaudRate / commSlaveAddress, 没有把 CAN_LINK_S_ADDR 与 CAN_LINK_BAUD_SEL 的算式写反
  - 收帧分发循环改用独立邮箱下标 mbox (原来复用 recStatus): 两者取值相同 (循环都从 0 起、每轮 +1), 但分开后 mbox 只做下标、recStatus 只装 eCanDataRec 的返回值, 语义更清楚; 邮箱号算式 (REC_BOX_N - mboxIndex) 与 (TRAN_BOX_N - mboxIndex) 的减法方向未变 —— 这是最容易被『看起来更自然』改错的一处, 目标端符号集合与差分测试都在钉它
  - CanLinkConDeal 的两个 `if (err)` 合并成一个: 原第一个 if 写错误码/错误回帧内容, 第二个 if 递增饱和计数, 两者之间只夹一个 `else if (sciFlag.bit.pwdPass)` 的密码回帧分支, 且该分支只在 `err == 0` 时可达。合并成『if (err) { ... ; if (u3[11] < 65535) u3[11]++; } else if (sciFlag.bit.pwdPass) { ... }』后求值顺序与副作用逐条对应
  - 与硬件的全部边界仍是那十个桩函数 (InitdspECan / InitTranMbox / InitRecMbox / eCanDataTran / eCanDataRec / CommWrite / CommRead / PlcDataFramDeal / InvTranTabCfg / InvRecTabCfg), 调用次序与实参不变 —— 差分测试把桩调用轨迹 (谁、什么实参、什么顺序) 也纳入比对
  - hazard (d) 有意不测: CanLinkConDeal 的 `CAN_LINK_R_CFG` / `CAN_LINK_R_INFO` / `CAN_LINK_R_WAR` 三条臂不给 `err` 赋值, 之后的 `if (err)` 读的是栈上残留。这是真缺陷, 但**不可测** (两侧是不同的函数, 栈帧不保证相同), driver 因此把命令码收窄到协议实际使用的取值。改它是**行为变更**, 不是改写, 故保持原样
  - 目标端 .text 尺寸 703 -> 662 字 (差 -41, build_and_compare.py 口径: **关优化**编译, 灵敏度最高), 两侧告警数均为 0, 符号集合相同

| 项 | 结果 |
|---|---|
| 差分用例数 | **20,175** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CanControlRead`, `_CanControlWriter`, `_CanHostTx`, `_CanLinkChara`, `_CanLinkConDeal`, `_CanLinkRemoteDeal`, `_CanLlinkDataDeal`, `_CanRxTxCon`, `_CanlinkDataRec`, `_CanlinkDataTran`, `_CanlinkFun`, `_CanlinkRecBuf`, `_CommRead`, `_CommWrite`, `_InitCanlink`, `_InitCanlinkRec`, `_InitCanlinkTran`, `_InitRecMbox`, `_InitTranMbox`, `_InitdspECan`, `_InvRecTabCfg`, `_InvTranTabCfg`, `_PlcDataFramDeal`, `_commRcvData`, `_commReadData`, `_curTime`, `_eCanDataRec`, `_eCanDataTran`, `_errorInfo`, `_errorOther`, `_funcCode`, `_p2pData`, `_sciFlag` |
| `.text` 尺寸 | 703 → 662 (**-41**) |

### `src/function/f_profibus.c`

- 变换: DP (PROFIBUS-DP) 从站帧协议 (473 行, 7 个对外函数 + 9 个对外全局量, .text 676 -> 684 字)。改写手法: 变量改名 (35 组 155 处) + 魔数具名 (18 个文件作用域宏) + 结构改写 (2 处) + 排版统一 (32 行 47 个制表符展开成 4 空格)。两处结构改写是: (1) 把 ProfibusSendDataDeal 里那段 EEPROM 索引锁存提成 `static void DpLatchEepromIndex(void)`; (2) 把 PZD 循环里复用的两个临时量 group/grade 去掉, 把 `/ 100` `% 100` 直接写进 GetGradeIndex 的实参。改写由 build/_rewrite_profibus.py 按一张替换表生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再自算活/死代码并双向断言, 最后核对成员访问次数与 16 个对外符号的出现次数。差分测试: 冻结基准与改写后各跑 n=20000, 都是 **118120 用例 / 0 处不一致**, 且七个入口的分相计数完全相同。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_profibus/README.md。

- 必须守住的不变量:

  - 本模块定义的 16 个外部符号 (7 函数 ProfibusCommErrCheck / ProfibusRcvDataDeal / ProfibusSendDataDeal / ProfibusStartDeal / UpdateProfibusCommFormat / getDPControlCmd / setDPStatusInfo, 9 数据 DPDataNum / DPRcvTest / dpControlCmd / dpControlCmdBak / dpFrqAim / dpStatus / rcvRigthFlag / saveEepromFlag / saveEepromIndex) 与引用的 23 个外部符号 (2 函数 + 21 数据, 另有编译器自造的 U$$MOD) 全部不变; 目标端目标文件的外部符号集合 39 个逐一相同。**本模块与 f_canlink 的最大差异就在这里: 它做了函数拆分** —— 提取了一个 `static void DpLatchEepromIndex(void)`。`static` 在 COFF 里是 C_STAT = 3 的**局部符号**, 不进外部符号集合 (nm2000 的外部符号清单里没有它), 所以不违反不变式; 本模块原本**零 static**, 拆分也不会引入任何 `$N` 后缀去重符号。f_canlink 的不变式明文禁止拆分, 本模块允许 —— 两个用例在这点上不可互相套用
  - 目标端告警集合**逐条相同**。这里踩过一次坑: ref 侧本来就有一条 `variable "pzdDataSaveCmd" was set but never used`。因为 TI 的告警**文本里带变量名**, 把这个死存储改名成更好看的 pzdCommandWord 之后, 同一条告警就变成了『新增 1 条 + 消失 1 条』, target_check.py 的『不新增告警』门当场判**不通过** (实测)。所以 `pzdDataSaveCmd` **有意保留原名** —— 这个不好看的名字是刻意的, 不是漏改。反过来, 因 PZD 循环去临时量而不再被使用的 `group, grade` **必须删掉声明**, 否则同样会新增一条 unused 告警而判 FAIL。教训: 改名的自由度受『编译器诊断文本』约束, 不是想改就能改
  - 死代码的**内容行**逐字节不变, 共 5 段 (非空 48 行 / 含空行 55 行; 以冻结基准 1-based 行号计): 第 44 行 (`#if 0` 掉的位域成员 `Uint16 DpCmdEnable:1;`) / 第 118~121 行 (`#if 0` 掉的空 else 分支) / 第 200~241 行 (`#if 0` 掉的 getDPControlCmd 整个函数体, 42 行) / 第 461 行 (`#if 1` 的 `#else` 支) / 第 466~472 行 (`#elif 1` 支的空桩, 7 行)。**冻结对象是内容行, 不含 `#if` / `#else` / `#elif` / `#endif` 这类指令行**; 核对由 build/_rewrite_profibus.py 在模型层 (保护行未被消费) 与 输出层 (整段文本仍在) 双向断言
  - 制表符: 原文件 33 行用制表符共 48 个, 活代码那 47 个全部展开成 4 空格, 死代码第 461 行 (`\treturn 0;`) 那 1 个**原样保留**并逐字节未动 —— 所以『输出零制表符』这个断言在本模块不成立, 脚本的断言改成『输出制表符总数 == 冻结死代码行的制表符总数 == 1』。交付文件零非 ASCII、CRLF, 由 verify_tree.py 强制
  - 魔数具名只是把字面量换成同值宏, 18 个文件作用域宏逐条对应: DP_DATA_NUM_OFFSET 4 (两处 `DPDataNum[commProtocol] + 4`) / DP_TEST_FRAME_LEN 5 (三处 `rcvNumMax = 5`) / DP_TEST_PROTO_REPLY 5 (连通测试回帧的协议号) / DP_PROTO_REPLY_BASE 1 / DP_PROTO_REPLY_MAX PPO5 (原文 `> 3`) / DP_PZD_HEADER_LEN 16 (`sendNumMax - 16`) / DP_PZD_DATA_OFFSET 10 / DP_PPO3_DATA_OFFSET 2 / DP_PZD_READ_SLOT 10 (`fe[i + 10]`) / DP_RESP_FUNC_CODE 0x10 / DP_RESP_ERR_CODE 0x70 / DP_ADDR_HIGH_NIBBLE_MASK 0xF000 / DP_ADDR_GROUP_MASK 0x0F / DP_ADDR_GRADE_MASK 0xFF / DP_GROUP_BASE_A0 0xA000 / DP_GROUP_BASE_B0 0xB000 / BYTE_MASK_LOW 0x00FF / FUNC_GROUP_SCALE 100。注意 DP_PZD_DATA_OFFSET 与 DP_PZD_READ_SLOT **同值不同义** (前者是回帧里 PZD 数据的起始下标, 后者是读功能码相对写功能码的槽位偏移), 没有合并成一个宏。令牌总数 1822 -> 1886, 数字如实记录, 不承诺『只增不减』
  - 变量改名逐条核对**成员访问**不变 (35 组全部通过)。这里又踩了一次『名字既是局部量又是结构成员』的坑: EEPROM 段被遮蔽的局部量 `group` 与 `funcCode.group` 同名, 直接全局替换会把 `funcCode.group.fe[i]` 也改掉; 修法是给那条加 `(?<![.>])` 前视排除
  - DpLatchEepromIndex 的提取保持语义与可达性: 原代码只在 `if (saveEepromFlag)` 为真时进入那个嵌套块, 提取后也只在同一条件下调用; 两级条件 `(saveEepromIndex) && (eepromIndex != saveEepromIndex)` 与『先 SaveOneFuncCode(旧值)、再 saveEepromIndex = 新值』的次序逐条保留; 原嵌套块里被遮蔽的局部量 group/grade/index/highH 改名成 eepromGroup/eepromGrade/eepromIndex/addrNibble, 它们在原作用域外不可见, 改名不改变任何外部可见状态
  - PZD 循环去临时量不改变求值: 原文 `group = fe[i]/100; grade = fe[i]%100; indexWrite = GetGradeIndex(group, grade);` 再对 `fe[i + 10]` 重复一遍, 两条语句互相独立; 改写成 `writeFuncIndex = GetGradeIndex(fe[i]/100, fe[i]%100);` 与 `readFuncIndex = GetGradeIndex(fe[i+10]/100, fe[i+10]%100);` 之后, 赋值次序 (先 write 后 read) 与各实参取值都不变 —— 两个实参之间无副作用, 求值顺序无关紧要
  - `pzdDataSaveCmd = (PZDHighCmd << 8) + PZDlowCmd;` 是个**死存储** (赋值后从未被读), 但它**原样保留**了 —— 它唯一的对外痕迹就是那条编译器告警, 而那条告警是 ref/new 可比对的证据; 删掉它会变成『消失一条告警』, 虽然不违反门 (门只查『新增』), 但会让两侧的告警集合不再一致。负向对照也据此把它**排除**在变异靶子之外 (扰动它必然 MISSED)
  - 三个真实越界读 **有意不改**: (a) PPO5 正常帧 36 字使 rcvFrame[35] 越过真数组一个词 / (b) commProtocol >= 4 时 DPDataNum[commProtocol] 越过 4 项表 / (c) pzdNum = (sendNumMax - 16) >> 1 在 sendNumMax < 16 时下溢。改它们是**行为变更**, 不是改写, 故保持原样; driver 用 precheck() 把这三条挡在用例之外 并记录在案 (cases/f_profibus/README.md 第 7 节)
  - 顺带纠正一个容易搞反的事实: `commProtocol` **不是** PPO 选择器。它是 f_comm.c:1100 用 GetNumberDigit1 (f_main.c:855 的 `* 52429 >> 19` 十进制分解) 从 funcCode.code.commProtocol 里按 commType 取出的**十进制位**, 自然范围 **0..9**; 真正给出 0..3 的是那四个 PPO 选择器 (PPO1..PPO5)。所以 (b) 不是『协议号越界』, 而是『十进制位越界』
  - 目标端 .text 尺寸 676 -> 684 字 (差 +8, build_and_compare.py 口径: **关优化**编译, 灵敏度最高)。这 +8 来自 DpLatchEepromIndex 的函数拆分 —— 多一次 CALL 与一份帧开销, 换掉的是原来的内联代码; 两侧告警集合相同、外部符号集合相同

| 项 | 结果 |
|---|---|
| 差分用例数 | **118,120** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_DPDataNum`, `_DPRcvTest`, `_ModifyFunccodeEnter`, `_ModifyFunccodeUpDown`, `_ProfibusCommErrCheck`, `_ProfibusRcvDataDeal`, `_ProfibusSendDataDeal`, `_ProfibusStartDeal`, `_UpdateProfibusCommFormat`, `_commProtocol`, `_commRcvData`, `_commReadData`, `_commRunCmd`, `_commSendData`, `_commTicker`, `_dpControlCmd`, `_dpControlCmdBak`, `_dpFrqAim`, `_dpStatus`, `_errorCode`, `_frq`, `_frqAim`, `_frqFdb`, `_frqRunDisp`, `_funcCode`, `_funcCodeGradeSum`, `_funcCodeOneWriteIndex`, `_funcCodeOneWriteNum`, `_getDPControlCmd`, `_maxFrq`, `_motorFc`, `_rcvFrame`, `_rcvRigthFlag`, `_runFlag`, `_saveEepromFlag`, `_saveEepromIndex`, `_sciFlag`, `_sendFrame`, `_setDPStatusInfo` |
| `.text` 尺寸 | 676 → 684 (**+8**) |

### `src/motor/MotorDataExchange.c`

- 变换: 电机层 <-> 功能层之间的参数搬运层 (268 行, 5 个对外函数 + 7 个对外数据, .text 由 build_and_compare.py 实测)。五个入口里每一条语句都是 `lhs = rhs;`, 没有分支、没有算术、没有硬件访问。改写手法: (1) 魔数具名 —— 7 个 `NUM_*` 宏换成带方向的 `F2M_*` / `M2F_*`, 并把原名 `NUM_2ms_FUNC_TO_MOTOR` 被当成『字数』与『第二段起始下标』两个角色用的地方拆成 `..._WORDS` 与 `..._BASE`, 且 `_BASE` 定义成**引用** `_WORDS` 而不是再写一个字面量, 两处不会再各自漂移; (2) 结构改写 —— ParSend2Ms 的第二段 30 条与 ParGet2Ms 的调试段 40 条, 原来每条都写`数组[基址宏 + k]`, 改成函数开头取一个局部指针 (`int *ext` / `int *dbg`) 后写 `p[k]`, 只改访问写法, 下标与目标字段完全不变; (3) 排版统一 —— 202 行里的制表符展开成 4 空格。改写由 build/_rewrite_mde.py 按一张替换表生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再断言源码里没有 `#if` (本模块没有条件编译, 整份文件都参与编译), 最后断言输出零制表符、纯 ASCII、无注释、无旧宏名残留。差分测试: 冻结基准与改写后各跑 n=20000, 用例数与不一致数见 docs/_difftest/MotorDataExchange.json。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/MotorDataExchange/README.md。

- 必须守住的不变量:

  - 本模块定义的 12 个外部符号 (5 函数 ParSend05Ms / ParGet05Ms / ParSend2Ms / ParGet2Ms / ParSendTune, 7 数据 gSendToMotor05MsDataBuff / gSendToMotor2MsDataBuff / gSendToFunctionDataBuff / gRealTimeToFunctionDataBuff / gParaIdToFunctionDataBuff / uReservedData / gSoftVersion) 全部不变; 目标端目标文件的外部符号集合逐一相同。**没有做任何函数拆分**, 也没有引入任何 static —— 所以也不会有 C_STAT 局部符号
  - 引用的 49 个外部全局量一个不多一个不少 (清单来自 `nm2000 build/Debug/MotorDataExchange.obj` 的 U 集合, 不是人眼数的)。这 49 个全是数据, **模块不调用任何外部函数** —— 目标文件未定义符号里没有一个 T/函数类符号。mock 与 driver 共用 shim/mock_mde.h 里那张 X-macro 表, 不会各自漂移。源码里声明的 `gABFluxS_comp` 从未被引用, 因此不在未定义符号表里, 也不进这张表; `gABFluxS_LowFilter` 相反 —— 它在源码第 23 行自己声明 (没走头文件), 所以 mock 与 driver 都各自声明一次
  - 7 个宏换名后**取值逐一不变**: F2M_2MS_MAIN_WORDS 75 (= 原 NUM_2ms_FUNC_TO_MOTOR) / F2M_2MS_DEBUG_WORDS 40 / F2M_2MS_DEBUG_BASE == F2M_2MS_MAIN_WORDS / M2F_2MS_MAIN_WORDS 30 / M2F_2MS_EXT_WORDS 30 / M2F_2MS_EXT_BASE == M2F_2MS_MAIN_WORDS / F2M_05MS_WORDS 14 / M2F_05MS_WORDS 5 / M2F_TUNE_WORDS 20。五个缓冲区的长度算式逐个不变: 14 / 75+40=115 / 30+30=60 / 5 / 20。**这是本模块最要紧的一条**: 长度算式一变, 越界与否就变了
  - `_BASE` 拆成**引用** `_WORDS` 而不是重复字面量, 是为了让『调试段基址 == 主段字数』这个耦合关系显式化。原来两个角色共用一个名字, 改一处就同时改了长度算式与基址 —— 那正是最危险的一类隐性耦合。换成引用后, 改动 `_WORDS` 会同时正确地移动两处; 而如果当初写成两个独立的字面量 75, 将来改一个忘一个就会静默错位
  - 结构改写只改**访问写法**: `gSendToFunctionDataBuff[M2F_2MS_EXT_BASE + k]` 与 `ext[k]` 指向同一处 (`ext = &gSendToFunctionDataBuff[M2F_2MS_EXT_BASE]`), `gSendToMotor2MsDataBuff[F2M_2MS_DEBUG_BASE + k]` 与 `dbg[k]` 同理; 30 条与 40 条的下标集合分别是 0..29 与 0..39, 逐条未变, 两个局部指针都是**函数内自动变量**, 不进符号表。生成器对这两段的替换做了命中计数断言 (30 / 40), 少一条多一条都会中止
  - 赋值语句的**条数与顺序**逐条未变: ParGet05Ms 14 条 / ParSend2Ms 60 条 / ParGet2Ms 115 条 / ParSendTune 20 条 / ParSend05Ms 0 条。顺序有意义 —— 例如 ParGet05Ms 写 gMainCmd.FreqSet, 而 ParSend2Ms 会读它 —— 所以生成器只做等值替换, 不做任何重排
  - ParSend2Ms 里 `gFVCSpeed.SpeedEncoder` 被**用了两次** (第 24 条写 `gSendToFunctionDataBuff[23]`, 第 42 条写 `gSendToFunctionDataBuff[M2F_2MS_EXT_BASE + 11]` 即 `[41]`), 所以 60 条语句只对应 59 个不同的源。这不是抄错, 是原工程本来的写法; 于是『这两格永远同值』本身成了要保住的行为 —— 改写若只动其中一条的源, 差分测试会当场抓到。其余四个入口的语句数与去重后的源个数相等 (0/0, 14/14, 115/115, 20/20)
  - 本模块**没有条件编译**: 源码里 0 个 `#if` / `#ifdef` / `#if 0`, 整份 268 行都参与编译。所以既没有死代码区间要保护 (不需要 f_frqSrc_pid 那种条件栈), 也不需要 `live_span` —— 负向对照的 107 条锚点只要在全文唯一, 位置就没有歧义
  - 制表符: 基线 202 行含制表符, 全部用于等号后的对齐; 改写时按 4 空格展开, 展开后 0 个制表符。交付文件零非 ASCII、CRLF, 由 verify_tree.py 强制。注意展开后行数不变、令牌流只少不增 (制表符不是令牌)
  - `ParSend05Ms` 的函数体是**空的**, 且**有意保持为空**。它唯一的对外痕迹是『调用它什么都不变』这个不变量; 差分测试里由 phase_empty 专门钉住 —— 改写要是『顺手』把它填上了, 那里会失败。空函数体不生成任何代码, `nm2000` 里它仍是一个地址等于下一条符号的 T 符号 (1 个字的 LRETR)
  - `gRealTimeToFunctionDataBuff` 被声明、被定义、**没有任何函数引用它**。它是本模块里唯一一个纯死导出: 差分测试把它放进快照 (有变异开始写它会被抓到), 但除此之外它区分不了 ref 与 new。**有意保留不删** —— 删掉它就是行为变更 (改变对外符号集合), 不是改写
  - 宿主端的宽度口径必须在案: 本用例用**项目自己的头文件**, 所以 `int` 在宿主上仍是 32 位、`Uint` (来自 MotorDefine.h) 也仍是 32 位; 只有 Uint16/int16/int32/Uint32 被 cases/MotorDataExchange/shim/DSP2803x_Device.h 钉到精确宽度。对差分测试这没问题 (两侧编译方式相同, 每个截断点都一样), 但它**不**许可改写在宿主上改变宽度 —— 那种等价宿主看不见。本次改写只有改名、宏具名、局部指针与空白, 没有任何表达式宽度移动
  - 有意不测的既有缺陷: 无。本模块没有越界读、没有隐式声明、没有未初始化读, 49 个外部量与 5 个缓冲区之间的下标全部在界内 (最大下标 114 对应长度 115, 最大下标 59 对应长度 60, 最大 19 对应 20, 最大 13 对应 14)

| 项 | 结果 |
|---|---|
| 差分用例数 | **25,233** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ParGet05Ms`, `_ParGet2Ms`, `_ParSend05Ms`, `_ParSend2Ms`, `_ParSendTune`, `_gABFluxS_LowFilter`, `_gADC`, `_gAI`, `_gBasePar`, `_gComPar`, `_gCpuTime`, `_gDeadBand`, `_gError`, `_gEstBemf`, `_gExtendCmd`, `_gFVCSpeed`, `_gFluxR`, `_gFluxWeak`, `_gGetParVarable`, `_gHVfOscIndex`, `_gIAmpTheta`, `_gIMTQ12`, `_gIMTQ12_obs`, `_gIMTSetQ12`, `_gIPMPos`, `_gIPMPosCheck`, `_gIUVWQ12`, `_gInvInfo`, `_gLineCur`, `_gLoadLose`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtInfo`, `_gMotorExtPer`, `_gMotorExtReg`, `_gMotorInfo`, `_gOutVolt`, `_gPGData`, `_gPMInitPos`, `_gPWM`, `_gParaIdToFunctionDataBuff`, `_gPhase`, `_gPmParEst`, `_gPowerTrq`, `_gRatio`, `_gRealTimeToFunctionDataBuff`, `_gRotorTrans`, `_gSendToFunctionDataBuff`, `_gSendToMotor05MsDataBuff`, `_gSendToMotor2MsDataBuff`, `_gSoftVersion`, `_gSubCommand`, `_gTemperature`, `_gTestDataReceive`, `_gUDC`, `_gUMTSet`, `_gUVCoff`, `_gUVWPG`, `_gVCPar`, `_gVFPar`, `_uReservedData` |
| `.text` 尺寸 | 1259 → 1218 (**-41**) |

### `src/function/f_fcDeal.c`

- 变换: 功能码 (参数) 的初始化 / 清记录 / 恢复出厂 / 掉电保持 / 互限夹取 (419 行, 6 个函数 + 3 个导出数据)。改写手法: (1) 魔数具名 —— `2 - 1`(夹取遍数)、`0xFFFF0000UL`/`0x0000FFFFUL`/`>> 16`(掉电保持拆 plcTime 高低字)、`0x222`(清记录时 errorFrqUnit 的定值)、`6`(清记录单点个数)、`1140`(T1140 机型额定电压)、`0`/`1`(SVC 模式禁止/允许) 全部换成带名字的宏; (2) 判据抽函数 —— `GetFuncCodeInit` 里 5 个判据各自抽成 static 谓词 IsInitExceptSeries / IsInitExceptSingle / IsCalibrateCurveCode / IsAccDecTimeCode, 取值那一层抽成 GetCalibrateCurveInit, 主链从『两段 30 行的布尔表达式』变成『一行一个谓词』; (3) 结构改写 —— ClearRecordDeal 的 12 行手抄改成两张 const 表 + 一个 6 次循环, LimitOtherCodeDeal 的 60 行内层循环体抽成 static Uint16 LimitOneCode(Uint16), 外层只剩『扫描表 + 两遍 + 遍历清单』; (4) 局部量改名 i/j/k -> entry/scan/pass、init -> value、tmp -> clamped; (5) 排版 —— `AO_NUMBER-1`/`REM_NUM-1` 补空格, 3 行制表符展开成 4 空格。改写由 build/_rewrite_fcdeal.py 按一张 8 条替换表生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再断言两块死代码与全部 9 处条件编译栈改写后仍在, 最后断言输出零制表符、纯 ASCII、无注释、6 个导出符号齐全、括号配平。差分测试: 冻结基准与改写后各跑 n=20000, 用例数与不一致数见 docs/_difftest/f_fcDeal.json。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_fcDeal/README.md。

- 必须守住的不变量:

  - 本模块定义的 9 个外部符号 (6 函数 GetFuncCodeInit / GetFuncCodeInitOriginal / ClearRecordDeal / RestoreCompanyParaOtherDeal / PowerOffRemDeal / LimitOtherCodeDeal, 3 数据 limitedByOtherCodeIndex / limitedByOtherCodeIndexNum / limitOtherCodeIndex) 全部不变; 目标端目标文件的外部符号集合逐一相同 (31 个, 含 22 个未定义)。**`limitOtherCodeIndex` 必须是外部符号**: `f_debug.h` 第 27 行的 `#if 1` 那一支把 `LOCALF` 定义成**空**, 所以 `LOCALF Uint16 const limitOtherCodeIndex[]` 在目标端是大写 `R` 的外部符号, 不是 file-local。**这是本模块最容易被抄错的一条**
  - 本模块**新增了 6 个 static 函数与 2 个 static 数据对象**, 全部是 COFF 的**局部**符号 (nm2000 里小写 t/d), 不进外部符号集合 —— 与 f_profibus 提取 DpLatchEepromIndex 是同一条纪律。6 个函数: IsInitExceptSeries / IsInitExceptSingle / IsCalibrateCurveCode / GetCalibrateCurveInit / IsAccDecTimeCode / LimitOneCode; 2 个数据: `_$P$T0$1` / `_$P$T1$2` (ClearRecordDeal 里那两张 6 字 const 表, 编译器自动命名, 落在 `.econst`)。目标端 nm2000 实测: 外部符号 31 个 (9 定义 + 22 未定义) 与基准逐一相同, 新增的全是小写 t/d 局部符号。C28x 上这些 static 函数**不内联**, 所以 `.text` 变长 (关优化 768 -> 799); 按工程真实选项编译时 -o3 反而更短 (635 -> 625 字), -o2 下也有差异 —— o2_check.py 因此记『不同』, 这是『允许更深的改写』下预期内的代价。`.econst` 的两个新子段由 F28035.cmd 的 `.econst : > FLASHA` 一并放置, 不需要改链接脚本
  - 引用的 22 个外部符号一个不多一个不少: 2 个函数 (MenuModeDeal / TorqueBoostDeal, mock 里各有一个桩) + 20 个数据 (funcCode / funcCodeOneWriteIndex / funcCodeOneWriteNum / funcCodeRwModeTmp / startIndexWriteSeries / endIndexWriteSeries / invPara / frqFlag / runCmd / upDownFrq / plcStep / plcTime / pmsmRotorPos / saveEepromIndex / runDirPanelOld / invTypeLimitTable / ovVoltageInitValue / pInvTypeParaTable / funcCodeAttribute / gSendToFunctionDataBuff)。清单来自 `nm2000 build/Debug/f_fcDeal.obj` 的 U 集合, 不是人眼数的; mock 与 driver 共用 shim/mock_fcdeal.h 里那组声明, 不会各自漂移
  - `GetFuncCodeInit` 的**判据链顺序与短路语义逐条不变**: series -> single -> 曲线区间 -> 加减速时间 -> (逆变器相关 5 支) -> 兜底。前两支原来都写 `init = funcCode.all[index];`, 合并成 `if (IsInitExceptSeries(...) || IsInitExceptSingle(...))` 一支 —— 合并合法的前提是两支**都没有副作用**且短路顺序不变 (谓词按原顺序求值)。曲线的三层嵌套拆成`IsCalibrateCurveCode`(外层) + `GetCalibrateCurveInit`(内层), 内层的`ai[2]` 区间 -> `ao[1]` 区间 -> `group.ae[...]` 兜底顺序不变
  - `GetCodeIndex` 展开成 `(Uint16)(&(x) - &(funcCode.all[0]))`, 是**联合体成员之间的指针差**。所以本用例必须用真头文件 (real.txt = `*`), 抄一份结构体等于在测另一份偏移。整个改写**没有动任何 `GetCodeIndex(...)` 的实参**, 只动了外层判据的写法; `limitOtherCodeIndex[]` 的 13 个成员与它们的顺序逐字未变
  - 写队列是唯一的副作用通道: `SaveOneFuncCode(x)` 展开成 `funcCodeOneWriteIndex[funcCodeOneWriteNum++] = (x)`。driver 逐条比对`funcCodeOneWriteNum` 与整个 `funcCodeOneWriteIndex[]`, 所以『夹取时写了几次、写的是哪个下标、按什么顺序』全在观测集合里。`LimitOneCode` 抽函数时 3 处`limitedByOtherCodeIndex[i]` 与 1 处 `pAttribute->lower` 的实参逐一对应搬进形参 `codeIndex`, 没有换过任何一处
  - `LimitOtherCodeDeal` 的**外层两遍循环 (原 `k = 2 - 1`) 与 `break` 都保留**: 遍数换成具名宏 `LIMIT_PASS_NUM`(值仍是 2), `break` 原样在。这两者的可观测性有专门构造: `phase_limit_chain` 摆出一个『第一遍把 all[t] 抬出上下限之外、第二遍才夹回来』的场景, 构造完当场断言 `S[0].qNum > 2` —— 证明这个构造确实制造出了两遍差异, 而不是碰巧。至于 `break`, 13 个值互不相同, 循环最多命中一次, 删掉它不可观测 (所以 F110 改用步长 2, 跳过 6 个奇数位成员)
  - `funcCodeAttribute[]` 的替身表长度**恰好 909 项**, 等于真表长度 (实测等于 `FNUM_PARA`)。做成 1024 项『更安全』是错的 —— `LimitOtherCodeDeal` 读这张表没有边界检查, 表变长就等于把『读到 909 之外』这件事在宿主上藏起来。生成器 build/_gen_fcd_attr.py 与 shim/mock_fcdeal.h 由一条编译期断言绑死: `typedef char fcd_attr_table_len_must_equal_FNUM_PARA[(ATTR_TABLE_N == FCD_ATTR_N) ? 1 : -1];`。表里 8 种 (upperLimit, lowerLimit, signal) 组合轮转出现, driver 的 check_invariants() 断言 8 种全都非空 (实测 114 114 114 114 114 113 113 113)
  - `limitOtherCodeIndex[]` 的宿主补丁 (cases/f_fcDeal/host_patch.json) 只把静态初始化式换成不带初始化式的声明 (16 行 -> 1 行, count=1)。原因是这 13 个初值都是`GetCodeIndex(funcCode.code.<成员>)`, 即**两个地址常量相减**, MSVC 报 error C2099 (与 f_invPara / f_motorFc 同一个坑)。值由 driver 用**同一批宏**在跑之前填, 不是另写一份公式。表的**排列**不可观测 —— 模块只做 `if (limitOtherCodeIndex[j] == index) { ...; break; }`, 13 个值互不相同 (driver 的 check_limit_table() 断言), 命中唯一; 表**内容**的等价由目标端 build_and_compare.py 的 Tier B 逐段比对 .econst 证明
  - 两块死代码逐字节不变: 第 16~18 行 `#if DEBUG_F_POSITION_CTRL` 里的 `#pragma DATA_SECTION(limitedByOtherCodeIndex, "data_ram")`, 与 `GetFuncCodeInitOriginal` 里第 390~403 行的 `#if 0` 块。生成器的 DEAD_SNIPPETS 在改前改后各断言一次『出现 1 次』。**正因为要保住那段 `#if 0`, `GetFuncCodeInitOriginal` 整体没有改写** —— 死块里写的是 `init = EEPROM_CHECK1;` 等, 把活代码的 init 改名成 value 会把死块一起带走, 也就失去了『死块没被碰过』这个证据。代价是两个函数里同一角色一个叫 init 一个叫 value, 这是**有意**的不一致, 记在 README 第 1 节
  - 9 处条件编译栈全部原样保留: `#if F_DEBUG_RAM`/`#elif 1`、`#if DEBUG_F_POSITION_CTRL`、`#if DEBUG_F_LIMIT_OTHER_CODE`、`#if DEBUG_F_CLEAR_RECORD`、`#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL`、`#if (DEBUG_F_INV_TYPE_RELATE || DEBUG_F_MOTOR_POWER_RELATE)`、`#if !F_DEBUG_RAM`、`#if DEBUG_F_MOTOR_FUNCCODE`、`#if 0`。**没有把 `#if F_DEBUG_RAM / #elif 1 / #endif` 塌成一行 `#define`** —— 那是本工程全局的 RAM/Flash 配置开关 (f_debug.h 由 `FUNC_DEBUG_RAM` 控制), 拆掉它就等于在另一个配置下改了行为, 而差分测试只覆盖当前配置
  - 三条既有越界路径**原样保留, 不主动走到界外也不假装不存在**: (a) 写队列 `funcCodeOneWriteIndex[200]` 无上界, 而 LimitOtherCodeDeal 最坏`2 遍 × 150 条 × 2 次 = 600` 写 —— 差分测试里队列长度**就用真实的 200**, driver 把 limitedByOtherCodeIndexNum 钉在 LIM_N_MAX=16 并断言 4*16 <= 200; (b) `funcCodeAttribute[]` 索引无检查; (c) `GetInvParaPointer()` 没有下界检查, driver 只喂合法的 (volLevel, type) 组合, 六张机型表放在 fcdInvParaPool[6][64] (最长 30, stride 取 64)
  - `limitOtherCodeIndex` 的**声明**在目标端仍是 `const` (`LOCALF Uint16 const`), 只有宿主补丁里那一份去掉了 const (因为要运行期填)。模块只读它, 语义不变。改写后 `.text` 由 target_check.py 实测 768 -> 799 字 (+31): 多了 5 个 static 谓词函数, C28x 上不内联。差分测试证明的是**行为**等价; 机器码变长是『允许更深的改写』下预期内的代价
  - 宿主端的宽度口径必须在案: 本用例用**项目自己的头文件** (real.txt = `*`), 所以 `int` 在宿主上仍是 32 位; 只有 int16/int32/Uint16/Uint32 被 shim/DSP28x_Project.h 与公共 shim 的 DataTypeDef.h (PINNED_SHIMS, 不转发) 钉到精确宽度。`DataTypeDef.h` 必须钉住 —— 真文件把 `u16` 定义成 `unsigned int`, 宿主上是 32 位, 转发进去整个测试台的宽度前提就没了
  - 有意不测的东西: 死代码两段 (不参与编译, 打上变异必然 MISSED, 那是测试台的正确行为, 不是洞); `#if 0` 那三个 else-if 分支 (同上)。**没有**其它有意不测项 —— 全文活代码 419 行里除上述两段外都被 38923 个用例覆盖

| 项 | 结果 |
|---|---|
| 差分用例数 | **38,923** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ClearRecordDeal`, `_GetFuncCodeInit`, `_GetFuncCodeInitOriginal`, `_LimitOtherCodeDeal`, `_MenuModeDeal`, `_PowerOffRemDeal`, `_RestoreCompanyParaOtherDeal`, `_TorqueBoostDeal`, `_endIndexWriteSeries`, `_frqFlag`, `_funcCode`, `_funcCodeAttribute`, `_funcCodeOneWriteIndex`, `_funcCodeOneWriteNum`, `_funcCodeRwModeTmp`, `_gSendToFunctionDataBuff`, `_invPara`, `_invTypeLimitTable`, `_limitOtherCodeIndex`, `_limitedByOtherCodeIndex`, `_limitedByOtherCodeIndexNum`, `_ovVoltageInitValue`, `_pInvTypeParaTable`, `_plcStep`, `_plcTime`, `_pmsmRotorPos`, `_runCmd`, `_runDirPanelOld`, `_saveEepromIndex`, `_startIndexWriteSeries`, `_upDownFrq` |
| `.text` 尺寸 | 768 → 799 (**+31**) |

### `src/function/f_error.c`

- 变换: 故障管理: 欠压检测与 EEPROM 落盘握手、故障码择优与等级仲裁、故障现场保存、故障复位状态机、自动复位 (真值判据 + 功能码间隔判据) 与故障等级查表 (889 行, 13 个函数 + 18 个导出数据 + 4 个文件内 static)。改写手法: (1) 模块私有常量集中到文件头部一块 —— 原来 3 个 define 散在 GetNewError 之前、`ERROR_SCENE_NUMBER` 藏在 ErrorSceneSave 里; 并给 13 个魔数具名 (16/4/0xF 是电机侧位图的切分, 4 是无故障时的等级, 10 是自动复位清零等待的倍数, 5 是 OC 类自动复位的最小间隔, 100 是 devChk/osChk 的时间单位, 0x0FFF 与 <<4 是 errorFrqUnit 的打包); (2) 抽 static 助手 5 个 —— `UpdateError` 的三层嵌套 + 空语句占位改成『提前 return + RollbackError()』, `GetNewError` 里 6 处『取属性 -> 比等级 -> 换故障码』抽成 `ConsiderError(code, &errorLevel)`, OC/OV 的按加减速分档抽成 `ClassifySpeedFault`, `ErrorSceneSave` 的 8 个现场字段写入抽成 `SaveErrorScene()`, `GetErrorAttribute` 的『取数字位 -> 加 1』抽成 `GetActionLevel(group, index)`; (3) 结构改写 —— `getMotorError` 的 `if(...){for...} else {...}` 改成全零提前 return + 循环内 continue 守卫, 三个自动复位函数与 `ErrorSceneSave`/`GetNewErrorAttribute` 改成提前 return 风格; (4) 局部量改名 —— 4 个文件内 static (devTcnt/osTcnt/motorSnOld/diFuncErrRstOld) 改成 devChkTicker/osChkTicker/motorSnPrev/errorResetInOld, `errAutoRstSpaceTime`(局部) 改成 spaceTime, 删掉从未使用的 `int16 i;`; (5) 排版 —— 59 行制表符按 4 列制表位展开, 去掉 `if ( x )` 式空格与 ` #endif` 前的空格。改写由 build/_rewrite_ferror.py 按一张 21 条替换表生成 (每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再断言五块死代码与全部 7 处条件编译栈改写后仍在, 最后断言输出零制表符、纯 ASCII、无注释、31 个导出符号齐全、旧标识符清干净、括号配平。差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数与 16 个分阶段计数逐一相同, 见 docs/_difftest/f_error.json。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_error/README.md。

- 必须守住的不变量:

  - 本模块定义的 31 个外部符号 (13 函数 ErrorDeal / UpdateError / underVoltageDeal / getMotorError / GetNewError / GetNewErrorAttribute / ErrorSceneSave / ErrorReset / GetErrorResetDeal / AutoResetErrorDeal / AutoResetErrorTrueDeal / AutoResetErrorFcDeal / GetErrorAttribute, 18 数据 errorCode / errorCodeOld / errorInfo / errorInfoOld / errorInfoFromMotor / errorOther / errorOtherOld / errorAttribute / errorAttributeOld / errorEeprom / errAutoRstNum / errAutoRstClrWaitTicker / errorSpaceTicker / errorDealStatus / uvDealStatus / bUv / curTime / errorCodeFromMotorList) 全部不变。目标端 nm2000 实测: 目标文件外部符号 **64 个** (31 定义 + 33 未定义) 与基准逐一相同; 关优化下 `.text` 1227 -> 1177 字 (-50); **工程实际选项下不收敛** (o2_check.py, docs/_difftest/f_error.o2.json): Debug -o2 下 .text 0x455 -> 0x41e 字 (1109 -> 1054), Release -o3 下 0x461 -> 0x43a 字 (1121 -> 1082) —— 抽出来的 5 个 static 函数在 C28x 上不内联, 反汇编里因此多出 `_GetActionLevel` 这样的独立符号。这与 f_vf 相反 (f_vf 改写过却仍逐字节相同, 那是最强的结果); 交付契约是『没改写过的必须相同』, 不要求『改写过的一定不同』。**`LOCALF` 展开成空** 是本模块最容易抄错的一条: `f_debug.h` 第 27 行的 `#if 1` 那一支把 `LOCALF` 定义成空, 所以 `LOCALF enum ERROR_DEAL_STATUS errorDealStatus;` 等 5 处在大写符号表里都是**外部**符号 (T/D), 不是 file-local —— 这也正是 new_case.py 的 31 个改名里有它们的原因
  - 本模块**新增了 5 个 static 函数**, 全部是 COFF 的**局部**符号 (nm2000 里小写 t), 不进外部符号集合 —— 与 f_profibus 提取 DpLatchEepromIndex、f_fcDeal 提取 5 个谓词是同一条纪律: ClassifySpeedFault / ConsiderError / RollbackError / SaveErrorScene / GetActionLevel。**这 5 个绝不能写成 `LOCALF`**: LOCALF 展开成空, 那样会多出 5 个外部符号, target_check.py 的 `symbols_equal` 当场判失败。生成器里有一条断言逐个检查 `static Uint16 <name>(` / `static void <name>(`
  - 4 个**函数内 static** 改了名 (devTcnt -> devChkTicker, osTcnt -> osChkTicker, motorSnOld -> motorSnPrev, diFuncErrRstOld -> errorResetInOld)。它们在目标文件里是 C_STAT 局部符号 (nm2000 里带 `$序号`), 不影响外部符号集合; target_check.py 的 `real_syms()` 按 storage class == 2 (C_EXT) 过滤, 所以改名是**预期**结果, 判据比『名字里带 `$` 就排除』更严 —— 若改写误把 static 改成外部链接, 符号会以 C_EXT 出现, 照样被抓到
  - **`sizeof(errorCodeFromMotorList)` 原样保留, 刻意不改成可移植写法。** 它出现在 getMotorError 的循环上界: C28x 的 sizeof 返回 16 位字数, 19/1 = 19 正好是项数; 宿主 MSVC 返回字节数, 算出 38, 会跑到表外。宿主补丁 (cases/f_error/host_patch.json) 把它换成 `sizeof(errorCodeFromMotorList)/sizeof(Uint16)`, **side = both (默认)**。这里与 f_invPara 同一处补丁的 side 不同: f_invPara 的改写自己换成了可移植写法, 所以只补 ref 侧; 本模块的改写**只改结构不改表达式**, 两侧都还是原文, 所以要补两侧。若把改写也换成可移植写法, 补丁在新侧找不到锚点 -> count 失配 -> run_case.py 直接 SystemExit; 而且负向对照的变体是把**冻结基准**当 new 侧编译的, 那样 ref 侧补、new 侧不补, 反而制造出一个人为差异。所以**不动**是最正确的选择
  - 另一处 sizeof —— `funcCode.code.errorScene1.all[sizeof(struct ERROR_SCENE_STRUCT) - 1]` —— **同样刻意保留表达式原样**。union ERROR_SCENE 的数组长度本身就写成 `Uint16 all[sizeof(struct ERROR_SCENE_STRUCT)]`, 于是目标端取 all[8-1]=all[7], 宿主取 all[16-1]=all[15], 两边都是**数组尾元素**, 语义一致。反过来, 若改写成字面量 8, 宿主侧会变成 all[7] 而数组有 16 项 —— 那才是真正引入差异。driver 逐条比对 `endIndexWriteSeries`, 这条不是纸面论证而是被观测的
  - `GetErrorAttribute` 的**调用次数与实参不变**: 原实现每组判据里恰好调一次 `GetNumberDigit1(digit, funcCode.code.errorAction[k])`, 再用 `digit[...] + 1` 取值; 改写把它抽成 `GetActionLevel(group, index)`, 每组仍然恰好一次、参数仍是 `errorAction[group]`、数字位下标逐项对应 (组 0 两段 = errCode-ERROR_OL_MOTOR / errCode-ERROR_LOSE_PHASE_INPUT, 组 1 两段 = errCode-ERROR_ENCODER / errCode-ERROR_24+2, 组 2 = errCode-ERROR_USER_1, 组 3 = errCode-ERROR_DEV, 位置初始化 = 常量 2)。**这两项是 driver 的观测量** (mockDigitCalls / mockDigitLastArg), 不是『看起来一样』。全部 7 个下标在各自分支的值域内都是 0..4 (driver 的 check_invariants() 断言), 所以把 `int` 下标收进 `Uint16` 形参不改变行为
  - `UpdateError` 的三层嵌套回退链改成『提前 return』后**判据顺序与短路方向逐条对应**: 新等级为 NO_ERROR -> 回退; 旧等级为 NO_ERROR -> 不回退; `#if DEBUG_F_ERROR_LEVEL` 下新 < 旧 -> 不回退, 旧 == RUN -> 不回退; 其余 -> 回退。原实现里那些 `;` 空语句分支正是『不回退』, 改写把它们变成 `return`。这条等价性有专门覆盖: `phase_update_error` 用 8x5x8x5 = 1600 组 (旧码 x 旧等级 x 新码 x 新等级) 扫全
  - `AutoResetErrorFcDeal` 的**既有溢出缺陷原样保留且两侧都测**: `errorSpaceTicker` 是 Uint16, 而阈值 `errAutoRstSpaceTime * (TIME_UNIT_ERR_AUTO_RST_DELAY/RUN_CTRL_PERIOD)` = spaces * 50 可以超过 65535; 越界后计数器先回绕, `>=` 判据**永远不成立**, 自动复位失效。driver 取 spaces = {0, 1, 10, 1310, 1311, 2000}: 1310*50 = 65500 可达到, 1311*50 = 65550 不可达到。这是**保留**的既有缺陷, 不是改写引入的
  - `getMotorError` 有一处**既有 UB 刻意绕开而不是修掉**: `errorInfo` 是未初始化的局部量, 若 `errorsCodeFromMotor` 非零但没有哪个置位的故障码能赢过初始等级 (GetErrorAttribute(ERROR_NONE) = 4), 循环一次都不赋值, 末尾 `errorInfoFromMotor = errorInfo` 就读到垃圾。改写**逐字保留**这个形状 (提前 return 的那一支显式 `errorInfo = 0`, 与原文一致), driver 的 `set_err_bit(i)` 则**总是同时置第 18 位** (ERROR_UVW_FDB), 保证循环至少赋值一次。两条合起来: 缺陷在案, 但测试不靠运气
  - `GetErrorAttribute` 对 ERROR_NONE 返回 4, 而 `errorAttribute.bit.level` 只有 **3 位** (struct ERROR_ATTRIBUTE_BITS { reset:1; level:3; }, level 在 bit1~bit3), 截断本身在测试范围内。**level 不在 bit0**: `errorAttribute.all = 3` 的含义是 level = 1, 不是 3。driver 的每一处属性赋值都走 `set_attr(level, reset)` 助手, 并在 check_invariants() 里断言 `probe.bit.level = 5` 对应 `probe.all == (5 << 1)` —— 这是本用例最容易写错的一处
  - 五块死代码逐字节不变: (1) 第 15~18 行 `#if F_DEBUG_RAM` 的 0 号分支体 (注意 14 行的 `#if` 与 19 行的 `#elif 1` 本身是活指令, 死块只到 18 行); (2) 第 221~262 行 `#if 0` 的 getMotorErrorInfo(); (3) 第 320~325 行 getMotorError() 里的 `#if 0` else; (4) 第 869~880 行 GetErrorAttribute() 里 `#if 0` 的两条; (5) 第 885~889 行文件末尾 `#elif 1` 的空 ErrorDeal()。生成器的 DEAD_SNIPPETS 在改前改后各断言一次『出现 1 次』。第 (3) 块在改写后**语法上不再跟在 if 后面** (那一支已改成提前 return), 但它在 `#if 0` 里从不参与编译, 所以形状保持原样即可
  - 7 处条件编译栈全部原样保留: `#if F_DEBUG_RAM` / `#elif 1` / `#if defined(FUNC_DEBUG)` / `#if DEBUG_F_ERROR` / `#if DEBUG_F_ERROR_FROM_MOTOR` / `#if DEBUG_F_ERROR_LEVEL` / `#if DEBUG_F_PLC_CTRL`。**没有把 `#if F_DEBUG_RAM / #elif 1 / #endif` 塌成一行 `#define`** —— 那是本工程全局的 RAM/Flash 配置开关, 拆掉就等于在另一个配置下改了行为, 而差分测试只覆盖当前配置。当前配置下 DEBUG_F_ERROR = 1, DEBUG_F_ERROR_FROM_MOTOR = 1, DEBUG_F_ERROR_LEVEL = 1, DEBUG_F_PLC_CTRL = 1 (f_debug.h 第 24 行), F_DEBUG_RAM = 0 (FUNC_DEBUG_RAM 未定义), DEBUG_F_POSITION_CTRL = 0 (未定义 ZJ_MD380M), 且 `FUNC_DEBUG` **全工程无定义** (只有 `FUNC_DEBUG_RAM`), 所以走的是 `#else` 那一支
  - 本用例必须用真头文件 (real.txt = `*`)。理由: 模块的 include 链 f_error.h -> f_funcCode.h -> f_interface.h 把 funcCode / errorAction / errorScene1 / errorScene3 / errorLatest1 / errorLatest3 / frqPoint / errAutoRstNumMax / errAutoRstSpaceTime / ovPointSet / devChkValue / devChkTime / osChkValue / osChkTime / plcEnable / plcErrorCode 全部带进来, 而 driver 要读**联合体成员的偏移** (errorScene3.elem.errorCurrent 之类) 与位域布局 (diFunc.f1.bit.externalErrOpenIn 等 4 个 DI 位); 抄一份结构体等于在测另一份。只有 int16/int32/Uint16/Uint32 被 shim/DSP28x_Project.h 与公共 shim 的 DataTypeDef.h (PINNED_SHIMS, 不转发) 钉到精确宽度 —— `DataTypeDef.h` 必须钉住, 真文件把 `u16` 定义成 `unsigned int`, 宿主上是 32 位, 转发进去整个测试台的宽度前提就没了
  - 模块私有、头文件里没有的宏在 driver 里**各自镜像一份并注明来源**: `DEV_CHK_TIME_MIN 5` (原文第 335 行)、`ERROR_EXTERNAL_INFO_OPEN/CLOSE` (333~334 行)、`ERROR_SCENE_NUMBER 8` (567 行)、以及 `UV_DEAL_STATUS` 枚举 (45~50 行, **只在 .c 里, 不在任何头文件里**, driver 以 FERR_UV_STATUS 镜像同样的 0/1/2)。`errorCodeFromMotorList` 在 driver 里必须写成**完整数组类型** `const Uint16 [19]`, 写 `[]` 会让宿主的 sizeof 变成 0 (警告 C4034)
  - 两个外部被调函数都在 mock 里做成桩, 而不是把真模块链进来: `GetNumberDigit1` (声明在 f_main.h:66, 定义在 src/function/f_main.c:855 —— 写这条时该模块**尚未**改写, 此后 f_main 也进了改写清单, 这个 mock 仍是照抄其实现) 与 `PowerOffRemDeal` (f_fcDeal.c, 第 16 个改写模块)。前者是**逐字照抄** f_main.c:855 的实现, 因为 GetErrorAttribute 的返回值就是 `digit[k] + 1`, 数字位规则抄错等于在测别的函数; 后者只累加一个调用计数 (driver 观测 mockPowerOffCalls), 因为它唯一可观测的作用就是『被调了几次』
  - 有意不测的东西: 五块死代码 (不参与编译, 打上变异必然 MISSED, 那是测试台的正确行为, 不是洞)。**没有**其它有意不测项 —— 全文活代码 (889 行减去五块死代码)里除上述外都被 33762 个用例覆盖, 其中随机混合 20000 个。负向对照共 204 条变异 (编号 E001~E209, 其中 5 条已证明等价后删除, 见下), 首次全量跑出 9 条 MISSED, 全部定位到**测试台的盲区或可证等价**, 已逐条处理: (a) E031 原文是 `GetErrorAttribute(errorCode)` -> `GetErrorAttribute(ERROR_NONE)`, 但上一行刚写下 `errorCode = ERROR_NONE;`, 两个实参恒等 —— **可证的同义变异**, 任何测试都区分不了, 已改成改『初始故障码』本身 (全 0 位图那一支会把它透传给 errorCodeFromMotor); (b) E084/E089/E094 三条落在 devChk / osChk 的触发分支上, 而 driver 的 base_state() 给的 `errorAction[3] = 9876` 让 attr(ERROR_DEV) = 7、attr(ERROR_OS) = 8, 两个都比触发判据里的初始等级 4 差 —— 这一路**根本没被走到**。driver 里已把 devChk/osChk 两个阶段的 errorAction[3] 换成 21 (attr 变成 2/3), 并把 errorCode 残留值钉在等级同为 2 的 ERROR_USER_1 上, 让 E089 那条『errorLevel 被压成 FREE_STOP』也能在判据上劈开; (c) E038 (`<` 改成 `<=`) 需要两个**等级相等且都 >= 2** 的候选, 而 set_err_bit() 总是顺手置第 18 位 (ERROR_UVW_FDB, 等级恒为 1), 会把相等关系顶掉 —— phase_motor_error 里补了(e)/(e2) 两组只置第 6/12 位的用例 (ERROR_LOSE_PHASE_INPUT 与 ERROR_LOSE_LOAD, 等级同为 3, 以及压到同为 2 的第二组); (d) E177/E178/E179/E181 四条落在 AutoResetErrorFcDeal 的 OC 最小间隔夹取上, 而 phase_auto_reset 原来只喂 ERROR_UVW_FDB (非 OC), 夹取分支从来没被走到 —— 补了 4 个故障码 x 5 个间隔值 x 6 个计数器预置 = 120 个用例 (OC 三兄弟 + 一个非 OC 作对照)。另有 5 条经证明**在宿主模型下处处等价**后按纪律删除 (不是标记排除): E084 (devChk 计数上界的 DEV_CHK_TIME_MIN 余量只决定一个不导出计数器的饱和点, 两种写法都饱和在触发阈值之上; 唯一能劈开它的输入要求 devChkTime*50 溢出 C28x 的 16 位 int, 而宿主是 32 位, 且负向对照的 new 侧编译的是『冻结基准的变异副本』, 与交付源码文本不同, 按文本匹配的宿主补丁无法同时命中两者 —— 超出测试台表达能力; 改写的表达式结构与原文逐字一致, 目标端行为不变)、E125 (峰值电流判据 `>` -> `>=`: 赋值目标 errorCurrent 在判据前刚被写成 outCurrentDispOld, 相等时两分支写入同一个值)、E180 (OC 最小间隔比较界 `> 5` -> `> 4`, 兜底仍是 `: 5`, 三种取值都一致)、E193/E196 (GetErrorAttribute 组 0/1 的内部分界 `<` -> `<=`, 而分界值 ERROR_OT_IGBT=14 / ERROR_22=22 都不在各自组的成员集合里, 永远走不到)。证明写在 negative_control.py 五个位置的原处注释与 tools/difftest/cases/f_error/README.md 第 7.3 节

| 项 | 结果 |
|---|---|
| 差分用例数 | **33,762** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AutoResetErrorDeal`, `_AutoResetErrorFcDeal`, `_AutoResetErrorTrueDeal`, `_ErrorDeal`, `_ErrorReset`, `_ErrorSceneSave`, `_GetErrorAttribute`, `_GetErrorResetDeal`, `_GetNewError`, `_GetNewErrorAttribute`, `_GetNumberDigit1`, `_PowerOffRemDeal`, `_UpdateError`, `_bUv`, `_curTime`, `_currentOcDisp`, `_diFunc`, `_diSelectFunc`, `_diStatus`, `_doStatus`, `_dspMainCmd`, `_dspMainCmd1`, `_dspStatus`, `_dspSubCmd`, `_endIndexWriteSeries`, `_errAutoRstClrWaitTicker`, `_errAutoRstNum`, `_errorAttribute`, `_errorAttributeOld`, `_errorCode`, `_errorCodeFromMotor`, `_errorCodeFromMotorList`, `_errorCodeOld`, `_errorDealStatus`, `_errorEeprom`, `_errorInfo`, `_errorInfoFromMotor`, `_errorInfoOld`, `_errorOther`, `_errorOtherOld`, `_errorSpaceTicker`, `_errorsCodeFromMotor`, `_frq`, `_frqFdbTmp`, `_frqRun`, `_funcCode`, `_funcCodeRwMode`, `_funcCodeRwModeTmp`, `_generatrixVoltage`, `_getMotorError`, `_keyFunc`, `_maxFrq`, `_motorErrInfor`, `_motorFc`, `_motorSn`, `_outCurrentDispOld`, `_powerOnStatus`, `_runCmd`, `_runFlag`, `_runMode`, `_startIndexWriteSeries`, `_tuneCmd`, `_underVoltageDeal`, `_uvDealStatus` |
| `.text` 尺寸 | 1227 → 1177 (**-50**) |

### `src/function/f_menu.c`

- 变换: 菜单 / 编辑 / 显示这一层 (2815 行基准 -> 2797 行交付, 73089 -> 75014 字节; 246 行死代码 + 2569 行活代码)。内容: `menu[]` 函数指针表驱动的多级菜单状态机 (0/1/2/3 级 + 口令提示/输入 + U0 用户组), 功能码的增减与进入改值 (ModifyFunccodeUpDown / ModifyFunccodeEnter), 上下限与回转夹取 (LimitDeal / OverTurnDeal / NoSameDeal / LimitOverTurnDeal), 12 个显示缓冲刷新 (UpdateMenu0/1/2/3DisplayBuffer / UpdateErrorDisplayBuffer / UpdateTuneDisplayBuffer / UpdateMenuPwdHint|InputDisplayBuffer / UpdateDisplayBufferAttribute|VisualIoStatus|VisualDiFunc), 分级与组切换 (UpdataFuncCodeGrade / GroupUpDown / DealUserMenuModeGroupGrade / DealCheckMenuModeGroupGrade), 运行量换算 (DispDataDeal / MotorDebugFcDeal / cycleShiftDeal / ValidateTuneCmd)。导出 125 个外部符号 = **70 个函数 + 55 个数据** (46 个 .ebss 标量/结构体 + 9 个 .econst 表), 另加 71 个未定义外部引用。改写手法: (1) 模块私有常量集中到文件头部一块 —— 原文件**没有任何 #define 常量表**, 魔数全散在函数里; 本轮给 18 个具名 (5 是数字位数, 1/2/3/4 是 operateDigit 的个/组/口令档, 100 是用户菜单 group/grade 打包, 16 是每组字母数, 10 是十位步进, 3 是 winkFlag 位基址, 0x01/0x08/0xF8 是 winkFlag 三档, 4/65535 是口令位数与上限, 0x000FFFFF/0x00FF 是位图掩码, 0x1040 是 PLC 视图属性, 0x000F 与 >>4 是 errorFrqUnit 的十进制位, 1<<14 与 >>15 是 PID 显示定标, 10000 是转速显示单位, 5/10 是脉冲频率换算, >>12 是转矩电流定标); (2) 抽两个 `static` 助手 —— `HandleQuickMenuSwitch()` 是四个 MenuXOnEnter 共用的『快速菜单切换』前导, `SetWinkFlag(level)` 是三处 `winkFlag = 1 << (3 + operateDigit)`; (3) 控制流正常化 —— Menu0OnUpDown / Menu2OnShift / UpdateMenu0DisplayBuffer / UpdateMenu2DisplayBuffer / ModifyFunccodeUpDown / MenuPwdInputOnEnter 等改成『提前 return + 扁平守卫』, Menu3OnEnter 的嵌套 if 合并成一个合取式, Menu2OnUpDown 的 if/else 尾巴互换, MenuOnQuick 的 else 分支改成提前 return; (4) 局部量改名 —— 16 个旧名清干净 (data1/upper1/lower1/currentGroupDispF/currentGroupDisp0/tmp1/tmp2/gradeTmp/checkMenuModeFcIndex/errorCodeOld4Menu0/tuneCmdOld4Menu0/menu0Number/attributeMenu0/bitDisp/bMinus/dataTmp), 20 个新名 (dataSigned/upperSigned/lowerSigned/gradePrev/searchFcIndex/errorCodePrev/tuneCmdPrev/dispNumber/dispAttribute/dispIndex/bNegative/absData/dataPrev/pDispBit/dispPara/dispParaNum/groupDispF/groupDisp0/letterIndex/letterDigit), 含两个**函数内 static** (errorCodeOld4Menu0/tuneCmdOld4Menu0 -> errorCodePrev/tuneCmdPrev); (5) 排版 —— 26 行制表符按 4 列制表位展开, 去掉装饰性空行与冗余的匿名作用域, 补全单语句分支的花括号。改写由 build/_rewrite_fmenu.py 按一张 **76 条替换表**生成 (72 条函数级覆盖 50 个函数 + 4 条文件级, 每条带期望命中次数, 不符即中止), 不是手抄; 该脚本先断言输入**逐字节等于**冻结基准, 再断言 13 块死代码整块逐字节仍在、11 条宿主补丁锚点命中次数不变、条件编译行的多重集改写前后相等, 最后断言输出零制表符、纯 ASCII、无注释、125 个导出符号齐全、16 个旧标识符清干净、两个新助手确实是 `static`。差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数 **106865** 与 16 个分阶段计数逐一相同, mismatches=0, 见 docs/_difftest/f_menu.json。交付文件不带任何注释, 改写依据记在替换表、本清单与 tools/difftest/cases/f_menu/README.md。

- 必须守住的不变量:

  - 本模块定义的 **125 个外部符号全部不变** (70 函数 + 55 数据): 目标端 nm2000 实测外部符号 **196 个** (125 定义 + 71 未定义) 与基准**逐一相同**, 关优化下 `.text` 4507 -> 4516 字 (**+9**), 两侧编译告警各 0 条、无新增无消失 (target_check.py 通过)。这条是判据的核心: 70 个函数里只有 9 个非 void, 其余全部 void, 作用都落在**共享状态**上, 所以 driver 把 125 个导出 + 整个 funcCode 联合体 + displayBuffer[6] + funcCodeGradeCurMenuMode[83] + keyFunc/tuneCmd/frqKeyUpDownDelta/funcCodeRwModeTmp + invPara 一起比 —— 少写一个字段、把 menuLevel 与 menuMode 写反, 只有这样才看得见。**契约方向提醒**: 交付契约是『没改写过的必须相同』, 不要求『改写过的一定不同』。`.text` 差 9 字是抽出来的两个 static 助手在 C28x 上不内联造成的 (与 f_error 同一现象)。
  - **两个新助手必须是 `static`**: HandleQuickMenuSwitch / SetWinkFlag。它们是 COFF 的**局部**符号 (nm2000 里小写 t), 不进外部符号集合。这与 f_profibus 提取 DpLatchEepromIndex、f_fcDeal 提取 5 个谓词、f_error 提取 5 个 static 是同一条纪律。**绝不能写成 `LOCALF`**: `f_debug.h` 第 27 行的 `#if 1` 把 `LOCALF` 定义成**空**, 那样会多出 2 个外部符号, target_check.py 的 `symbols_equal` 当场判失败。生成器里有一条断言逐个检查 `static Uint16 <name>(` / `static void <name>(`。反过来, `LOCALD` 展开成 `extern` (f_debug.h:29), 所以 f_menu.c 里 `LOCALD` 声明的那一批函数是**真外部符号** —— 这也是 driver 的 70 个函数里包含 Menu0OnPrg / Menu1OnDown 这类『空实现』的原因。
  - 4 个**函数内 static / 模块私有局部量**改了名 (errorCodeOld4Menu0 -> errorCodePrev, tuneCmdOld4Menu0 -> tuneCmdPrev, 以及 GONE/NEWS 里那 16/20 个)。函数内 static 在目标文件里是 C_STAT 局部符号 (nm2000 里带 `$序号`), 不影响外部符号集合; target_check.py 的 `real_syms()` 按 storage class == 2 (C_EXT) 过滤, 所以改名是**预期**结果, 判据比『名字里带 `$` 就排除』更严 —— 若改写误把 static 改成外部链接, 符号会以 C_EXT 出现, 照样被抓到。
  - **13 块死代码逐字节不变** (行区间以冻结基准为准, 共 246 行): 16-28 `#if F_DEBUG_RAM` 的 0 号分支体; 53-68 `#if 0` 的 RESTORE_COMPANY_PARA_EXCEPT_INDEX 表; 274 `#pragma DATA_SECTION(menu, "const_zone")`; 717-731 Menu2OnUpDown 里 `#elif 1` 的另一支; 790-827 `#if DEBUG_F_RESTORE_COMPANY_PARA_DEAL` (=0); 2006-2010 `#if 0` 的 paraInitMode 分支; 2066-2146 `#if (DEBUG_F_POSITION_CTRL)` (=0); 2208-2210 `#if 0` 的 outPower/itDisp; 2218-2249 `#if 0` 的 RestoreCompanyParaRamDeal 整个函数; 2460-2464 `#if 0` 的 DealCheckMenuModeGroupGrade 收尾; 2703-2716 与 2755-2767 两处 `#if DEBUG_RANDOM_FACPASS` (=0); 2797-2806 `#elif 1` 的空口令函数组。生成器的 DEAD_RANGES 切**整块**做逐字节比对 (比取代表句更强), DEAD_SNIPPETS 另取 13 句代表句各断言一次『出现 1 次』。
  - **条件编译结构逐行原样保留**: `#if F_DEBUG_RAM` / `#elif 1` / `#if 0` / `#if 1` / `#if DEBUG_F_*` / `#elif 1` / `#else` / `#endif` 共 25 条不同写法。除了在 COND_STACKS 里点名断言, 还对『全部条件编译行』做**多重集相等**检查 —— 没有把 `#if F_DEBUG_RAM / #elif 1 / #endif` 塌成一行 `#define`, 那是本工程全局的 RAM/Flash 配置开关, 拆掉就等于在另一个配置下改了行为, 而差分测试只覆盖当前配置。当前配置 (F_DEBUG_RAM = 0, 所以走 `#elif 1` 那一支): DEBUG_F_NO_SAME / DEBUG_F_USER_MENU_MODE / DEBUG_F_CHECK_MENU_MODE / DEBUG_F_DISP_DIDO_STATUS_SPECIAL / DEBUG_F_GROUP_HIDE / DEBUG_F_MOTOR_FUNCCODE1 / DEBUG_F_MFK / DEBUG_F_PASSWORD / DEBUG_F_ERROR_TUNE_USE_SHIFT / DEBUG_FRQ_POINT / DEBUG_ACC_DEC_TIME_POINT **全为 1**; DEBUG_F_POSITION_CTRL = 0 (未定义 ZJ_MD380M), DEBUG_F_RESTORE_COMPANY_PARA_DEAL = 0, DEBUG_RANDOM_FACPASS = 0, DEBUG_F_INV_TYPE_RELATE = DEBUG_F_MOTOR_POWER_RELATE = 1 (f_invPara.h 的 `#elif 1`)。
  - **11 条宿主补丁锚点逐字不变** (`cases/f_menu/host_patch.json`, side=both): `#include "f_menu.h"`; `const Uint16 frqSrcFuncIndex[] = {...}` 整块; `const Uint16 diFuncIndex[] = {...}` 整块; 两条 memcpy(funcCodeGrade, funcCodeGradeAll, FUNCCODE_GROUP_NUM) 写法; sizeof(frqSrcFuncIndex) 与 sizeof(diFuncIndex) 各 2 处; `i < number` 与 `i >= number` 各 1 处; `(loopNumber < upper - low)`; `50 * decNumber[funcCode.code.frqPoint]` 3 处。**为什么这些不能动**: 补丁按**文本**匹配, 而负向对照的 new 侧编译的是『冻结基准 + 一处变异』、正常差分测试的 new 侧编译的是『交付源码』—— 同一套补丁要同时命中两种文本。改写若动了这些表达式, 补丁在新侧找不到锚点 -> count 失配 -> run_case.py 直接 SystemExit。**实测代价**: 变异 M126 原写法 `i < number` 改 `i <= number` 正好撞上补丁锚点, 变体必然编不过 (记 BROKEN), 后来改打循环起点 `i = 0` 改 `i = 1` 才成。`build/_fmenu_patch_clash.py` 把这一类冲突**静态**查出来, 不必等编译。
  - **`#include "f_funcCode_disp.c"` (第 202 行) 故意不动, 这是一个差分测试盖不到的函数**: 该文件在 include/function/ 下 (Debug.lkf 里没有对应 .obj, 所以它不是独立编译单元, 只是被 f_menu.c 拉进来一起编 —— 代码确实进 f_menu.obj)。而 `host_ref_f_menu.c` 与 `host_new_f_menu.c` 都生成在**同一个** gen 目录里, 带引号的 include 先按『包含它的那个文件所在目录』查找, 于是两侧拿到**同一份**文本: UpdateU0Data 的函数体在两侧逐字相同, 改动它**永远**报 0 处不一致。**已实测确认**: 把 `funcCode.group.u0[0] = frqDisp;` 改成 `= frqDisp + 1;`, run_case.py 仍然报 mismatches=0。处置: **不改写该文件**, 保持与重写前逐字节相同 —— 源码相同 + 同一编译器, 行为必然相同, 这比差分测试更强 (差分测试只是『没观测到差异』)。它的等价性由 verify_tree.py (编码/注释) 与 build_and_compare.py (目标端产物级比对) 独立保证。`UpdateU0Data` 与 `commDispIndex` 这两个导出符号由该文件提供, 生成器对它们只断言 include 指令仍在。
  - **测试台的 13 处『不可复现/不终止』形态是刻意排除的, 不是遗漏** (逐条记在 driver.c 文件头 与 cases/f_menu/README.md): (1) Menu3OnEnter 第 758 行的 `writable` 出现在第 784 行的 用户组提前 return **之前**, U 组的越界读在宿主与目标端读到不同内容; (2) MenuOnQuick 的 `menuModeNext[((Uint16)menuModeTmp) - 1]` 在目标端是无符号 16 位减法 (x=0 得 65535)、宿主是 32 位有符号 (-1), 都是越界但位置不同; (3) cycleShiftDeal 的 do-while 在 para 低位全 0 时死循环; (4) NoSameDeal 的 `i < number` 是**无符号**比较 (i 是 Uint16, number 是 int16), number 取负会跑 6 万多次; (5) NoSameDeal 在 upper < lower 时死循环; (6) ModifyFunccodeEnter 把 dataNew 原样写进 funcCode 后当**表下标**用, 喂 65535 就是 131 KB 越界读; (7) UpdateGroupDisplay 在 FUNCCODE_GROUP_CF(64) 与 U0(67) 之间留了 65/66 两个**空号**, 走到就是 `DISPLAY_CODE[65534]` 越界; (8) 反汇编层面的既有缺陷见 README。另有两条**契约域约束** (不是缺陷, 但喂错就崩): (9) `curFc` 必须是一个真实存在的 (group, grade) 对 —— Menu2OnEnter 第 662 行 / Menu3OnUpDown 第 949 行都写 `curFc.index = GetGradeIndex(curFc.group, curFc.grade)` 而 GetGradeIndex 就是 `funcCodeGradeSum[group] + grade`, **没有任何边界检查**, 用户组的 funcCodeGradeSum 已 >= FNUM_PARA(909), 一进去就顶出 funcCode.all[]; (10) `flag` 只可能是 ON_UP_KEY(1) / ON_DOWN_KEY(2) —— 三个函数都把它原样转给 LimitOverTurnDeal 做 `data += deltaK[flag]`, 而 `const int16 deltaK[3] = {0, 1, -1}` 只有 3 项。这两条是 driver 自己的 bug, 在本轮定位并修掉 (改 curFc 取值域、改 flag 夹取), 已写进 driver 文件头。
  - **driver 补了两个针对性的阶段/写法, 都是『把常量变成输入』而不是『改测试台迁就代码』**: (a) `phase_no_same_wrap()` —— NoSameDeal 的 `data > upper` 与 `data < lower` 两条回卷判据只有在 data 步进后**恰好落在界上**时才与 `>=`/`<=` 写法分得开, 而随机用例里撞值本来不常有、撞值又恰好压在界上几乎不可能。实测 M130 (`data > upper` 改 `>=`) 在 20000 个随机用例下是 **MISSED**; 补上这个阶段 (8 个用例: 4 组 lower x 上下两个方向, 用真表 ref_frqSrcFuncIndex 的两个下标把碰撞值精确摆在 upper-1 / lower+1) 之后 M130 与 M133 都变成 CAUGHT。加阶段只**增加**用例, 所以此前判 CAUGHT 的变异仍然 CAUGHT (『抓到』在增加用例下单调), 证据有效。(b) `ccenv.run_exe()` 加了 **300 s 超时** —— 一个不终止的变体会把整轮负向对照**永久**挂住: 实测 M132 (`data -= 1` 改 `data -= 2`) 挂了 16 分钟以上 (跨度是奇数时永远碰不到 lower), 而 `ps` 里只看到一个 exe, 极易被误读成『机器慢』。超时后杀进程并抛 SystemExit, 记成 BROKEN —— 『变体没给出结果』是诚实的结论。
  - **变异总数 448** (编号 M001~M553, 中间跳过已删除的 M117/M119), 逐条记在 negative_control.py 的 `'f_menu'` 条目里, 来源分三份按源码区间分工 (build/_fmenu_muts_a.py 行 341~1108 / _b.py 行 1110~1938 / _c.py 行 1939~2814), 合并后 450 条**核减为 448 条**, 三条改动都写在原处注释与 README 里, 不是静默丢条目: (1) **M117 / M119 证明为可证等价后删除** —— `LimitDeal` 的 `if (data1 > upper1) { data1 = upper1; }` 改 `>=` (与 `if (data1 < lower1) { data1 = lower1; }` 改 `<=`): 被守卫的语句写回的**就是**比较的那个界, 在唯一的差别点 (data1 == upper1 / == lower1) 上赋值是幂等的, 两种写法在全部输入上结果逐位相同, 任何测试都区分不了 —— 这是可证等价, 不是覆盖不够 (把不可达状态喂进测试台只会让结论看着更强其实更弱)。注意这不是改写引入的: 冻结基准里就是这个形状。(2) **M126 改锚点**: 原写法与宿主补丁锚点 `i < number` 冲突, 变体必然 BROKEN。(3) **M132 改锚点**: 原写法 (`data -= 2`) **不终止**, 没法用『跑出差异』观测 (真固件同样会挂), 改成打同一分支的下翻回卷目标 `data = upper` -> `upper - 1` (仍是同类 bug, 而步长保持 1 必然终止)。
  - 本用例必须用真头文件 (real.txt = `*`)。理由: 模块的 include 链把 funcCode / funcCodeAttribute / funcCodeGradeAll / funcCodeGradeSum / funcCodeGradeCurMenuMode / invPara / displayBuffer / diFunc / frqPoint 等全部带进来, 而 driver 要读**联合体成员的偏移**、位域布局 (FUNC_ATTRIBUTE 的 point[0:2] unit[3:5] displayBits[6:8] upperLimit[9] lowerLimit[10] writable[11:12] signal[13] multiLimit[14:15]) 与三张真表的**内容**; 抄一份结构体等于在测另一份。只有 int16/int32/Uint16/Uint32 被 shim/DSP28x_Project.h 与公共 shim 的 DataTypeDef.h (PINNED_SHIMS, 不转发) 钉到精确宽度 —— DataTypeDef.h 必须钉住, 真文件把 `u16` 定义成 `unsigned int`, 宿主上是 32 位, 转发进去整个测试台的宽度前提就没了。driver 还镜像了模块私有、头文件里没有的宏: `UP_DOWN_DEAL_DONE_TIME 800` 与 `DECIMAL_DISPLAY_UPDATE_TIME` (见 mock_menu.h)。
  - **三张真表从 src/function/f_funcCode.c 逐字抄出** (funcCodeAttribute[909] / funcCodeGradeAll[83] / funcCodeGradeSum[83], 经 build/_gen_fmenu_tables.py 生成 tbl_*.inc, 由 mock_menu.c `#include`)。本模块整套限值/分级语义都挂在这三张表上, 用合成表等于换了一套语义。check_invariants() 核对: `sizeof(FUNCCODE_ALL)/sizeof(Uint16) >= FNUM_ALL` (**不是 ==**: 宿主 sizeof 返回字节数, 而 f_funcCode.h 里那批 `Uint16 all[sizeof(struct X)]` 的『按字重叠』写法依赖 C28x 的『sizeof 返回字数』, 于是宿主上 FUNCCODE_ALL 被撑到 1290 字而目标端是 1116 字; 两边共用同一份布局, 分歧在比较中抵消, 目标端 .econst 的逐字节一致性由 build_and_compare.py 的 Tier B 独立证明), 两张 GetCodeIndex 表的宿主取值都落在 union 实际宽度内, FMENU_ATTR_N == FNUM_PARA, funcCodeAttribute 的 upperLimit/lowerLimit 链**无环** (GetDispDigits 会顺着链走, 有环就死循环), 驱动会喂进去的下标全都 < FNUM_PARA, FUNC_ATTRIBUTE 的位序与驱动假设一致。
  - 有意不测的东西: 13 块死代码 (不参与编译, 打上变异必然 MISSED, 那是测试台的**正确行为**, 不是洞), 以及 UpdateU0Data (见上, 差分测试对它没有分辨力, 靠『不改写』保证等价)。**没有**其它有意不测项 —— 2569 行活代码里除上述外都被 106865 个用例覆盖, 其中随机混合 20000 个, 分 17 个阶段 (基础工具 12915 / NoSameDeal 回卷 8 / ModifyFunccodeUpDown 65448 / ModifyFunccodeEnter 4805 / UpdateDisplayBufferAttribute 1441 / UpdateGroupDisplay 91 / UpdateMenu0* 84 / UpdateMenu1/2/3DisplayBuffer 779 / 其它显示 54 / DispDataDeal 7 / cycleShiftDeal 48 / 分级查找 220 / MenuInit/ModeDeal/ModeSwitch 115 / 菜单按键 handler 696 / 口令与 ValidateTuneCmd 154 / 随机混合 20000)。菜单状态机是**序列相关**的, 所以 phase_menu_handlers 有意把『按下 -> 再按下 -> 再按下』打成轨迹, 让状态机的转移边变成可观测量。

| 项 | 结果 |
|---|---|
| 差分用例数 | **107,013** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ABZPos`, `_ClearRecordDeal`, `_DISPLAY_CODE`, `_DealCheckMenuModeGroupGrade`, `_DealUserMenuModeGroupGrade`, `_DispDataDeal`, `_FcDigit2Bin`, `_GetDispDigits`, `_GetFuncCodeInit`, `_GetNumberDigit`, `_GetNumberDigit1`, `_GroupUpDown`, `_InverterTypeRelatedParaDeal`, `_LED_CODE`, `_LimitDeal`, `_LimitOtherCodeDeal`, `_LimitOverTurnDeal`, `_LpfCalc`, `_Menu0AddMenuLevel`, `_Menu0OnEnter`, `_Menu0OnPrg`, `_Menu0OnShift`, `_Menu0OnUpDown`, `_Menu1OnDown`, `_Menu1OnEnter`, `_Menu1OnPrg`, `_Menu1OnShift`, `_Menu1OnUp`, `_Menu1OnUpDown`, `_Menu2OnDown`, `_Menu2OnEnter`, `_Menu2OnPrg`, `_Menu2OnShift`, `_Menu2OnUp`, `_Menu2OnUpDown`, `_Menu3OnDown`, `_Menu3OnEnter`, `_Menu3OnPrg`, `_Menu3OnShift`, `_Menu3OnUp`, `_Menu3OnUpDown`, `_MenuInit`, `_MenuModeDeal`, `_MenuModeSwitch`, `_MenuOnMfk`, `_MenuOnQuick`, `_MenuOnRun`, `_MenuOnStop`, `_MenuPwdHint2Input`, `_MenuPwdHintOnQuick`, `_MenuPwdHintOnShift`, `_MenuPwdInputOnDown`, `_MenuPwdInputOnEnter`, `_MenuPwdInputOnQuick`, `_MenuPwdInputOnShift`, `_MenuPwdInputOnUp`, `_MenuPwdInputOnUpDown`, `_MenuPwdOnPrg`, `_ModifyFunccodeEnter`, `_ModifyFunccodeUpDown`, `_MotorDebugFcDeal`, `_MotorPowerRelatedParaDeal`, `_NoSameDeal`, `_OverTurnDeal`, `_ResetUpDownFrq`, `_UpdataFuncCodeGrade`, `_UpdateDisplayBufferAttribute`, `_UpdateDisplayBufferVisualDiFunc`, `_UpdateDisplayBufferVisualIoStatus`, `_UpdateErrorDisplayBuffer`, `_UpdateGroupDisplay`, `_UpdateMenu0DispStatus`, `_UpdateMenu0DisplayBuffer`, `_UpdateMenu1DisplayBuffer`, `_UpdateMenu2DisplayBuffer`, `_UpdateMenu3DisplayBuffer`, `_UpdateMenuPwdHintDisplayBuffer`, `_UpdateMenuPwdInputDisplayBuffer`, `_UpdateTuneDisplayBuffer`, `_UpdateU0Data`, `_ValidateInvType`, `_ValidateTuneCmd`, `_ZCount`, `_accDecFrqPrcFlag`, `_accDecFrqTicker`, `_aiDeal`, `_bFrqDigital`, `_bFrqDigitalDone4WaitDelay`, `_checkMenuModeCmd`, `_checkMenuModeDealStatus`, `_checkMenuModePara`, `_checkMenuModeSerachNone`, `_commDispIndex`, `_curFc`, `_curFcDispDigits`, `_curTime`, `_currentOcDisp`, `_currentPu`, `_cycleShiftDeal`, `_decNumber`, `_deltaK`, `_diFunc`, `_diFuncIndex`, `_diStatus`, `_dispAttributeU0`, `_displayBuffer`, `_doStatus`, `_enCoderPosition`, `_errorAttribute`, `_errorCode`, `_errorInfo`, `_facPassViewStatus`, `_factoryPwdStatus`, `_frq`, `_frqAim`, `_frqAimDisp`, `_frqAimPLCDisp`, `_frqDisp`, `_frqDroop`, `_frqFdb`, `_frqKeyUpDownDelta`, `_frqPLCDisp`, `_frqRun`, `_frqRunDisp`, `_frqSrcFuncIndex`, `_frqXDisp`, `_frqYDisp`, `_funcCode`, `_funcCodeAttribute`, `_funcCodeGradeAll`, `_funcCodeGradeCurMenuMode`, `_funcCodeGradeSum`, `_funcCodeOneWriteIndex`, `_funcCodeOneWriteNum`, `_funcCodeRwMode`, `_funcCodeRwModeTmp`, `_gPhiRtDisp`, `_generatrixVoltage`, `_groupDisplay`, `_groupHideChkOkFlag`, `_groupHidePwdStatus`, `_hexNumber`, `_invPara`, `_invTypeLimitTable`, `_invtStatus`, `_itDisp`, `_keyFunc`, `_lineSpeed`, `_loadSpeedDisp`, `_maxFrq`, `_memoryValue`, `_menu`, `_menu0DispStatus`, `_menu3Number`, `_menuAttri`, `_menuFc`, `_menuLevel`, `_menuMode`, `_menuModeOld`, `_menuModeStatus`, `_menuModeTmp`, `_menuPwdNumber`, `_motorDebugFc`, `_motorFc`, `_motorSn`, `_outCurrentDisp`, `_outPower`, `_outVoltageDisp`, `_p2pData`, `_pcOriginDisp`, `_pidFunc`, `_pidFuncFdbDisp`, `_pidFuncRefDisp`, `_plcStep`, `_pmsmRotorPos`, `_pulseInFrq`, `_pulseInFrqDisp`, `_runFlag`, `_runMode`, `_runSrc`, `_setRunLostTime`, `_stopDispIndex`, `_superFactoryPass`, `_temperature`, `_ticker05msFuncEachDisp`, `_ticker05msFuncInternalDisp`, `_ticker4LowerDisp`, `_torQueFrqDispLpf`, `_torqueAim`, `_torqueCurrent`, `_torqueCurrentAct`, `_tuneCmd`, `_upDownDelta`, `_userMenuModeFcIndex`, `_vfSeparateVol`, `_vfSeprateVolAim` |
| `.text` 尺寸 | 4507 → 4516 (**+9**) |

### `src/system/DSP2803x_CpuTimers.c`

- 变换: 把三个定时器各自重复一遍的初始化序列抽成文件内静态函数 InitOneTimer(定时器, 寄存器块), 三处调用它; 0xFFFFFFFF 提为具名常量 CPU_TIMER_PRD_MAX; ConfigCpuTimer 把 Timer->RegsAddr 提成一个局部指针 Regs, 后续不再逐次重解引用。T2 首个模块。

- 必须守住的不变量:

  - 函数名 InitCpuTimers / ConfigCpuTimer 与三个全局 CpuTimer0/1/2 不变
  - 每个定时器的写入顺序不变 (PRD -> TPR -> TPRH -> TCR.TSS -> TCR.TRB -> InterruptCount), 三只定时器之间仍按 0/1/2 的顺序
  - ConfigCpuTimer 只写 Timer->RegsAddr 指向的那一块, 不碰另外两只
  - temp = (long)(Freq * Period) 的乘法与浮点转整形的类型不变, PRD.all 仍是 Uint32
  - TCR 位域的写位集合不变: Init 只写 TSS/TRB, Config 额外写 SOFT/FREE/TIE (0/0/1)
  - 新增的 InitOneTimer 是 static, 不进外部符号集合 —— 由 target_check.py 的符号集合比对守住
  - 三个寄存器块在宿主上由 mock_cputimers.c 提供普通内存实例, 两侧共用同一份, 只比内容不比地址

| 项 | 结果 |
|---|---|
| 差分用例数 | **20,312** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ConfigCpuTimer`, `_CpuTimer0`, `_CpuTimer0Regs`, `_CpuTimer1`, `_CpuTimer1Regs`, `_CpuTimer2`, `_CpuTimer2Regs`, `_InitCpuTimers` |
| `.text` 尺寸 | 114 → 87 (**-27**) |

### `src/motor/MotorInfoCollect.c`

- 变换: ADC 面向的采集层三个入口 (GetCurExcursion / GetUDCInfo / GetCurrentInfo)。改写手法: (1) 控制流正常化 —— `EnableCount = (EnableCount>200)?200:EnableCount;` 的三元夹取写成 `if (EnableCount > 200) { EnableCount = 200; }`; 两处嵌套 `if` 收尾改提前 return (`if (Count < 32) return;`); 两个 `-32768 == m_ErrIx` 的饱和保护由负字面量比较写成正字面量在左的形式; 长表达式按物理含义断行 (扣偏置 / 采样定标 / 母线定标各占一行); (2) 排版 —— 制表符与行内多空格对齐统一成 4 空格, 去掉装饰性空行。**未做**的是把全局量提成局部指针: 实测那样在 C28x 上对静态分配的全局量是**劣化** —— 直接寻址 `MOV AL,@0x2c` 是 1 字, 换成指针要 `MOVL XARn,#addr` + 栈上溢出 + 调用后重载, 三个函数合计 .text 从 297 涨到 367 (+70 字), 且 GetUDCInfo/GetCurrentInfo 都在 ADC 中断里。所以本轮只改表达式与控制流。

- 必须守住的不变量:

  - **三个函数名与 15 个全局量全部不变**; 目标端外部符号集合 **23 个逐一相同** (nm2000 实测: _AdcResult/_GetCurExcursion/_GetCurrentInfo/_GetUDCInfo + 3 个本模块定义的 gADC/gCurSamp/gExcursionInfo 一类 + 12 个别处定义的引用), 两侧编译告警各 0 条、无新增无消失 (target_check.py 结论: 通过)。
  - **GetUDCInfo 与 GetCurrentInfo 的目标端指令序列与改写前逐行相同** —— 关优化下 .text 297 -> 298, 差值全部落在 GetCurExcursion (+1 字), 这两个函数 **0 字**。这不是失败, 是本工程契约里最强的结果 (『没改写过的必须相同』, 不要求『改写过的一定不同』)。这两个函数恰好是**在 ADCOverInterrupt 中断里被调用**的两个 (MotorMain.c:660/661), 所以中断路径的时序完全没动, 不需要重测。
  - GetCurExcursion 的 +1 字来源已定位: 三元夹取编译成 `MOVB AL,#0xc8` + 尾部落回 `MOV @0x25,AL` (2 字, 值留在 AL 里复用), 改写成 `if` 后变成 `MOVB @0x25,#0xc8,UNC` (2 字) 之后必须重新 `MOV AL,@0x25` 才能做下一个比较 —— 净 +1 字 / +1 周期。该函数由 Main2msMotorD (MotorMain.c:343) 调用, 在 2 ms 任务里, 不在中断路径, 1 个周期可忽略。
  - 三个函数的**写入顺序与写入字段集合**不变: GetCurExcursion 仍是 EnableCount 斜坡 -> TotalIu/Iv 累加 -> Count -> 均值 -> 饱和保护 -> 三个累加器清零 -> RunEnable -> 偏差判据 -> (正常分支写 ErrIu/ErrIv/ErrCnt) / (报错分支 |= ERROR_CURRENT_CHECK 并清 ErrCnt/EnableCount); GetCurrentInfo 仍是 Iu 链 -> Iv 链 -> Q24 U/V -> W = -(U+V) 再限幅 -> 温度 -> 三路 AI 累加 -> 计数器。
  - **没有**引入任何新的外部符号: 三个函数都是 void, 没有新增 static 助手 (唯一的新标识符是函数内 的局部量 m_uDC/m_Iu/m_Iv, 原本就有)。符号集合由 target_check.py 的 symbols_equal 守住。
  - 驱动把**每个字段**都比: 每侧私有 gExcursionInfo/gUDC/gIUVWQ24/gTemperature/gAI/gCurSamp (memcpy 整结构体), 共享的 gMainStatus/gError/gShortGnd/gUVCoff/AdcResult 也整块比 —— 少写一个字段、把 U 与 V 写反、把 W 的符号弄错, 只有这样才看得见。
  - **差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数 515204 与 4 个分阶段计数逐一相同, mismatches=0**, 见 docs/_difftest/MotorInfoCollect.json。分阶段: GetCurExcursion 493920 / GetUDCInfo 108 / GetCurrentInfo 1176 / 随机混合 20000。
  - **负向对照 44/44 全抓到, 0 BROKEN**, 见 docs/_difftest/_negctl_partial_MotorInfoCollect.json。原始清单 47 条, 其中 3 条经证明为**可达状态空间内处处等价**后删除 (不是覆盖不足): (a) 原 N12/N13『饱和替值 -32767 改 -32766』—— 被守卫写入的 m_ErrIu/m_ErrIv 之后只有两处使用: `abs(m_ErrIu) < 5120` 的比较, 与 `gExcursionInfo.ErrIu = m_ErrIu;` (它在比较为**真**的分支里)。守卫只在 m_ErrIu == -32768 时触发, 触发后是 -32767 或 -32766, 两者 abs 都 >= 5120, 于是比较恒假、赋值恒不可达 —— 两个候选值没有任何可观测量依赖它; (b) 原 N42『温度 AD 掩码 0xFFF0 改 0xFFF1』—— ADC_TEMP 是 `(AdcResult.ADCRESULT4 << 4)` (SystemDefine.h:50), 低 4 位恒为 0, 两个掩码只在 bit0 上不同而 bit0 恒为 0, 于是两条表达式都是恒等函数。把不可达状态喂进测试台只会让结论看着更强其实更弱, 所以删除而不是『加覆盖』。
  - **测试台补了两个覆盖洞, 都是『把常量变成输入』而不是改测试台迁就代码**: (1) `dirty_status()` —— 把 gMainStatus 与 gError 预置成全 1。ref-new 比对只看得到 new 侧的变化, 一个『不再写某字段』的变异只有在原值 != 正确值时才可见; 从全 0 起步时 `StatusWord.bit.RunEnable = 1` 被改成 `= 0` 在两侧产生完全相同的字节, 是**不可见**的。加之前 N14 会 MISSED, 加之后 CAUGHT。(2) 扫描量从 TotalIu 换成**移位后**的结果 —— 两个 -32768 保护与 5120 判据真正比较的是 `(TotalIu + Iu) >> 5`, 不是 TotalIu。原驱动把 5119/5120/5121 喂给了 `ErrIu`/`ErrIv`, 而 GetCurExcursion **只写不读**这两个字段, 等于什么都没喂。本轮把 TotalIu 写成 `tot[e] - Iu` (被测函数会先 `TotalIu += Iu` 再右移), 让累加后的值**恰好**落在边界上; 并且 TotalIu 与 TotalIv **独立**扫描, 因为 5120 判据是合取式, `&&` 与 `||` 只在两相幅度恰有一相低于门限时才分得开。踩过一次: 第一版直接写 `TotalIu = 5120*32` 而 Iu = 0x1234, 移位后是 5265 不是 5120, 于是 `< 5120` 与 `< 5121` 不可区分, N15/N16 报 MISSED; 改成 `tot[e] - Iu` 之后两条都 CAUGHT。用例数因此从 2310 涨到 493920 (只增不减, 此前判 CAUGHT 的变异仍然 CAUGHT)。
  - **有意不覆盖、逐条记录**: (1) 宿主的 `int`/`Uint` 是 32 位而 C28x 上是 16 位, 所以 gUDC.uDC/uDCFilter 的 `(a+b)>>1` 在目标端 65536 处回绕、宿主上是 4G, CUR_EXCURSION_STRUCT 的 int 字段饱和点也不同 —— 宿主不复现目标的截断/回卷点。比对本身仍有效 (两侧同为宿主), 但这不是对 C28x 行为的断言, 已写在 driver.c 文件头。**注意这一条还改变了一个可达性结论**: 目标端 `int m_ErrIu = (long)TotalIu >> 5` 会在 TotalIu>>5 == 32768 时截断成 -32768 从而触发饱和保护, 宿主 (int 32 位) 不会 —— 这正是 原 N12/N13 在宿主上不可观测的**另一个**原因, 与上面 (a) 的证明互补。(2) `__IQsat` 是 **C28x 编译器内建**, 不是工程宏 (由反汇编证明: `MOVB XAR6,#0x64 / MOV PL,#0xff9c / MINL ACC,XAR6 / MAXL ACC,P`, 即 min(max(A,Neg),Pos) 作用在 32 位 ACC 上)。宿主由用例 shim 提供语义等价的 static 函数, **饱和值**仍被比对 (所以改限幅实参会被抓到, 见 N32/N37/N41), 但没证明宿主限幅与 MINL/MAXL 在**每个**输入上逐位相同。(3) ADC 通道宏带 `<<4` (SystemDefine.h:45-60), 宿主上 `Uint16<<4` 提升成 32 位而 C28x 上截回 16 位, 同属第 (1) 条。(4) 浮点: 本模块没有。

| 项 | 结果 |
|---|---|
| 差分用例数 | **515,204** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AdcResult`, `_GetCurExcursion`, `_GetCurrentInfo`, `_GetUDCInfo`, `_gADC`, `_gAI`, `_gCurSamp`, `_gError`, `_gExcursionInfo`, `_gIAlphBeta`, `_gIAlphBetaQ12`, `_gIAmpTheta`, `_gIMTQ12`, `_gIMTQ24`, `_gIMTSetQ12`, `_gIUVWQ12`, `_gIUVWQ24`, `_gLineCur`, `_gMainStatus`, `_gShortGnd`, `_gTemperature`, `_gUDC`, `_gUVCoff` |
| `.text` 尺寸 | 297 → 298 (**+1**) |

### `src/motor/MotorParEst.c`

- 变换: 电机参数辨识的调度层三个入口 (RunCaseGetPar / PrepareParForTune / EndOfParIdentify) 加六张 const 流程表。改写手法: (1) RunCaseGetPar —— 把 `switch(gGetParVarable.ParEstContent[gGetParVarable.ParEstMstep])` 的选择子先读进局部量 `m_stepNow`, switch 分派在快照上; 8 路 case 与 default 的顺序、目标函数、`ParSendTune()` 的收尾位置一字未动。(2) PrepareParForTune —— 字段搬运仍是逐条赋值 (未合并、未改序), 只是去掉了列对齐用的多空格; 表选择 switch 保持 5 路 + default; 搬运循环 `for(m_index=0; ...) ParEstContent[m_index] = *(m_PIdentifyFlow + m_index);` 改写成块内两个游走指针 `*m_pDst = *m_pSrc; m_pDst++; m_pSrc++;`。(3) EndOfParIdentify —— `gGetParVarable.QtEstDelay` 先读进局部量 `m_delay`, 两个分支 (`== 0` / `>= 5`) 复用同一次读; `TUNE_FINISH != StatusWord` 与 `IDC_FVC_CTL == gMainCmd.Command.bit.ControlMode` 都写成常量在左; 所有 if/else 补花括号。**没有**改任何判据常量、写入字段、调用顺序或写入次序。

- 必须守住的不变量:

  - **三个函数名与全部文件级符号不变**: 六张 const 流程表 (AsynParEstStatic / AsynParEstRotor / pmTuneProgNoLoad / pmTuneProgLoad / noTuneProgress / debugTuneProcess) 与五个全局量 (gUVCoff / gMotorExtReg / gGetParVarable / gRrLoIdentify / gLmIoIdentify) 名字、类型、初值一字不动。目标端外部符号集合 **43 个逐一相同** (nm2000 实测, target_check.py `符号集合相同: True`), 两侧编译告警各 5 条、无新增无消失 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数 20786 与 4 个分阶段计数逐一相同, mismatches=0**, 见 docs/_difftest/MotorParEst.json。分阶段: RunCaseGetPar 600 / PrepareParForTune 18 / EndOfParIdentify 168 / 随机混合 20000。
  - **负向对照 64/64 全抓到, 0 BROKEN**, 见 docs/_difftest/_negctl_partial_MotorParEst.json。64 条按三个函数分组: P01-P15 打 RunCaseGetPar 的放弃前导 (故障/未启动判据取反、放弃时状态字改错、不再 DisableDrive、不再 DINT、8 路 case 两两互换、default 改指、不再回送调参结果), P16-P39 打 PrepareParForTune 的字段搬运 (每个源字段换成另一个、写到错的目标字段、清零量改非零、表选择错配、搬运循环起点/上界/写入值出错、不再复位 RR/LO 辨识变量), P40-P64 打 EndOfParIdentify (收尾判据 `== 0` 改 `<= 0`、故障等待判据 `>= 5` 改 `>= 4`、成功/完成状态字互换或取反、不再清 PrgStatus、PIE 重指向错、EALLOW/EDIS 丢一个、不再重新 InitSetPWM/InitSetAdc/SetInterruptEnable、不再 EINT/ERTM、QtEstDelay 步进改 2)。**没有一条被证明为等价而删除** —— 本模块的判据常量全部落在可达区间里 (辨识流程会真的把 QtEstDelay 走到 0 与 >= 5)。
  - **测试台的关键设计: 调用序列必须成链**。RunCaseGetPar 是 8 路分派 + 收尾 ParSendTune, PrepareParForTune 是纯搬运, EndOfParIdentify 有 `QtEstDelay++` 的跨调用累积 —— 所以驱动不是逐函数孤立喂参, 而是 600 步的『准备 -> 逐级推进 -> 收尾』链式序列 (phase_run_case), 让 ParEstMstep / QtEstDelay / StatusWord 三个跨调用状态真的走完一遍。**调用日志按序列比对**: 被分派的 12 个外部函数 (RsIdentify / RrLoIdentify / LmIoIdentify / SynTuneInitPos / SynTunePGZero_No_Load / SynTunePGZero_Load / SynTuneBemf / ParSendTune / DisableDrive / InitRrLoVariable / InitSetPWM / InitSetAdc / SetInterruptEnable) 每次调用往共享 gCallLog[] 追加自己的 id, 两侧比对**序列**而不是集合 —— 否则 `case IDENTIFY_RR_LO: LmIoIdentify();` 这类『分派到错函数』的变异会因为两边都只调一次而看不出差别。这正是 P08-P14 能被抓到的原因。
  - **CPU 控制序列被计数观测**: DINT/EALLOW/EDIS/EINT/ERTM 在 shim 里变成 `(gHostCpuCtl[n]++)`, 两侧比对 7 个计数器。所以『放弃时不再 DINT』(P07)、『收尾不再 EALLOW/EDIS 成对』(P57 一类)、『不再 EINT/ERTM』(P63 一类) 是可观测的, 而不是因为宿主不执行这些指令而完全看不见。
  - **每个字段都比**: 两侧私有的 gGetParVarable / gUVCoff / gMotorExtReg / gPmParEst / gEstBemf / gIPMZero / gIPMPos / gUVWPG / gPGData / gVCPar (memcpy 整结构体) 与共享的 gMainStatus / gMainCmd / gError 整块比对 —— 少搬一个字段、把 R1 搬成 R2、把 UvwDir 搬成 UvwZeroPos_deg, 只有这样才看得见。
  - **目标端尺寸差已定位到函数**: .text 359 -> 369 (+10), 全部落在三个函数里 —— RunCaseGetPar 82 -> 85 (+3), PrepareParForTune 185 -> 189 (+4), EndOfParIdentify 92 -> 95 (+3); 六张 const 流程表是 .const 数据, 0 字变化 (build/_funcsizes.py 由 nm2000 符号地址差算出)。+10 的成因是: 局部量 m_stepNow / m_delay 各占一个栈槽并多一次 `MOV` 装载, 搬运循环换成两个游走指针后多一次指针自增。**这三个函数都由 2ms 任务调用 (非中断路径)**, 10 个字 / 约 10 个周期的量级在 2 ms 预算里可忽略; 中断路径未触及。
  - **没有**引入任何新的外部符号: 唯一的新标识符是函数内局部量 m_stepNow / m_delay / m_pDst / m_pSrc (都是自动存储期), 不产生新的 C_EXT 符号。符号集合由 target_check.py 的 `符号集合相同: True` 守住。

| 项 | 结果 |
|---|---|
| 差分用例数 | **20,786** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ADC_Over_isr`, `_AsynParEstRotor`, `_AsynParEstStatic`, `_EPWM1_TZ_isr`, `_EPWM1_zero_isr`, `_EndOfParIdentify`, `_GpioDataRegs`, `_InitRrLoVariable`, `_InitSetAdc`, `_InitSetPWM`, `_LmIoIdentify`, `_ParSendTune`, `_PieCtrlRegs`, `_PieVectTable`, `_PrepareParForTune`, `_RrLoIdentify`, `_RsIdentify`, `_RunCaseGetPar`, `_SetInterruptEnable`, `_SynTuneBemf`, `_SynTuneInitPos`, `_SynTunePGZero_Load`, `_SynTunePGZero_No_Load`, `_debugTuneProcess`, `_gError`, `_gEstBemf`, `_gGetParVarable`, `_gIPMPos`, `_gIPMZero`, `_gLmIoIdentify`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtInfo`, `_gMotorExtReg`, `_gPGData`, `_gPmParEst`, `_gRrLoIdentify`, `_gUVCoff`, `_gUVWPG`, `_gVCPar`, `_noTuneProgress`, `_pmTuneProgLoad`, `_pmTuneProgNoLoad` |
| `.text` 尺寸 | 359 → 369 (**+10**) |

### `src/function/f_dspcan.c`

- 变换: DSP 片内 eCAN 的五个入口 (InitdspECan / InitTranMbox / InitRecMbox / eCanDataTran / eCanDataRec)。改写手法: (1) InitdspECan —— 四段顺序执行的 `if (con == N)` 保持顺序 if 而非改写成 else-if 链 (con=0 会把 con 置 1, 紧接的 con==1 块**必须**继续执行, 改成 else-if 会整段跳过); 邮箱清零循环由 `MsgCtrlPi[i<<2] = 0` 改成游走指针 `*MsgCtrlPi = 0; MsgCtrlPi += 4;`; CCE 判据写成 `== 0` / `== 1` (CCE 是 1 位字段, 与原来的 `!= 1` / `!= 0` 等价)。(2) InitTranMbox —— **`eCanTranEnFlag |= 1ul << mbox;` 一字未动地保留**: 它移的是 mbox 而 InitRecMbox 移的是 id = mbox & 0x1f, 这个不对称是原行为, 不是要修的 bug; 数据写入改成 `dataPi[0]` / `dataPi[1]` 显式下标。(3) InitRecMbox —— `(mbox & 0x80) == 0x80` 简化为 `mbox & 0x80` (单比特), `(mbox & 0x40) == 0x40` 同理。(4) eCanDataTran —— `(flag & bit) != bit` 改成 `!(flag & bit)`。(5) eCanDataRec —— 外层判据倒置成早退 `return CAN_MBOX_EMPTY;`, 两处相同的 `CANRMP.all = 1ul<<mbox` 提到 CANRML 读之前 (CANRMP 与 CANRML 是不同寄存器, 重排等价)。**没有**改任何判据常量、寄存器写入值、错误码或写入次序。

- 必须守住的不变量:

  - **五个函数名与全部文件级符号不变**: 目标端外部符号集合 **12 个逐一相同** (nm2000 实测, target_check.py `符号集合相同: True`), 两侧编译告警各 0 条、无新增无消失 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数 29356 与 8 个分阶段计数逐一相同, mismatches=0**, 见 docs/_difftest/f_dspcan.json。分阶段: InitdspECan 成功 392 / 波特率错误 12 / InitTranMbox 204 / InitRecMbox 392 / eCanDataTran 196 / eCanDataRec 68 / 随机混合 28080 / 超时锁存 12。
  - **`.text` 604 -> 580 (-24) 字**, 逐函数归属 (build/_funcsizes.py, nm2000 相邻地址差分): _InitdspECan -3 / _InitTranMbox -1 / _InitRecMbox -4 / _eCanDataTran -4 / _eCanDataRec -12。**没有任何一个函数变大**, 也没有新增外部符号。
  - **负向对照 80/80 全抓到, 0 BROKEN**, 见 docs/_difftest/_negctl_partial_f_dspcan.json。80 条按函数分组: D01-D04 打 InitdspECan 的两个早退判据与错误码, D05-D16 打 GPIO 复用/量化/上下拉与 CANMC 位, D17-D19 打邮箱与 LAM 清零, D20-D45 打四段状态机的迁移条件/重试计数/波特率寄存器取值/收尾清零/EALLOW-EDIS 配对, D47-D53 打 InitTranMbox 的 MSGID/MSGCTRL/CANMD/CANME/数据字与邮箱号掩码, D55-D82 打 InitRecMbox 的 RTR 判据、eCanDataTran 的标志位与错误码、eCanDataRec 的 RMP/RML 寄存器选择、长度掩码、溢出与空帧返回码。
  - **已记录的覆盖缺口 (1 处, 是测试台守不住, 不是改写有问题)**: 原 D46『`eCanTranEnFlag |= 1ul << mbox` 改成 `1ul << id`』与原 D54『`eCanReEnFlag |= 1ul << id` 改成 `1ul << mbox`』**已删除**, 因为它们在宿主上是**恒等变换**。证明: 只有当 mbox >= 32 时 mbox 与 id = mbox & 0x1f 才分道扬镳, 而 MSVC 的移位指令把 32 位操作数的移位计数按 & 31 截断 (`shl eax, cl` 只取 cl 的低 5 位), 于是 `1ul << mbox` 与 `1ul << (mbox & 0x1f)` 在宿主上逐位相同; 实测把 mbox 取遍 0..255、两侧标志取遍 0 / 单比特 / 全 1, 变异前后产物完全一致。**本条不对目标端作任何断言** —— C28x 上移位计数 >= 32 是未定义行为。结论只有一句: 本测试台守不住 InitTranMbox 与 InitRecMbox 在移位操作数上的不对称。另外 D52 原本选的『`dataPi++` 多走一步』也是恒等变换 (自增结果再没人读), 那是**变异选得没意义**, 已换成『数据第二字写错目标』并成功抓到。
  - **测试台的关键设计一: 两侧必须逐调用锁步比对**。InitdspECan 在函数级持有 `static Uint16 con` / `count`, 状态跨调用累积 —— 所以驱动不是把每个函数孤立喂一遍, 而是 ref/new 一次只走一步、**每步之后都比一次**, 由 `struct STEP` 描述这一步的调用与前置条件 (`seed` / `keep` / `cce` / `ccr` / `trs` / `rmp` / `rml` / `poke`), 每步快照 `ret` / 两个使能标志 / 7 个 CPU 控制计数器 / 4 字输出缓冲 / **整个外设映像** (`struct PERIPH { ECAN_REGS; ECAN_MBOXES; LAM_REGS; GPIO_CTRL_REGS; }`)。
  - **测试台的关键设计二: 返回码直方图证明路径可达**。驱动统计 `g_retHist[8][8]` 并在末尾打印, InitdspECan 0/8364/4152/8/12/0, InitTranMbox 4219, InitRecMbox 4300, eCanDataTran 1047/2109/1035, eCanDataRec 0/0/0/2031/1027/1052 —— 『0 处不一致』因此是**每条路径都真的走过**之后的 0, 不是测试台没工作。这张直方图当场抓到一个真实的测试台缺陷: 最初的随机阶段会放单次 InitdspECan 通过, 随机 baud + 随机 CCE 下 `count` 越过 IINIT_CAN_TIME 把 static 永久锁死, 之后任何输入都返回 CAN_INIT_TIMEOUT (ret2 计数 491 暴露了它); 修法是让随机阶段的每次 InitdspECan 都是**完整序列**, 并把系统性的超时阶段放到**最后**。
  - **寄存器按整字快照而非位域**: 外设块按 `.all` 字比对, 所以每一位都被观测到。驱动里还有一条布局断言 —— `sizeof(ECAN_MBOXES) == 32*4*sizeof(Uint32)` 且 `MBOX1 - MBOX0 == 4 字` —— 实测 ECAN_REGS=104 / ECAN_MBOXES=512 / LAM_REGS=128 / GPIO_CTRL_REGS=128, 保证 `MsgCtrlPi[i<<2]` 式的下标访问落在预期的排布上。负向对照里把邮箱号掩码从 0x1f 改成 0x3f 会**越界**踩到测试台自己的全局量, 所以那条改成 `& 0x1e` (清一位, 索引仍合法但访问的邮箱变了)。

| 项 | 结果 |
|---|---|
| 差分用例数 | **29,356** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ECanaLAMRegs`, `_ECanaMboxes`, `_ECanaRegs`, `_GpioCtrlRegs`, `_InitRecMbox`, `_InitTranMbox`, `_InitdspECan`, `_eCanBaud`, `_eCanDataRec`, `_eCanDataTran`, `_eCanReEnFlag`, `_eCanTranEnFlag` |
| `.text` 尺寸 | 604 → 580 (**-24**) |

### `src/motor/MotorPublicCal.c`

- 变换: 与机型/调制方式无关的公共保护与制动模块, 八个入口: RunCaseShortGnd / RunCaseLowPower / CBCLimitCurPrepare / MaxUVWCurrent / HardWareErrorDeal / HardWareOverUDCDeal / RunCaseDcBrake / BrakeResControl。改写手法: (1) 两个 switch 的选择子 `gMainStatus.SubStep` 先读进局部量 `m_subStep`, 分派在快照上 —— case 体里对 SubStep 的写因此不再改变被选中的分支 (原代码 switch 表达式本来就只求值一次, 这是**把隐式的一次求值写成显式**); (2) MaxUVWCurrent 的两级三目取大写开: `m_IU = (m_IU >= m_IV)?m_IU:m_IV;` -> `if (m_IU < m_IV) { m_IU = m_IV; }`; (3) 单 bit 掩码判据 `(all & M) == M` 写成 `(all & M) != 0`; (4) `(gBrake.Flag & 0x01) == 0` 写成 `!(gBrake.Flag & 0x01)`; (5) 去掉注释与被 `/* */` 包起来的旧制动占空比算法死块。**没有**改任何判据常量、寄存器写入值、写入顺序或调用顺序。

- 必须守住的不变量:

  - **八个函数名与四个全局量全部不变**: RunCaseShortGnd / RunCaseLowPower / CBCLimitCurPrepare / MaxUVWCurrent / HardWareErrorDeal / HardWareOverUDCDeal / RunCaseDcBrake / BrakeResControl 与 gDCBrake / gBrake / gShortGnd / gLowPower 名字、类型一字不动。目标端外部符号集合 **34 个逐一相同** (nm2000 实测, target_check.py `符号集合相同: True`), 两侧编译告警各 0 条、无新增无消失 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数 237724 与各分阶段计数逐一相同, mismatches=0**, 见 docs/_difftest/MotorPublicCal.json。
  - **负向对照 98/98 全抓到, 0 BROKEN**, 见 docs/_difftest/_negctl_partial_MotorPublicCal.json。清单原有 100 条, 删掉的 2 条性质**不同**, 分开记: (i) 原 P84『半占空分界 `BrakeCoff <= 50` 改 `< 50`』是**真等价**, 可以证明 —— 该判据只在 BrakeCoff == 50 时分道, 而那一刻第三支给出 `(OnCnt, OffCnt) = (1, (100-50)/50) = (1, 1)`, 第四支给出 `(50/(100-50), 1) = (1, 1)`, 两支产物逐字段相同; c != 50 时改动完全不起作用。属于 (b) 类, 证明后删除。(ii) 原 P50『AIO2 采样循环 `i < 10` 改 `i < 11`』**不是等价, 是测试台在宿主上守不住的洞**, 必须与 (i) 区分开: 若十次读到的 AIO2 不全相同 (例如前 4 次为 1、之后为 0), 10 次与 11 次求和的 `sum < 5` 结论确实会翻转, 所以变异本身是有效变异。宿主上无法覆盖的原因是 `GpioDataRegs.AIODAT.bit.AIO2` 是**普通内存读**, 十次读之间没有任何可拦截的调用点, 驱动只能把 AIO2 钉成常量 (0 或 1, 见 phase_hw_error 的 `aio[] = {0,1}`), 于是 sum 恒为 0 或 10, 落在 5 的同一侧。**本条不对目标端作任何断言**; 原文注释『10 次读在 2us 内完成, 母线电压肯定不变』说明设计上假定十次读数相同 —— 在那个假定下该变异不可观测, 但那是硬件假定, 不是宿主可证的等价性。处置沿用 f_dspcan 用例的先例: 记为已记录的覆盖缺口并删除, 不伪装成『抓到』, 也不喂不可达状态去凑覆盖。
  - **`.text` 751 -> 745 (-6) 字**, 逐函数归属 (build/_funcsizes.py, nm2000 相邻符号地址差分): _RunCaseShortGnd +3 / _RunCaseLowPower -4 / _MaxUVWCurrent -1 / _HardWareErrorDeal -4。RunCaseShortGnd 的 +3 是 `m_subStep` 局部量带来的栈槽与装载 (该函数由 2 ms 任务驱动, 非中断路径); 其余三个函数的缩小来自三目改 if 与掩码判据简化后省下的重复比较。**没有任何一个函数被放进中断路径** —— 本次改写的八个函数在改动前后都由主循环 / 2 ms 任务调用, 中断时序未触及。
  - **EALLOW/EDIS 配对与 EPwm 寄存器写序不变**: RunCaseShortGnd case1 仍是 TBPRD -> CMPA/CMPB -> AQCSFRC(1/2/3) -> DBCTL(1/2/3); case3 的 CMPA 更新仍在 EALLOW/EDIS 内; case4 仍是 DBCTL 恢复 0x0007 -> AQCSFRC 清 0 -> TZCLR.INT。CBCLimitCurPrepare 仍是 TZFLG.CBC 判 -> TZCLR.CBC 清 -> Flag 置位, U/V/W 顺序不变。
  - **掩码判据等价性有据**: `(gError.ErrorCode.all & ERROR_LOW_UDC) == ERROR_LOW_UDC` 改成 `!= 0` 等价, 因为 ERROR_LOW_UDC = `1L<<(4-1)` (MotorInvProtectInclude.h:16) 是单 bit 掩码; 同理 ERROR_OVER_CURRENT = `1L<<(1-1)` (同文件:13), 所以 HardWareErrorDeal 里 `(all & ERROR_OVER_CURRENT) != ERROR_OVER_CURRENT` 改成 `== 0` 也等价。若哪天这两个宏被改成多 bit, 这两处必须一并回改 —— 由负向对照 P 系列的单 bit 掩码变异守住。
  - **TMS320F28035 已定义**, 所以 HardWareErrorDeal 走 AIO2 采样支路 (循环 10 次求和, `sum < 5` 判过压 -> HardWareOverUDCDeal 并提前 return), `#else` 支路不参与编译; 该 `#else` 支路原样保留在源码里, 没有删除。
  - **所有算术表达式保持原有类型宽度** (Uint / Ulong / int / int16), 未引入窄化中间量; `((Ulong)m_BrakeCur * (Ulong)gMotorExtPer.R1) >> 16` 与 `((long)gOutVolt.Volt << 16)` 一字未动。

| 项 | 结果 |
|---|---|
| 差分用例数 | **217,924** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_BrakeResControl`, `_CBCLimitCurPrepare`, `_CpuTimer1`, `_EPwm1Regs`, `_EPwm2Regs`, `_EPwm3Regs`, `_GpioDataRegs`, `_HardWareErrorDeal`, `_HardWareOverUDCDeal`, `_MaxUVWCurrent`, `_PID`, `_RunCaseDcBrake`, `_RunCaseLowPower`, `_RunCaseShortGnd`, `_gADC`, `_gBrake`, `_gBuffResCnt`, `_gCBCProtect`, `_gComPar`, `_gDCBrake`, `_gError`, `_gExcursionInfo`, `_gIUVWQ24`, `_gInvInfo`, `_gLineCur`, `_gLowPower`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtPer`, `_gOutVolt`, `_gPhase`, `_gShortGnd`, `_gUDC`, `_gVFPar` |
| `.text` 尺寸 | 751 → 745 (**-6**) |

### `src/motor/MotorSpeedCheck.c`

- 变换: 异步机/永磁机共用的**转速搜索**状态机, 六个入口: PrepareForSpeedCheck / RunCaseSpeedCheck / SpeedCheckSUB1 / SpeedCheckUdcRiseDeal / SpeedCheckCycleOver / SpeedCheckCycleSUB2。改写手法: (1) 三目赋值展开成 if/else —— 第 49 行 `SpeedCheck = (SpeedLast<0) ? (-SpeedMaxPu) : SpeedMaxPu` 与第 54 行的 Speed5hz 版本各展开成一对分支; (2) 单 bit 位域的等号判据改写成非零判据: `Case4Sig1.bit.CHECK_GET == 1` -> `!= 0` (第 104 行), `Case4Sig2.bit.DelayOver == 1` -> `!= 0` (151), `AlmostOver == 1` -> `!= 0` (162); (3) 赋值语句的列对齐多空格去掉。**没有**改任何判据常量、写入字段集合、写入顺序、调用顺序; `#if 0` 包起来的『按电流反算 CurDelta』那一支**原样保留**在源码里 (它决定了第 145 行恒执行 `CurDelta = 5125`, 见下面 invariants 里 S64 那条)。

- 必须守住的不变量:

  - **六个函数名与四个全局量全部不变**: PrepareForSpeedCheck / RunCaseSpeedCheck / SpeedCheckSUB1 / SpeedCheckUdcRiseDeal / SpeedCheckCycleOver / SpeedCheckCycleSUB2 与 gFeisu / gFeisuNew / gSpeedCheckPID / gSpeedCheckPID1 名字、类型一字不动。目标端外部符号集合 **26 个逐一相同** (nm2000 实测, target_check.py `符号集合相同: True`); 两侧编译告警各 1 条且相同 (`function "CalTorqueUp" declared implicitly`, 原工程就有的), 无新增无消失 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, 两边用例数 33909 与各分阶段计数逐一相同, mismatches=0**, 见 docs/_difftest/MotorSpeedCheck.json。分阶段: 边界相 1259+1008 (B1~B9) / 状态机序列 18 组共 7650 / 随机混合 20000。
  - **负向对照 131/131 全抓到, 0 BROKEN**, 见 docs/_difftest/_negctl_partial_MotorSpeedCheck.json。**这是本模块唯一一次真正意义上的失败与修复**, 经过值得完整记下来, 因为它同时暴露了测试台的两类缺陷。
  - **缺陷一: 测试台的随机数发生器退化, 让一半被测代码成了死代码。** 最初 135 条里 74 条 MISSED, 其中 S37~S101 是一整段**连续**的变异, 由 build/_negctl_diag.py 定位到全部从基准第 118 行开始。根因是驱动的 `rnd()` 写成 `g_rng * 6364136223846793005UL + ...` 再 `>> 33`: 宿主上 `unsigned long` 是 **4 字节** (实测 sizeof == 4), 移位计数 33 越过宽度是未定义行为, MSVC 发出 `shr eax, cl` 而 x86 把 cl 掩到 5 位, 于是 `>> 33` 实际等价于 `>> 1` —— 取到的正是 LCG 最低的几位。更糟的是 LCG 第 k 位的周期只有 2^(k+1), 而 phase_random 的循环体每次恰好消耗 **40** 次 rnd() (40 ≡ 0 mod 8), 于是同一个调用点每次都落在同一相位、拿到的值恒等 mod 4。`Case4Sig1.bit.CHECK_GET` 就是 bit 1, 所以它在随机相里**永远是 1**, RunCaseSpeedCheck 每次都从『已找到转速』那一支 (基准第 104~126 行) 提前 return, 基准第 **129~345 行**在差分测试里成了死代码 —— 而『0 处不一致』看上去依然完美。修法是把 rnd() 换成 32 位 LCG + xorshift/乘法混合, 并且**把可达性变成会失败的断言**: do_step 里统计两条主干各走了多少次, main 末尾 `g_get0 == 0 || g_get1 == 0` 就 FATAL 退出码 2。修完后 CHECK_GET 覆盖 ==0 **11530** 次 / ==1 **2709** 次。
  - **缺陷二: 驱动扫的量不是被测代码真正比较的量 (两处, 教训同源)。** (i) 基准第 149 行 `gFeisu.VoltCheck = (gSpeedCheckPID.Out>>16)` 把驱动写进去的 VoltCheck 无条件覆盖掉, 而 VoltCheck 恰好是第 167 行 (AlmostOver 判据) 与第 179 行 (m_UData 判据) 的**边界输入** —— 所以 B3/B4 里直接写 `gFeisu.VoltCheck` 是装饰, S61 (`m_UData > VoltCheckAim` 改 `>=`) 因此 MISSED。修法是给 PID 替身加一个 `gPidOutForce` 注入口 (见 mock_speed_check.c 的注释), 驱动用 `value << 16` 把 VoltCheck 钉在指定值上; B4 再按 `ceil(4096<<9 / ((|SpeedCheck|>>8)+512))` 反解出恰好让 m_UData == 4096 的 VoltCheck, 九个 SpeedCheck 值都给出干净的 4095/4096/4097 三连。(ii) 基准第 145 行 `gFeisu.CurDelta = 5125;` 同样把驱动的 CurDelta 覆盖掉 (`#if 0` 那一支才是按电流反算的), 于是 CurDelta 在 RunCaseSpeedCheck 里是**只写不读的输入**; 第 191 行真正比较的是 `CurPer + 1000 < 5125`, 而 S64 只把 1000 改成 1001, 所以**分道点只有一个: CurPer == 4124**。B5 原来扫 CurDelta ∈ {999,1000,1001} 完全扫错了量, 修成扫 CurPer ∈ {0,4123,4124,4125,5125}。这两条与 MotorInfoCollect 的 `tot[e] - Iu` 是同一条纪律: 要扫**被测代码真正比较的那个派生量**, 不是它上游的原始输入。
  - **同样加了会失败的覆盖断言**: 除 CHECK_GET 那两个计数器外, B5 还把第 191 行的落点断言下来 —— `CurPer == 4124` 时 ref 必须走 if 支 (CsrWtOver 保持 0), `CurPer == 4125` 时必须走 else 支 (CsrWtOver 被置 1), 落点不对就 FATAL。这条断言的价值在于它**钉住了第 145 行 `CurDelta = 5125` 这个前提**: 一旦有人把 `#if 0` 打开, 断点立刻失败, 而不是让 S64 悄悄退回 MISSED。实测输出『落点正确 288 次, 落点异常 0 次』。
  - **删除的 4 条变异 (S119/S121/S123/S124) 是 (b) 类真等价, 附证明**: 它们把 SpeedCheckUdcRiseDeal 四个钳位判据的等号侧改掉 (`>` 改 `>=`, `<` 改 `<=`)。`>` 与 `>=` 只在两侧**相等**时分道, 而此刻分支体做的正是 `SpeedCheck = <被比较的那个值>` —— 把变量赋成它自己, 一个 no-op。分支链的跳过后继也补不上: 掩码 == 0x4000 时第 3/4 分支要求掩码 != 0x4000, 接不上, 而第 1/2 分支互斥于 SpeedLast 的符号, 也接不上 (前两条**无条件**等价); 掩码 != 0x4000 时第 3/4 分支互为『SpeedMaxPu < 0』这一个条件, 由系统不变量 `gBasePar.FullFreq = gBasePar.MaxFreq + 20*si2puCoeff` (MotorParChange.c, si2puCoeff ∈ {1,10,100}) 给出 FullFreq > MaxFreq, 于是 `SpeedMaxPu = ((Ulong)MaxFreq<<15)/FullFreq < 32768` 即恒 >= 0, 两个分支永远同时为假 (后两条在可达状态空间上等价)。处置是**删掉并写下证明**, 不是去喂不可达状态凑覆盖。测试台这一侧的对应改动: phase_random 现在按同一条不变量生成 FullFreq (= MaxFreq + rnd(1,200)), 不再制造 FullFreq <= MaxFreq。
  - **每个字段都比**: 两侧私有 gFeisu / gFeisuNew / gSpeedCheckPID / gSpeedCheckPID1 整结构体比对, 共享的 gMainCmd / gMainStatus / gOutVolt / gLineCur / gError / gComPar / gUDC / gInvInfo / gMotorExtPer / gBasePar / gIMTSetApply / gCtrMotorType / gMotorInfo 与 GpioDataRegs 也整块比 —— 少写一个字段、把 UdcOld 与 UdcBak 写反、把 StatusWord 的位写错, 只有这样才看得见。**调用日志按序列比对**: CalOutVotInVFStatus 在本模块里被调用三次且三次实参不同 (gFeisu.SpeedCheck / gMainCmd.FreqSyn / gFeisu.SpeedCheck), PID 的入参结构体也被逐次记入日志并比对, 所以『传错了哪一个』是可观测的。
  - **`.text` 743 -> 726 (-17) 字**, 逐函数归属 (build/_funcsizes.py, nm2000 相邻符号地址差分): _PrepareForSpeedCheck +2 / _RunCaseSpeedCheck -9 / _SpeedCheckUdcRiseDeal -10。**没有任何一个函数变大到值得担心** —— 三个函数都由主循环调用 (RunCaseSpeedCheck 由 MainStateMachine 驱动, 不在中断里), +2 字的量级在 2 ms 预算里可忽略; 中断路径未触及, 不需要重测时序。
  - **所有算术表达式保持原有类型宽度** (Uint / Ulong / int / Uint16), 未引入窄化中间量; `((Ulong)m_UData * (Ulong)gFeisu.VoltCheck)>>9`、`(((Ulong)gBasePar.MaxFreq)<<15)/gBasePar.FullFreq`、`(500L<<15)/gBasePar.FullFreq01` 一字未动。
  - **已知的宿主/目标差异 (与比对有效性无关, 但对结论有边界)**: 宿主的 `int`/`Uint` 是 32 位而 C28x 上是 16 位, 所以第 178 行 `m_UData` 的 32 位乘积在目标端会截回 16 位, 宿主不会; 第 35 行 SpeedMaxPu 的 32 位商在目标端赋给 int16 时也会截断。两侧同为宿主, 比对本身仍有效, 但这**不是**对 C28x 截断点的断言。

| 项 | 结果 |
|---|---|
| 差分用例数 | **33,909** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CalOutVotInVFStatus`, `_CalTorqueUp`, `_GpioDataRegs`, `_PID`, `_PrepareForSpeedCheck`, `_RunCaseSpeedCheck`, `_SpeedCheckCycleOver`, `_SpeedCheckCycleSUB2`, `_SpeedCheckSUB1`, `_SpeedCheckUdcRiseDeal`, `_gBasePar`, `_gComPar`, `_gCtrMotorType`, `_gError`, `_gFeisu`, `_gFeisuNew`, `_gIMTSetApply`, `_gInvInfo`, `_gLineCur`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtPer`, `_gOutVolt`, `_gSpeedCheckPID`, `_gSpeedCheckPID1`, `_gUDC` |
| `.text` 尺寸 | 743 → 726 (**-17**) |

### `src/motor/MotorCarrier.c`

- 变换: 载波频率与 PWM 角度 / 死区补偿, 六个入口: CalCarrierWaveFreq / AsynPWMAngleCal / SynPWMAngleCal / SoftPWMProcess / CalOutputPhase / CalDeadBandComp。改写手法: (1) 四处 `m_MaxFc = (m_MaxFc>N)?N:m_MaxFc;` 的三目钳位展开成 `if (m_MaxFc > N) { m_MaxFc = N; }`; (2) `gFcCal.FcBak = (gFcCal.FcBak > gBasePar.FcSet) ? gBasePar.FcSet : gFcCal.FcBak;` 与 `(gFcCal.FcBak<10) ? 10 : gFcCal.FcBak` 同样展开成 if; (3) 同步/异步分派 (`ModuleApply == 1` -> `!= 1`) 与 AsynPWMAngleCal 的分频判据 (`FcApply > 61440` -> `<= 61440`) 都**倒置**, 分支体随之互换; (4) SoftPWMProcess 的 `m_Coff = 3277; if (30 < FcSetApply) m_Coff = 1966;` 合成一个三目, `gPWM.SoftPWMCoff = gPWM.SoftPWMCoff + 29517*GetTime();` 写成 `+=`; (5) SynPWMAngleCal 的频点表扫描由 `while (AbsFreq >= T[m_Index]) { m_Index++; if (m_Index >= 10) break; }` 改成把上界并入循环条件 `while ((m_Index < 10) && (AbsFreq >= T[m_Index]))`; (6) CalDeadBandComp 的分段判据倒置成 `FreqReal > 40000` 先行; (7) CalOutputPhase 去掉空的 `else { ; }` 分支。**没有**改任何判据常量、写入字段、写入顺序或调用顺序; 两处 `DINT ... EINT` 之间的语句一条没动、一条没挪。**刻意保留**了两处原样死代码, 并在这里记下来而不是顺手清掉: 末尾 `#if 0` 的三个空死区函数, 以及 CalDeadBandComp 里 `#if 1/#else` 的备用死区算法 (与 MotorSpeedCheck 保留 `#if 0` 那一支同一处置)。另外 `if (gInvInfo.InvTypeApply >= 19) { m_TempLim = 70; }` 是一个**恒等分支** (两支都赋 70), 同样**原样保留**: 删掉它会让'以后有人把上一行改成别的值'这件事改变含义 —— 留着是原行为的一部分, 不是没看见。

- 必须守住的不变量:

  - **六个函数名与全部文件级符号不变**: 四个全局量 (gFcCal / gSynPWM / gPhase / gDeadBand) 与三张 `Uint/Ulong const` 表 (gSynFreqTable / gSynNcTable / gSynStepAngleTable) 名字、类型、初值一字不动。目标端外部符号集合 **34 个逐一相同** (nm2000 实测, target_check.py `符号集合相同: True`), 两侧编译告警各 1 条且相同 (`constant is too large for long; interpreting as unsigned long type`, 来自 gSynStepAngleTable 的 4294967295, 原工程就有), 无新增无消失 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, mismatches=0**, 用例数 32441, 见 docs/_difftest/MotorCarrier.json。分阶段 (从台账 stdout 读出, 不手抄): CalCarrierWaveFreq 边界 8516 / AsynPWMAngleCal 边界 351 / SynPWMAngleCal 边界 2025 / SoftPWMProcess 边界 543 / CalOutputPhase 边界 274 / CalDeadBandComp 边界 732 / 随机 20000。各分阶段计数在改写前后**逐一相同**。
  - **负向对照 106/106 全抓到, 0 BROKEN, 0 HUNG**, 见 docs/_difftest/_negctl_partial_MotorCarrier.json。按函数分组: CW 打 CalCarrierWaveFreq (温度迟滞前后沿与步长、过载双窗口、机型/母线电压分档、FcSetApply ±1 步进、同步-异步锁存三个门限、CPWM/DPWM 选择), CA 打 AsynPWMAngleCal (分频判据与两条移位、半周期分母、角度步长系数与右移、正反向补偿角、DINT-EINT 内的写入), CS 打 SynPWMAngleCal (频点表扫描上界与判据、升降档回差与标志、载波追赶步长、同步步长表与补偿角), CP 打 SoftPWMProcess (五条早退判据、两档系数、调制度定标、时间漂移系数、周期与步长补偿右移), CO 打 CalOutputPhase (机型/PGMode/旋变/辨识步四条判据、位置来源字段、零位符号、位置定标、输出相位), CD 打 CalDeadBandComp (分段判据与衰减、电流滤波、直流制动清零、角度滤波、三相补偿符号与写错相)。另有 7 条 (CW11 / CW24 / CW27 / CW29 / CW30 / CW35 / CW39) 被**证明在可达状态空间里处处等价**后按纪律从清单删除 (不是'测不到', 而是'在固件能走到的状态上不产生分歧'), 逐条证明见 build/_mc_equiv_proofs.md, 删除动作由 build/_mc_drop_equiv.py 执行并复核。原始清单 114 条 -> 现 106 条。
  - **`.text` 925 -> 932 (+7) 字**, 逐函数归属 (build/_funcsizes.py, nm2000 相邻符号地址差分): _CalCarrierWaveFreq -1 / _SynPWMAngleCal +6 / _SoftPWMProcess +2; 另外三个函数 (AsynPWMAngleCal / CalOutputPhase / CalDeadBandComp) **一字不差**。+6 的来源是频点表扫描: 原写法的 `break` 让 C28x 生成一个更紧的循环, 把上界并入循环条件后多一次比较与一次跳转。**六个函数全部由 2 ms 任务 Main2msMotorB 调用, 不在任何中断路径上**, +7 字 / 约 7 个周期在 2 ms 预算里可忽略; 中断时序未触及, 不需要重测。
  - **宿主不可表示区 (一): 整数除零 —— 分类而不是排除**。SynPWMAngleCal 末尾的 `m_HalfTc = m_LData / m_Long` 里, `m_Long = (((FullFreq01 * abs(FreqSyn)) >> 7) * gSynNcTable[Index] / 100) >> 8` 在 abs(FreqSyn) 低于一个与 FullFreq01 有关的地板时就是 0 (FullFreq01 == 2001、Index == 0 时约 21), 而 FreqSyn == 0 是**可达的**: MotorMain.c 的 DC_CONTROL 与 default 两支都把 gMainCmd.FreqSyn 置 0, 同时 gMainCmd.Command.bit.ControlMode 仍留在 IDC_VF_CTL —— 正是 CalCarrierWaveFreq 把 ModuleApply 锁存成 1 所需的那组条件。宿主上整除零会抛 STATUS_INTEGER_DIVIDE_BY_ZERO 直接杀死进程, 目标端 C28x **不陷阱** (运行时除法例程返回未指定值, 固件带着垃圾 gPWM.gPWMPrd 继续跑)。这不是改写引入的: 除数表达式两侧逐字相同, 原工程也逐字相同。处置是**分类而不是排除** —— 驱动用 SEH `__try/__except` 只捕 EXCEPTION_INT_DIVIDE_BY_ZERO (其它异常照旧 EXCEPTION_CONTINUE_SEARCH 放行, 因为那是真缺陷), 两侧同时陷阱就计数跳过, **只有一侧陷阱则报 MISMATCH** (宿主上'是否陷阱'本身就是可观测行为)。实测 n=20000 里 51 例, 全部是 SynPWMAngleCal 且 abs(FreqSyn) ∈ {0,1,2}, 两侧一致。**机制是实测选的, 不是猜的**: 先用三行探针 (build/_fpetest.c) 验过 MSVC 的 CRT **不会**把 STATUS_INTEGER_DIVIDE_BY_ZERO 翻成 SIGFPE —— signal(SIGFPE) 装了也不进处理函数, 进程照样死; 换成 `__try/__except` 才真的接住 (build/_fpetest2.c)。输入覆盖因此没有被削减, 跳过的条数打印在汇总里, 改写若把陷阱挪了位置也藏不住。
  - **宿主不可表示区 (二): 频点表下标下溢 —— 这一处是限制, 附证明**。SynPWMAngleCal 的下标扫描对 AbsFreq < gSynFreqTable[0] == 75 时循环体一次都不进, `m_Index--` 从 0 回卷成 65535, 紧接着的 `gSynFreqTable[gSynPWM.Index + 1]` 在 gSynPWM.Index == 9 时读到数组尾后一格。**目标端无害, 原因是布局**: .econst 里 gSynFreqTable 紧挨 gSynNcTable 且都是 `Uint const`, 所以 gSynFreqTable[10] 就是 gSynNcTable[0] == 80, 判据 `AbsFreq > 90` 对 AbsFreq < 75 不成立, gSynPWM.Index 原封不动 —— 表里本来就没有低于 75 的项, 下溢没有可去之处。**宿主端两数组是两个独立全局**, gSynFreqTable[10] 是未定义值; 一旦够大就会把 Index 写成 65535, 再拿它索引 gSynNcTable 就是读 131070 字节外的访问违例。而且这是**布局相关**的危险: 今天过、换个无关的改动重新链接后崩, 所以必须挡在门外。可达性要如实说: 锁存在 85 以上才置 1、只在 65 以下才清 0, 所以 ModuleApply == 1 配 AbsFreq ∈ [65,74] 是**可达**的, 而它正是调用 SynPWMAngleCal 的那条路径 —— 因此这一区**不是**因为不可达才排除, 是因为宿主表达不了; 表里缺 75 以下的项是原设计的一处既有短板 (目标端无害), 两侧逐字相同。结论按原话说清: **AbsFreq < 75 时 SynPWMAngleCal 的行为本测试台没有验证**, 75 及以上全部验证过。带内只喂 ModuleApply == 0, 75..85 照常两个值都喂。
  - **测试台设计: 逐字段比对 + 可观测的副作用**。(i) 模块自有四个全局量由 ref_tu.c / new_tu.c 改名成两套独立实例, 直接整结构体 memcmp; 它**借用**的全局量 (gBasePar.FcSetApply / gPWM / gIPMPos.RotorPos / gIPMPos.ABZ_RotorPos_Ref / gIAmpTheta.ThetaFilter) 也**每侧各拍一份快照**再比 —— 否则一侧少写、另一侧后跑把它覆盖回去, 就看不见了。(ii) 两处 `DINT ... EINT` 在 shim 里变成 `(gHostCpuCtl[n]++)`, 七个计数器逐侧比对, 所以丢掉、颠倒或挪动这对配对是**可观测**的而不是隐形的。(iii) CalOutputPhase 只在 (PM 机型, PGMode == 0) 一支里调用 SynCalRealPos(), 真身住 MotorPmsmMain.c 会把整条 PMSM 栈拖进来, 所以用带调用日志的替身 (gSynCalPosLog / gSynCalPosCount) —— '调用了几次'因此可观测。(iv) 每条输入都非平凡预置: `dirty` 式预填 + 每个字段都写, 免得'两侧都还停在 0'冒充'一致'。(v) 32 位 LCG + xorshift 混合后再 `>> 8`, 避免 MotorSpeedCheck 那次低比特退化 (LCG 第 k 位周期 2^(k+1)) 重演。
  - **所有算术表达式保持原有类型宽度** (Uint / Ulong / Ullong / int), 未引入窄化中间量; `((Ullong)m_AbsFreq * (Ullong)gBasePar.FullFreq01 * 439804651L / m_Fc)>>16`、`(((Ullong)m_Long<<9))/100`、`((long)m_Com * gDeadBand.InvCurFilter)>>9` 一字未动。

| 项 | 结果 |
|---|---|
| 差分用例数 | **32,441** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AsynPWMAngleCal`, `_CalCarrierWaveFreq`, `_CalDeadBandComp`, `_CalOutputPhase`, `_CpuTimer1`, `_SoftPWMProcess`, `_SynCalRealPos`, `_SynPWMAngleCal`, `_gBasePar`, `_gCtrMotorType`, `_gDeadBand`, `_gExtendCmd`, `_gFcCal`, `_gGetParVarable`, `_gIAmpTheta`, `_gIPMPos`, `_gIPMZero`, `_gInvInfo`, `_gMainCmd`, `_gMainStatus`, `_gMotorInfo`, `_gOutVolt`, `_gOverLoad`, `_gPGData`, `_gPWM`, `_gPhase`, `_gPmParEst`, `_gRotorTrans`, `_gSubCommand`, `_gSynFreqTable`, `_gSynNcTable`, `_gSynPWM`, `_gSynStepAngleTable`, `_gTemperature` |
| `.text` 尺寸 | 925 → 932 (**+7**) |

### `src/function/f_ui.c`

- 变换: 键盘/显示刷新, 十个入口: UpdateDisplayBuffer / WinkDeal / DisplayScanPrepare / DisplayScan / KeyScan / KeyBordTest / KeyProcess / InitSpiaGpio / InitSetSpiForFunc / UpdateMenuModeDisplayBuffer。改写手法: (1) DisplayScanPrepare 里 `spiData.in.key >> 7` 被读两次 (一次写 keyValue, 一次判 keySelType), 提成一个具名局部量 `m_keyIn` —— **必须**放在 RX 循环之后: `spiData.in` 与 `spiData.out` 虽是 struct 的两个成员 (不是 union), 但 RX 循环里 `spiData.in.all[j] = SpiaRegs.SPIRXBUF;` 会改写整个 `spiData.in` (含 `.key`), 提到循环之前就变成了读旧值; (2) 两处三目赋值展开成 if/else —— `(!(spiData.in.key >> 7)) ? (keySelType = 0) : (keySelType = 1);` 与 `(tcnt >= KEY_SEL_TEST_TIME) ? (tcnt = 0) : (tcnt++);`; (3) 单比特判据 `== 1` 改成 `!= 0` (keyChecking / keySelType); (4) `if (!x)` 改成 `if (x == 0)` (errorCodeOld / scanStep / keySelStatus / tcnt / keyChecking / bLight[scanStep] / bKeyEsc / bKeyProc / keyTemp.funcCnt / keyPreCtrl); (5) DisplayScanPrepare 里 8 行制表符缩进改 4 空格; (6) `[menuModeTmp-1]`、`(1<<(...))` 补齐运算符两侧空格。**没有**改任何判据常量、写入字段、写入顺序或调用顺序。

- 必须守住的不变量:

  - **十个函数名与全部文件级符号不变**: 两个 `Uint16` 全局量 (flashMaxSDSD / scanStep 等)、三张 `const Uint16` 表 (DISPLAY_CODE / LED_CODE / dispCodeMenuMode)、`minRate[]` / `repeatNumMax[]`、`struct SPI_IN_OUT spiData` 名字、类型、初值一字不动。目标端外部符号集合 **71 个逐一相同** (nm2000 实测, target_check.py `符号集合相同: True`), 两侧编译告警各 0 条, 无新增无消失 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, mismatches=0**, 用例数 84331, 见 docs/_difftest/f_ui.json。驱动共 11 相 —— 10 个入口各一相 (phase_display_buffer / phase_wink_deal / phase_scan_prepare / phase_display_scan / phase_key_scan / phase_key_bord_test / phase_key_process / phase_init_spia_gpio / phase_init_spi_func / phase_menu_mode_display) 加 1 相随机 (phase_random); 带文件级静态量的入口按**序列**走 (WinkDeal 135 次 / DisplayScanPrepare 790 次连续调用), 每步都比对。驱动只在末尾打印总账 (cases / mismatches), **分相计数没有落进台账** —— 所以这里不列分相数字: 台账里没有的数不手抄。
  - **负向对照 52/52 全抓到, 0 BROKEN, 0 HUNG**, 见 docs/_difftest/_negctl_partial_f_ui.json。按函数分组: FU1..FU12 打 UpdateDisplayBuffer (运行/本地/方向/调谐四条 LED 位、本地源三态分派、自动复位三元判据、扫描步与菜单模式两支); FU13..FU24 打 WinkDeal (键检早退、显示码取错下标、数字闪烁门限与位下标、LED 边沿检测、故障快闪上限与取错 LED 位); FU25..FU31 打 DisplayScanPrepare / DisplayScan (键值右移、位段选择下标、键选边沿判据、扫描回卷上界、SPI 发送取反、KEYSEL 取错变量); FU32..FU40 打 KeyScan (键值左移、多键判据、四条 keyAge 门限、进位步长与档位、重复次数); FU41..FU45 打 KeyBordTest / KeyProcess (键盘测试位下标、点动保持判据、快捷菜单超时、按键功能传递、SHIFT 置位); FU46..FU52 打两个 init 函数与 UpdateMenuModeDisplayBuffer (GPIO 复用/上拉/量化、SPI 波特率与字符长度、菜单显示下标与列)。
  - **测试台补洞 (这一条是本模块最有价值的产出 —— 两轮 MISSED, 两个不同的洞)**: 首轮跑到 FU33 就报 2 条 MISSED + 1 条 BROKEN; 补完第一个洞后跑满 52 条, **又冒出 7 条 MISSED**。两次都不是被测代码的问题。
(1) BROKEN: FU8 的替换串写成不存在的 `RUN_MODE_VF_CTRL`, 改成真实存在的 `RUN_MODE_POSITION_CTRL` (`include/function/f_frqSrc.h` 里只有 SPEED / TORQUE / POSITION 三个)。
(2) 第一个洞 —— 输入注入次序。FU20 (数字闪烁门限 `>= 42` -> `> 42`) 与 FU23 (故障快闪上限 `flashMaxSDSD` -> `+1`) MISSED。根因在 `driver.c`: 三处序列相 (phase_wink_deal 两段、phase_scan_prepare 一段) 把 `install_side()` 写在 `inSide.*` 赋值**之前**, 而每个用例开头 `case_begin()` 会重新随机化 `inSide` —— 于是 `install_side()` 装进去的是随机预填值, 不是 `scanStep = ss` / `bEnflash = FLASH_YES`。实测证据 (`build/_probe_fui.py` 往变体 driver 里插 printf): 六轮序列装进去的 scanStep 是 4/1/2/0/0/5、bEnflash 是 1/1/0/0/1/1, 闪烁计数器 **一次都没递增过**, 一直停在预填值。修法是把 `install_side()` 挪到输入钉死之后。
(3) 第二个洞 —— 键值构造反了, 于是判据不可达。修完次序跑满 52 条, FU34..FU40 (KeyScan 状态机里四条 keyAge 门限 `>= -> >`、`upDownDelta *= 10` 进位、`repeatStep < 2` 档位上限、`repeatNum >= repeatNumMax[]`) 全部 MISSED。根因在 `phase_key_scan` 的 P5b, 而且**有两层**:
    第一层是键值方向。KeyScan 从 keyValue 的**补码**取键号: `keyValueTmp = ((~keyValue) & ((1 << KEY_NUM) - 1)) << 1;`, 所以 `funcVal == j` 意味着 keyValue 的第 (j-1) 位是 **0**。测试台原先写的是 `keyValue = 1U << (KEY_UP - 1)`, 恰好是反的 —— 它把第 (KEY_UP-1) 位置 1, 于是 keyValueTmp 里 8 个位被点亮, funcCnt 算出 8 而不是 1, `if (keyTemp.funcCnt > 1) bKeyEsc = 1;` 直接吃掉整个调用, switch 一次都没进。修法是加一个 `KEYV_ONE(k) = (((1U << KEY_NUM) - 1) ^ (1U << ((k) - 1)))` 宏 (全 1 只挖掉第 k-1 位), 把「键值 -> 键号」的方向写成一句可复用的定义, 免得下一相再写反。
    第二层是 (keyPreFunc, keyPreCtrl) 组合。`kc` 原先只给了 (0,0) / (0,1) / (KEY_UP,7) 三组, 而进入 `switch (keyStatus)` 的条件链恰好要求 **keyPreFunc == funcVal 且 keyPreCtrl == 0** (`ctrlKeyFlag == 0` 让 ctrlVal 恒为 0, 所以 keyPreCtrl 非零就会被"来了另一个键"那一支拦下并重置 keyAge)。三组全都不匹配。修法是给 `kc` 加第四组 `(KEY_UP, 0)`。
    两层必须同时修: 只加第四组而不改键值方向, funcVal 是 8 而 keyPreFunc 是 9, 照样进不去 switch —— 这正是第一轮修完仍然 7 条 MISSED 的原因。记录这一点是因为它是本次最有价值的教训: **要把变异锚在代码真正比较的那个量上 (funcVal), 而不是它的上游原始输入 (keyValue)**; 只盯着原始输入, 就会把"方向搞反"误判成"覆盖不够"。
两轮补完, 用例数从 77931 涨到 84331。**这正是 README 说的 (a) 类 MISSED: 测试台有洞就补覆盖, 而不是去改被测代码**; 而且第一个洞说明 "同一份代码两侧对跑 0 不一致" 只证明测试台**不报假警**, 证明不了它**覆盖到了** —— 覆盖必须靠负向对照一条条逼出来。
  - **`.text` 1167 -> 1166 (-1) 字**, 目标端实测 (target_check.py / nm2000)。十个函数名、调用次序、`#if DSP_2803X` 分支结构全部保留。**十个函数全部由 1 ms / 后台任务调用, 不在任何中断路径上** (本模块无 `interrupt void`), 少 1 字对时序没有影响, 不需要重测最坏执行时间。
  - **刻意不做的事**: (i) 不把 `displayBuffer[5]` 或 `menuAttri[menuLevel]` 提成局部量 —— 前者在函数中段被 `DispDataDeal()` 之外的可能路径读到, 后者是文件级全局数组元素; C28x 上 `displayBuffer[5]` 是绝对地址直接寻址 (一条指令), 换局部量要么多一次写回、要么改变 `DispDataDeal()` 看到的中间值, 两者都不是"功能不变" (与 MotorSpeedCheck 里 `m_pFeisu` 的取舍同一处置, 见 build/_write_speedcheck.py 的说明); (ii) 不删 WinkDeal 里第 307 行与第 311 行重复的 `winkFlagLedOld = winkFlagLed1;` (原工程就有, 删掉对产物零影响, 保留才是"功能不要变"); (iii) 不动 `#if 0` 的 flashTickerMax 数组死分支, 也不动 `#if DSP_2803X ... #else` 的备用分支。
  - **宿主不可表示区**: 本模块是纯逻辑 + 位运算, 没有除零、没有指针下溢、没有宿主/目标宽度差会引起分歧的表达式。所有算术保持原有类型宽度 (Uint16 / int16), `keyValueTmp = ((~keyValue) & ((1 << KEY_NUM) - 1)) << 1;`、`0x7F & (~(1U << (5 - scanStep)))`、`(0x01U << (7 - scanStep))` 一字未动。`m_keyIn` 的提取也保持了 `Uint16` 宽度 (不是 int)。
  - **测试台设计: 逐字段比对 + 可观测的副作用**。(i) 模块自有状态 (spiData / diHwStatus / displayBuffer / bLight / flashTicker / flashTickerLed / 整个按键状态机 / scanStep / keySel* 组) 由 ref_tu.c / new_tu.c 改名成两套独立实例, 逐字段快照后比对。(ii) 它**借用**的 21 个外部全局量每侧各拍一份快照再比 —— 否则一侧少写、另一侧后跑把它覆盖回去, 就看不见了。(iii) 三个外设寄存器块 (SpiaRegs / GpioCtrlRegs / GpioDataRegs) 由驱动扮演芯片: 每次调用前装一整块寄存器映像, 调用后整块快照回来比对, 所以"改写后不再写 SPIBRR"或"往 GPAMUX2 写了错位"都是可观测的。(iv) `EALLOW` / `EDIS` 在 shim 里变成 `(gHostCpuCtl[7]++)`, 逐侧比对 —— 丢掉或颠倒这对配对是可观测的而不是隐形的。(v) 10 个入口里 WinkDeal / DisplayScanPrepare 带**文件级静态量**, 因此按**序列**走 (135 次 / 790 次连续调用) 而不是单次采样, 且每一步都比对, 差异出现在导致它的那一次调用上而不是 700 次之后。(vi) 每条输入都非平凡预置: `dirty` 式预填 + 每个字段都写, 免得"两侧都还停在 0"冒充"一致"。(vii) 32 位 LCG + xorshift 混合后再 `>> 8`, 避免低比特退化。

| 项 | 结果 |
|---|---|
| 差分用例数 | **84,331** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_DISPLAY_CODE`, `_DealCheckMenuModeGroupGrade`, `_DispDataDeal`, `_DisplayScan`, `_DisplayScanPrepare`, `_GpioCtrlRegs`, `_GpioDataRegs`, `_InitSetSpiForFunc`, `_InitSpiaGpio`, `_KeyBordTest`, `_KeyProcess`, `_KeyScan`, `_LED_CODE`, `_Menu1OnPrg`, `_SpiaRegs`, `_UpdateDisplayBuffer`, `_UpdateMenuModeDisplayBuffer`, `_WinkDeal`, `_accDecFrqPrcFlag`, `_bEnflash`, `_bFrqDigital`, `_bKeyEsc`, `_bKeyProc`, `_bLight`, `_bOnShift`, `_checkMenuModeCmd`, `_checkMenuModePara`, `_diHwStatus`, `_dispCodeMenuMode`, `_displayBuffer`, `_errAutoRstNum`, `_errorCode`, `_errorCodeOld`, `_factoryPwd`, `_flashMaxSDSD`, `_flashTicker`, `_flashTickerLed`, `_funcCode`, `_keyAge`, `_keyBordTestFlag`, `_keyBordValue`, `_keyChecking`, `_keyFunc`, `_keyPreCtrl`, `_keyPreFunc`, `_keySelStatus`, `_keySelType`, `_keySelTypeBak`, `_keyStatus`, `_keyValue`, `_menu`, `_menuAttri`, `_menuLevel`, `_menuMode`, `_menuModeStatus`, `_menuModeTmp`, `_minRate`, `_onQuickTicker`, `_onQuickTickerMax`, `_onShiftPassViewTicker`, `_onShiftPassViewTickerMax`, `_repeatNum`, `_repeatNumMax`, `_repeatStep`, `_runFlag`, `_runMode`, `_runSrc`, `_scanStep`, `_spiData`, `_tuneCmd`, `_upDownDelta` |
| `.text` 尺寸 | 1167 → 1166 (**-1**) |

### `src/function/f_main.c`

- 变换: 函数侧顶层调度, 十八个入口: InitForFunctionApp / InitDspFunc / InitFuncCode / FuncVariableInit / Main2msFunction / Main0msFunction / Main05msFunctionEachStart / Main05msFunctionEachEnd / Main05msFunctionA / Main05msFunctionB / Main05msFunctionC / Main05msFunctionD / CodeRunMonitor / GetNumberDigit / GetNumberDigit1 / GetNumberDigit2 / SoftVersionDeal / getMomeryValue。改写手法: (1) GetNumberDigit 的两支**极性反转 + 函数体对调** (`if (!mode) { GetNumberDigit1(...); } else { GetNumberDigit2(...); }` 变成 `if (mode != 0) { GetNumberDigit2(...); } else { GetNumberDigit1(...); }`); (2) 位段裸判据补成显式比较 `!= 0` (`pAttribute->attribute.bit.upperLimit` / `.bit.lowerLimit` / `dspStatus.bit.runEnable` / `dspStatus.bit.uv`); (3) `if (!x)` 改成 `if (x == 0)` (errorEeprom / funcCodeRwModeTmp 两处 / pAttribute->attribute.bit.upperLimit); (4) 裸整型判据补成 `!= 0` (errorEeprom / errorFuncCode / digit[4..1]); (5) SoftVersionDeal 里 `funcCode.code.funcSoftVersion` 与 `funcCode.code.motorSoftVersion` 各被读两次, 提成两个**同宽 (Uint16)** 具名局部量 m_funcSoftVersion / m_motorSoftVersion —— 三个字段在 include/function/f_funcCode.h 里都是 `Uint16`, 提出来不改变任何一次比较或赋值的宽度; 读出时机在两处写 productVersion / funcSoftVersion **之后**、分支写 softVersion **之前**, 中间没有任何写路径; (6) Main05msFunctionEachEnd 的 `ticker++; if (ticker >= 4) { ticker = 0; }` 合成 `if (++ticker >= 4)`, 与本文件 CodeRunMonitor 里已有的写法统一; (7) getMomeryValue 的两条语句合成声明处初始化; (8) 28 行制表符缩进改 4 空格 (其中第 652 行是"3 空格 + 制表符"的混排, 单独按周围同级缩进摆平); (9) 4 处运算符两侧补空格 (`[fcIndex+1]` / `%10` / `* 3413` / `[FNUM_PARA-1]`); (10) GetNumberDigit 里 `return 5;` 的缩进笔误 (12 空格) 摆成同级的 8 空格。**没有**改任何判据常量、写入字段、写入次序或调用次序。

- 必须守住的不变量:

  - **十八个函数名与全部文件级符号不变**: `Uint16 memoryValue` / `driveCoeff[2][4]` / `aiaoChckReadOK` / `mainLoopTicker` / `CODE_RUN_DISPLAY_UPDATE_TIME` / `timeBase` / `timeBase05msOld` / `ticker05msFuncInternal` / `ticker05msFuncIn[4]` / `ticker05msFuncEach` / `ticker05msFuncEa[4]` / 两个 `*Disp[4]` / `union MOTOR_PARA motorPara` / `struct MOTOR_FC motorFc` / `enum MOTOR_SN motorSn` / `enum POWER_ON_STATUS powerOnStatus` / `enum FUNC_CODE_INIT_STATUS funcCodeInitStatus` 以及两个版本宏 (PRODUCT_VERSION 38000 / SOFTWARE_VERSION 72) 名字、类型、初值一字不动。目标端外部符号集合 **123 个逐一相同** (nm2000 实测, target_check.py `符号集合相同: True`), 两侧编译告警各 0 条, 无新增无消失 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, mismatches=0**, 用例数 77173, 见 docs/_difftest/f_main.json。驱动共 13 相: 12 个入口相 (phase_number_digit / phase_soft_version / phase_code_run_monitor / phase_each_start / phase_each_end / phase_main_5ms_dispatch / phase_init_dsp_func / phase_func_variable_init / phase_init_func_code / phase_init_for_function_app / phase_get_momery_value / phase_trivial) 加 1 相随机 (phase_random)。带文件级静态量的入口 (InitFuncCode 的 `static Uint16 flag`、Main05msFunctionEachStart 的 waitPowerOnTicker、Main05msFunctionEachEnd / CodeRunMonitor 的 ticker) 按**序列**走, 每一步都比对, 差异出现在导致它的那一次调用上。驱动只在末尾打印总账, **分相计数没有落进台账**, 所以这里不列分相数字: 台账里没有的数不手抄。
  - **负向对照 155/155 全抓到, 0 BROKEN, 0 HUNG**, 见 docs/_difftest/_negctl_partial_f_main.json。按函数分组: FM1..FM18 打 InitDspFunc (三段初始化次序、六条时钟使能、SPI/I2C/SCI/ECAP/DI/DO/EPWM4/5/6 的 GPIO 与参数设置互相调换、EDIS 写成 EALLOW); FM19..FM47 打 InitFuncCode 的**状态机与握手循环** (循环退出条件与->或、完成判据取反、检查字个数与比较、新旧 EEPROM 分支互换、AIAO 校验字的四条迁移判据、标定读的起始/结束地址、低字高字写入取错元素、地址步进 2->1 与跳越步长 4->5、跳越判据 4->3、读成功标志、整片写/读模式互换、写读完成判据取反、检查字写回、AIAO 校验字写回置零、待存索引取错。**这一组缺 FM34**: `readIndex > STOP` -> `>=` 经奇偶性穷证为等价变异, 按 README 第 8 节 (b) 类删除, 编号留空 (FM43 / FM45 同为首轮 MISSED, 但那是**测试台短路**而不是等价, 补洞后已抓到 —— 见下面"首轮三个 MISSED 的三种处置"); FM48..FM64 打 DEBUG_F_FC_INIT_OVER 的属性表扫描块 (扫描上界偏一、上限/下限属性判据取错、取表项与取字面量互换、上下限标志取反、限值符号位取反、写回判据取反、写回目标下标、待写登记下标、逐项计算漏掉脉冲采样、待限值个数偏一、最大频率的上限属性/下限系数 50->100/写回判据/待写登记下标); FM65..FM84 打 FuncVariableInit (显示用变频器类型未同步、类型更新换成参数更新、频率计算源备份取错字段、上下限频率源判据 或->与、上电初始加减速频率置零、转子位置/目标频率/记忆步数取错字段、掉电记忆判据取反、记忆时间高位左移 16->8、记忆时间备份丢失、扫描步初值 5->4、短路检查位取错字段、编码器源取错结构、控制字拷贝上界与目标下标偏一、0.5ms/2ms 数据更新互换、MenuInit 调用丢失); FM85 打 Main0msFunction (脉冲采样丢失); FM86..FM99 打 Main05msFunctionEachStart / EachEnd (内部与本周周期差反向、上电就绪判据 与->或、核心就绪状态取错、上电等待门限 >=->>、等待状态判据取错、等待迁移目标取错、通讯与 CAN 计数步长 1->2、EEPROM 处理调用丢失、两个换算系数 3413->3276、下标回绕上界 4->3、显示换算移位 11->10); FM100..FM112 打 Main05msFunctionA / B 两相 (显示准备换成显示扫描、核心数据更新丢失、数字量计算换成输出、脉冲计算换成脉冲采样、模拟量计算换成数字量、电机参数更新换成类型更新、记忆值读取丢失、键盘扫描换成按键处理、CAN 处理换成 P2P 处理、SCI 与示波器 SCI 处理丢失、频率源处理换成转矩计算、VF 分离处理丢失); FM113..FM121 打 Main05msFunctionC (加减速时间计算/运行时间计算/运行时间设置/运行源处理/转矩计算丢失、错误处理换成错误复位、AO1 与 AO2 通道互换、FMP 处理丢失); FM122..FM129 打 Main05msFunctionD (数字输出计算换成数字输入、温度处理/U0 数据更新/2ms 数据更新/代码运行监控丢失、显示缓冲更新与闪烁处理互换、错误复位换成错误处理); FM130..FM139 打 CodeRunMonitor (主循环标志判据取反与置值取错、刷新门限 >=->>、门限除法改成乘法、显示周期步进 2->1、回绕判据 >=->>、回绕初值 200->201、显示换算系数 3276->3413、内部周期显示取错下标、显示换算移位 15->14); FM140..FM150 打 GetNumberDigit / 1 / 2 (十进制十六进制判据取反、位数判据取错下标、位数返回 5->4 与 2->3、位数链第二级取错下标、除十乘数 52429->52428、除十移位 19->18、取余乘数 10->100、十六进制移位 4->3、取余移位 4->3、最高位清零目标取错); FM151..FM154 打 SoftVersionDeal (产品版本号与软件版本号互换、版本比较方向取反、取大值取错分支); FM155..FM156 打 getMomeryValue (读取值恒置零、地址偏移一)。
  - **测试台补洞: 终止性观测点 (这一条是本模块最有价值的产出)**。首轮跑到 FM19 就卡住: FM19 把 InitFuncCode 的握手循环判据 `while ((!errorEeprom) && (FUNCCODE_INIT_STATUS_OVER != funcCodeInitStatus))` 里的 `&&` 改成 `||`。状态机走到 OVER 之后 `(!errorEeprom)` 仍然为真, 于是循环永远不返回 —— 测试台的比对时机是"入口函数返回之后", 一条字段级证据都产不出来, 只能等 ccenv 的 300 s 超时, 结局是 HUNG。HUNG 既不算 CAUGHT 也不算 BROKEN, 总账判失败, 而且"它到底哪里不一样"永远问不出来。
    这不是"变异不成立", 是测试台缺一个观测点。driver.c 的头注释里原本写着这类区域"不是可以补的洞" —— 那是**错的**: 补法很直接。给替身加一个**停机预算**: 每个替身被调用时 `tick()` 累加一次, 累计超过 MOCK_TICK_BUDGET (500000) 就 longjmp 回 driver 的 `step()`, 于是照样逐字段比对。新增两个观测量: `aborted` (这一侧是不是没自己退出、被预算强停的) 与 `ticks` (这一趟一共推了多少拍替身, 也就是走过了多少条调用边)。
    预算由两侧**共用同一段代码**, 不可能偏向任何一侧; 而且基准侧单次调用的实测最大值只有 **3753 拍** (InitFuncCode 的 DEBUG_F_FC_INIT_OVER 分支对 909 项逐个夹限值), 预算是它的 **133 倍**, 基准侧永远够不着 —— "预算对基准侧透明"这件事不是声明, 是落盘数字: 干净跑一次台账 stdout 里就写着 `max ticks in one call: 3753 (budget 500000, headroom x133)`。
    补完之后 FM19 给出的现场是 `MISMATCH InitFuncCode: aborted` + `aborted 0/1 ticks 3637/500001`, 直接判 CAUGHT, 而且是**秒级**返回, 不再吃 300 s 超时。`ticks` 这一条通道还顺带提高了灵敏度: 循环多转一圈、少转一圈都成了可观测的。
    记录这一条是因为它纠正了一个容易犯的判断: "测不了"有两种 —— 一种是真的不可表示 (例如 getMomeryValue 的目标地址空间), 一种是**测试台缺一个观测点**。把第二种当成第一种、把变异从清单里删掉, 结论看着干净其实更弱。
  - **首轮三个 MISSED 的三种处置 (这一条纠正了"MISSED 就是等价"这个默认判断)**。FM34 / FM43 / FM45 三条首轮报 MISSED, 逐条核对后性质完全不同:
    (a) FM43 (`baseTime - GetTime() >= (TIME_1MS << 1)` 的 `>=` -> `>`) 与 FM45 (门内 `& 0x00ff` -> `& 0xff00`) 是**测试台有洞**, 不是等价 —— 两条都**修了测试台**并复测为 CAUGHT, 编号保留。根因是第 314 行那个"每 2 ms 一次"的门被短路了, 两处:
      1. 替身的计时器**加错了方向**。原来写 `CpuTimer1Regs.TIM.all += gTimeStep;`, 而 C28x 的 TIM 是下计数器, `baseTime - GetTime()` 本该从 0 变大; 写成加号后它恒为负, 按 Uint32 回绕成 ~4.29e9, 门恒为真 —— `>=` 与 `>` 于是永远都成立 (FM43 隐形)。已改成 `-=`。
      2. `displayBuffer[]` 是 f_main 的**读**输入 (第 316 行 `spiData.out.a.bit.dispCode = (displayBuffer[scanStep] & 0x00ff);`), 正常由替身 UpdateDisplayBuffer() 填, 而替身不填 -> 数组恒全 0 -> 两个掩码都得 0, 殊途同归 (FM45 隐形)。已把 displayBuffer 加进 driver 的输入映像并 rnd_fill。
    另外 gTimeStep 的扫描集补进了门槛的整除因子 (理由见上面第 (v) 条)。
    (b) FM34 (`readIndex > GetEepromIndexFromFcIndex(AI_AO_CALIB_STOP)` 的 `>` -> `>=`) 是**真等价**, 而且对测试台映射与真机映射**都**等价, 因此删除 (编号留空)。证明不靠采样: `readIndex` 只在第 245 行被赋成 `GetEepromIndexFromFcIndex(AI_AO_CALIB_START)`, 之后只经历第 266 行 `+= 2` 与 第 271 行 `+= 4`, 所以 `readIndex - START` 恒为**偶数**; 两个算符只在 `readIndex == STOP` 时分歧, 这要求 `STOP - START` 为偶数。而它是**奇数**, 且可穷证: START / STOP 是编译期字偏移 (实测 990 / 1005, 差 15); 真机映射 (f_eeprom.c 第 1001 行) 对 index >= fChk0 走 `fcNoAttri2Eeprom[index - fChk0]`, 那张表 (f_funcCode.c 第 1175 行, CHK_NUM+REM_NUM = 52 项) 是 `{0,1,2,3,16..63}`, 即 j<4 取 j、j>=4 取 j+12 —— 令 a = 990-fChk0, b = a+15, 两者都 >= 4 时差 15, a <= 3 而 b >= 15 时差 27, **两种情况都是奇数**, 与 fChk0 取何值无关。
    教训: "MISSED" 不等于 "等价"。三条里只有一条真等价; 若一律按等价删除, 就会把两个真 bug 连同它们背后的两个测试台缺陷一起销掉 —— 结论看着干净, 其实更弱。
  - **`.text` 1114 -> 1119 (+5) 字**, 目标端实测 (target_check.py / nm2000)。十八个函数名、调用次序、`#if DSP_2803X` 与 `#if DEBUG_F_FC_INIT_OVER` 的分支结构全部保留。**十八个入口全部由 `src/system/main.c` 的后台循环调用 (InitForFunctionApp 只在开机初始化时调一次), 不在任何中断路径上** (本模块无 `interrupt void`), 少 5 个字对时序没有影响, 不需要重测最坏执行时间。
  - **刻意不做的事**: (i) 不动 `asm(" nop");` (11 处) 与 `asm(" ESTOP0");` (13 处) 的任何一处 —— 它们既是原工程插入的空指令/断点, 也是 cases/f_main/host_patch.json 的锚点 (count=11 / 13), 改一个字符宿主补丁就会失配报错; (ii) 不动 `(Uint16*)MEMORY_ADDRESS` 的写法 (host_patch.json 的锚点, count=1, 宿主上被改写成 `(Uint16*)(gHostMemory + MEMORY_ADDRESS)`); (iii) 不删 `Main2msFunction` 的空函数体, 不删 `if (sizeOfFuncCodeAttribute != sizeOfPara)` 里那个只执行 `asm(" nop")` 的空块 —— 原工程就有, 删掉对产物零影响, 保留才是"功能不要变"; (iv) 不动 `#if 0` / `#elif 0` / `#if DSP_2803X ... #else` / `#if DEBUG_F_CHK_FUNC_CODE (0)` 那一整块 (918..1126) 的**语句与结构**, 只做缩进与空格归一 —— 死分支不参与编译, 归一它不可能改变产物; (v) 不把 `(TIME_1MS << 1)` 提成局部量: `TIME_1MS` 是 `100000UL` / `60000UL`, `<< 1` 之后是 32 位常量, 原样是**编译期立即数**, 提成局部量会给热循环平白多出一个栈槽, 收益为零。
  - **宿主不可表示区** (与 cases/f_main/driver.c 头注释里的五条一一对应): (1) getMomeryValue 解引用的是一个 16 位**地址**, host_patch.json 把它改成对 64K 字宿主表 gHostMemory 的索引, 于是读是良定义的而地址语义仍在, memoryAddr 扫过 0 / 1 / 0x00FF / 0x0100 / 0x7FFF / 0xFFFE / 0xFFFF / 0x1234 加 256 个随机值; (2) `asm(" nop")` / `asm(" ESTOP0")` 是 C28x 专有, host_patch.json 换成空语句, 它们本来就不产生可观测状态; (3) InitFuncCode 的 `else if (errorEeprom)` 那一支以 `for (;;)` 结尾, 只靠 errorEeprom != 0 才可达, 一进去就不返回 —— 测试台全程保持 errorEeprom == 0, **不对那一支声明任何覆盖**; (4) `FUNCCODE_ALL` / `union MOTOR_PARA` / `struct MOTOR_FC` 按 memcmp 比, 它们都是 `Uint16[]` 与位段结构的 union, 所以比的是精确内容, 但填充字节属于宿主布局, 测试台对目标端填充不作任何断言; (5) funcCodeOneWriteIndex / limitedByOtherCodeIndex 在测试台里按最坏情况给到 2048, 交付源码里仍是 200 / 150 (交付源码一个字都没动), 两个计数按原样比对。
  - **测试台设计: 逐字段比对 + 可观测的副作用**。(i) 模块自有状态 (18 个对象: funcCodeInitStatus / memoryValue / motorPara / motorFc / motorSn / powerOnStatus / driveCoeff / aiaoChckReadOK / mainLoopTicker / CODE_RUN_DISPLAY_UPDATE_TIME / 六组 ticker 数组与标量 / timeBase 与 timeBase05msOld) 由 ref_tu.c / new_tu.c 改名成两套独立实例, 逐字段快照后比对。(ii) 它**借用**的 27 个外部全局量 (funcCode / funcCodeRead / errorEeprom / dspStatus / dspMainCmd1 / spiData / scanStep / plcStep 组 / commTicker / canLinkTicker / gSendToFunctionDataBuff / limitedByOtherCodeIndexNum 等) 每次调用前整块重装、调用后整块快照比对 —— 否则一侧少写、另一侧后跑把它覆盖回去, 就看不见了。(iii) 两个外设寄存器块 (SysCtrlRegs / CpuTimer1Regs) 由驱动扮演芯片, 所以"改写后不再写 PCLKCR0.bit.ECANAENCLK"或"往 EPWM4ENCLK 写了错位"都是可观测的; `EALLOW` / `EDIS` 在 shim 里变成 `(gHostCpuCtl[4]++)` / `(gHostCpuCtl[5]++)`, 逐侧比对, 所以 FM18 那种"EDIS 写成 EALLOW"是可观测的而不是隐形的。(iv) 58 个替身函数逐条记 (op, 参数) 日志, 日志按**序列**比 (前缀逐字节 + 长度单独比), 所以"同一批调用换了顺序"与"少调了一次"都抓得到; 日志长度 5120 与容量 2048 的取值理由写在 mock_main.h 里。(v) 时间被建模成一个 32 位寄存器 (f_main.h 的 `GetTime()` 就是 `CpuTimer1.RegsAddr->TIM.all`), 每个替身被调用时把计时器**减**掉 gTimeStep 拍 —— 减, 因为 C28x 的 CPU 定时器 TIM 是**下计数器**, 方向由全工程的时间差写法定死 (f_main.c 第 314/592 行、MotorEncoder.c 第 692 行都是"先读的减后读的")。于是 `baseTime - GetTime() >= (TIME_1MS << 1)` 变成**可控输入**: 差值 = gTimeStep x (自 baseTime 起的替身边数), driver 因此扫 12 个值, 其中八个是门槛 120000 的**整除因子** (120000 / 60000 / 40000 / 30000 / 24000 / 20000 / 15000 / 12000) —— 只有整除因子才能让差值正好落在门槛上, 而那正是 `>=` 与 `>` 唯一分歧的地方。(vi) 每条输入都非平凡预置: dirty 式预填 + 每个字段都写, 免得"两侧都还停在 0" 冒充"一致"。(vii) 32 位 LCG + xorshift 混合后再 `>> 8`, 避免低比特退化。(viii) **终止性**: 见上面那条"测试台补洞"。

| 项 | 结果 |
|---|---|
| 差分用例数 | **77,173** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AccDecTimeCalc`, `_AiCalc`, `_AoCalcChannel`, `_CODE_RUN_DISPLAY_UPDATE_TIME`, `_CanlinkFun`, `_CodeRunMonitor`, `_CpuTimer1`, `_DiCalc`, `_DisplayScan`, `_DisplayScanPrepare`, `_DoCalc`, `_EepromDeal`, `_EepromOperateTimeDeal`, `_ErrorDeal`, `_ErrorReset`, `_FMPDeal`, `_FrqSrcDeal`, `_FuncVariableInit`, `_GetEepromIndexFromFcIndex`, `_GetFcIndexFromEepromIndex`, `_GetNumberDigit`, `_GetNumberDigit1`, `_GetNumberDigit2`, `_I2cDealBeforeInit`, `_InitDIGpio`, `_InitDOGpio`, `_InitDspFunc`, `_InitECap1Gpio`, `_InitEPwm4Gpio`, `_InitEPwm5Gpio`, `_InitEPwm6Gpio`, `_InitForFunctionApp`, `_InitFuncCode`, `_InitI2CGpio`, `_InitSciaGpio`, `_InitSetEPWM4`, `_InitSetEPWM5`, `_InitSetEPWM6`, `_InitSetEcap1`, `_InitSetI2ca`, `_InitSetScia`, `_InitSetSpiForFunc`, `_InitSpiaGpio`, `_KeyProcess`, `_KeyScan`, `_LimitDeal`, `_Main05msFunctionA`, `_Main05msFunctionB`, `_Main05msFunctionC`, `_Main05msFunctionD`, `_Main05msFunctionEachEnd`, `_Main05msFunctionEachStart`, `_Main0msFunction`, `_Main2msFunction`, `_MenuInit`, `_OscSciFunction`, `_P2PDataDeal`, `_PulseInCalc`, `_PulseInSample`, `_ReadFuncCode`, `_RunSrcDeal`, `_SciDeal`, `_SoftVersionDeal`, `_SysCtrlRegs`, `_TemperatureDeal`, `_TorqueCalc`, `_UpdateDataCore2Func`, `_UpdateDataFunc2Core0p5ms`, `_UpdateDataFunc2Core2ms`, `_UpdateDisplayBuffer`, `_UpdateErrorDisplayBuffer`, `_UpdateInvType`, `_UpdateMotorPara`, `_UpdateU0Data`, `_WinkDeal`, `_aiaoChckReadOK`, `_canLinkTicker`, `_commTicker`, `_decNumber`, `_displayBuffer`, `_driveCoeff`, `_dspMainCmd1`, `_dspStatus`, `_errorCode`, `_errorEeprom`, `_frqAimTmp0`, `_frqCalcSrcOld`, `_funcCode`, `_funcCodeAttribute`, `_funcCodeInitStatus`, `_funcCodeOneWriteIndex`, `_funcCodeOneWriteNum`, `_funcCodeRead`, `_funcCodeRwModeTmp`, `_gSendToFunctionDataBuff`, `_getMomeryValue`, `_limitedByOtherCodeIndex`, `_limitedByOtherCodeIndexNum`, `_mainLoopTicker`, `_memoryValue`, `_motorFc`, `_motorPara`, `_motorSn`, `_plcStep`, `_plcStepRemOld`, `_plcTime`, `_plcTimeRemOld`, `_pmsmRotorPos`, `_powerOnStatus`, `_runTimeCal`, `_scanStep`, `_setTimeRun`, `_spiData`, `_ticker05msFuncEa`, `_ticker05msFuncEach`, `_ticker05msFuncEachDisp`, `_ticker05msFuncIn`, `_ticker05msFuncInternal`, `_ticker05msFuncInternalDisp`, `_timeBase`, `_timeBase05msOld`, `_upDownFrqInit`, `_vfSeparateDeal` |
| `.text` 尺寸 | 1114 → 1119 (**+5**) |

### `src/function/f_interface.c`

- 变换: 核心/功能两侧的数据交换层, 三个 2ms 搬运函数 + 三个周期回调。改写手法: (1) **删掉全部 `#if 1 ... #else ... #endif` 与 `#elif 1` 的死分支** (5 处 memcpy 的逐字段 `#else` 展开、cf[] 的 40 项展开、uf[] 的 30 项展开) —— 那些分支一次都没被编译过, 删掉对产物零影响, 也让"哪些语句真在活路径上"一眼可读; (2) 未被任何负向对照锚点覆盖的 幻数提成具名宏 (`OUT_FDB_FRQ_DISP_FILTER_TIME 30` / `FRQ_DISP_CLAMP 32000` / `OUT_CURRENT_DISP_DEADBAND 15` / `SPEED_DIR_STABLE_TIME 2500`) —— **只提取, 不改值**; (3) `checkSpeedFdbDir` 的第二条判据把 `checkFlag == ...` 从末尾挪到开头 (与同一函数里第三条 判据的写法统一), `&&` 内的项次序不动; (4) `UpdateDataFunc2Core0p5ms` 末尾的 `#if 1` 块保留 原注释结构; (5) 全文件把若干处错位缩进与运算符两侧空格归一。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。

- 必须守住的不变量:

  - **函数名与文件级符号不变**: copyDataFunc2Motor05ms / copyDataFunc2Motor2ms / copyDataFunc2CF2ms / copyDataCore2Func2ms / copyDataCore2Uf2ms / copyDataCore2Func05ms / checkSpeedFdbDir / UpdateDataCore2Func / UpdateDataFunc2Core0p5ms / UpdateDataFunc2Core2ms 十个函数名, 以及 frq2Core / frqCurAim2Core / speedMotor / motorRun / frqRun / frqVFRun / frqVFRunRemainder / frqFdb / frqFdbTmp / powerAddDec / softOCTicker 等全部文件级全局的名字、类型、初值一字不动。目标端外部符号集合 **81 个逐一相同** (nm2000 实测, target_check.py `符号集合相同: True`), 两侧编译告警各 0 条, 无新增无消失; `.text` 尺寸 ref=1319 new=1319 字, 差 +0 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, mismatches=0**, 用例数 205651, 见 docs/_difftest/f_interface.json。驱动共 6 相: phase_copy_f2m05 / phase_copy_f2m2 / phase_copy_cf / phase_copy_c2f (三个搬运函数的逐字段搬运, 每个下标都驱动到非零值, 免得"两侧都停在初值"冒充一致) / phase_frq (frqRun 定标、VF 余项、frqFdbLpf 低通、checkSpeedFdbDir 状态机、方向取反与 32000 钳位、LPF 迭代到不动点后判门槛) / phase_power_add (powerAddDec 累加与 powerAddupInt 进位) / phase_softoc / phase_currdisp (电流显示死区 15 与软过流计时)。带文件级静态量的入口 (checkSpeedFdbDir 的 checkFlag / speedDir / speedDirBak / speedFdbDirTcnt、UpdateDataCore2Func 的 frqFdbLpf / outCurrentDispOldTmp) 按**序列**走, 每一步都比对。
  - **负向对照 208/208 全抓到, 0 BROKEN, 0 HUNG**, 见 docs/_difftest/_negctl_partial_f_interface.json。按函数分组: IF01..IF12 打 copyDataFunc2Motor05ms (14 个下标的源字段与目标下标); IF13..IF61 打 copyDataFunc2Motor2ms (下标的源字段/目标下标、7 处 memcpy 的长度/源/目的偏移/源下标); IF62..IF87 打 copyDataCore2Func2ms (30 个下标的源下标与目标字段); IF88..IF93 打 copyDataCore2Uf2ms 与 copyDataCore2Func05ms; IF94..IF104 打 checkSpeedFdbDir 状态机 (启动判据、方向乘积判据、稳定计数门限 2500、END 判据、写回字段); IF105..IF187 打 UpdateDataCore2Func (定标与余项、低通、方向、钳位、电流显示死区、软过流计时、功率累加、位段打包、末尾状态同步); IF188..IF208 打 UpdateDataFunc2Core2ms (位域打包, 21 条)。
  - **首轮 5 条 MISSED 全部查明是测试台有洞、补洞后抓到 (这一条纠正了"MISSED 就是等价"的默认判断)**:
    (a) IF124 / IF125 (钳位门槛 32000, `>` vs `>=` 与三目取正/取负) —— 门槛落在 `frqFdbDispTmp` 上, 而它是 `frqFdbLpf` 的**输出**; 单次调用只把输出从 0 挪一步 (t=30), 从 0 出发永远够不到 32000。补法: `phase_frq` 的钳位块用同一组输入把 LPF **迭代 160 次到不动点**再判门槛。收敛是**精确**的 (`tmp = (in - outOld + remainder) * PERIOD; out = tmp/t + outOld`, 当 `outOld == in` 且余项为 0 时 `out == in`), 所以迭代 160 次后输出恰好等于输入, 门槛可达。补后 mismatches=20/20。
    (b) IF131 / IF132 (`speedMotor == frq2Core` 这一支) —— 该分支从未被驱动过。补法: 新增 `speedMotor`/`frq2Core`/`frqDroop` 三分量的边界子相, 相等与不等各来一遍, `frqDroop` 扫 `{-32001,-32000,0,32000,32001}`。补后 mismatches=16/4。
    (c) IF158 (`powerAddupInt` 的进位门限 3600) —— 见下一条"隐藏输入通道"。补后 mismatches=1。
  - **隐藏输入通道 (本模块最有价值的产出): `funcCode.code.powerAddupInt` 是缓冲区只输入的别名, 直接写它是死写。** `FUNCCODE_ALL` 是个 union, 宿主实测 (build/_iface_off.py): `sizeof(FUNCCODE_ALL)=2580`, `sizeof(group)=2232`, `off group.uf = 2172 B = 字 1086`, `off code.powerAddupInt = 2196 B = 字 1098` —— 即 `group.uf[]` 是 group 的**最后 30 字** (1086..1115), 而 `code.powerAddupInt` 落在**字 1098**, 正好是 `uf[12]`。`UpdateDataCore2Func` 一进来就调 `copyDataCore2Func2ms()`, 它的结尾是 `memcpy(&funcCode.group.uf[0], &gSendToFunctionDataBuff[MOTOR_TO_Func_2MS_DATA_NUM], 30)`, 于是 `powerAddupInt` 在每次调用开头被**原地覆盖**成 `gSendToFunctionDataBuff[30+12] = [42]`。直接写 `inW.funcCode.code.powerAddupInt` 会被这次 memcpy 抹掉, 两侧都读到同一个与输入无关的陈旧常量 ⇒ 变异 MISSED, 且**看着跟语义等价一模一样**。补法: driver 里新增 `#define IFACE_FI_POWER_ADDUP_INT 42` 与 `updc_pai()`, 计数器改走 `inW.func2ms[42]`; 阈值扫 `{0,3598,3599,3600,65535}` (`++powerAddupInt >= 3600`, 3600 正好是 `>=` 与 `>` 的唯一分歧点)。
    这是同一条纪律的第二个实例: 判据量到底是"模块全局"还是"缓冲区喂的", 权威是 driver 里的 `SIDE_INSTALL` 列表和 `copyDataCore2Func*ms()` 的 memcpy, 不是字段名字长什么样。凡是想写 `funcCode.code.<x>`, 先算它落在第几字: 落在 1086..1115 的必须走 `gSendToFunctionDataBuff[30 + (字 - 1086)]`。
  - **已排除的等价变异 IF209 (从 negative_control.py 里删除, 不是标记排除)**: `UpdateDataCore2Func 里少调 copyDataCore2Func05ms`。它的函数体是**空的** (`void copyDataCore2Func05ms(void) {}`), 所以删掉调用不可观测 —— 对全工程任何状态、任何序列都等价。对照的是 IF208 (`UpdateDataCore2Func 里少调 copyDataCore2Func2ms`), 后者函数体非空, 实测 mismatches=20。删除理由与 FM34 同 (见 docs/rewritten_modules.json 里 f_main 那条): 一条处处等价的变异留在清单里, 会让 `test_difftest_negctl.py` 从 CASES 推导出的期望集合永远差一条。删掉后 IF01..IF208 连续无洞 (build/_iface_ids.py 实测 `non-sequential transitions: []` / `duplicates: []`)。
  - **锚点是硬约束, 不是建议 (这一条是本模块最痛的教训)**: negative_control 的变异文本是**逐字** `src.replace(old, new, 1)`, 而 host_patch 的保真补丁同样按逐字锚点匹配。第一版改写把 `copyDataFunc2Motor2ms` 的下标写法从 `gSendToMotor2MsDataBuff[k]` 提成局部别名 `p[k]` —— 结果是: (i) 34 条变异锚点 (IF13..IF61) 在新侧**找不到文本**, 变异变成空操作 ⇒ 全 MISSED; (ii) 7 条 memcpy 保真补丁 (side=both, lenient) 在新侧失配被跳过 ⇒ 宿主只搬 n 字节 (ref 侧搬 n 字) ⇒ 20 处**假差异**。修复是把这些文本**逐字还原**。实测: 还原后 `anchors missing: 0`, `cases=205651 mismatches=0`。
    结论: 改写前先跑一遍 `锚点缺失检查` (对每条变异做 `old in new_src`), 缺失数必须为 0; 同时 `build/_iface_patch_clash.py` 要报 `致命冲突 0 条`。锚点覆盖的行**不能**参与改写 —— 能动的只有锚点之外的行 (本模块 643 行里 163 行被锚点钉住, 480 行可动)。
  - **刻意不做的事**: (i) 不动 7 处 `memcpy` 的**任何一个字符** —— 它们既是负向对照锚点 (IF16..IF20 / IF31..IF33 / IF50/51 / IF55..IF57 / IF58..IF61 / IF88..IF90), 也是 cases/f_interface/host_patch.json 的锚点 (count=1, 逐条); (ii) 不动任何被锚点覆盖的赋值语句的字段名、下标、常量 —— 那 208 条变异是按这些文本写死的; (iii) 不删 `#if DSP_2803X` 的 `#else` 分支 (QEP2/QEP1 源选择), 那是**活结构**不是死分支, 只是本工程 `DSP_2803X` 为真; (iv) 不把 `#if 1` / `#elif 1` 结构改掉 —— 它们围住的是空块与活块, 保持原样才不改变产物; (v) 不把 `checkSpeedFdbDir` 的 `static` 局部量提成文件级 —— 它们的内存布局由链接器决定, 提到文件级会改变 `.bss` 归属。
  - **宿主不可表示区** (与 cases/f_interface/driver.c 头注释对应): (1) 三个搬运函数写的是 `gSendToMotor05MsDataBuff` / `gSendToMotor2MsDataBuff` / `funcCode` 三块共享内存, 由驱动按"调用前整块重装、调用后整块快照"处理, 于是"一侧少写、另一侧后跑把它覆盖回去"也看得见; (2) `memcpy` 的字/字节宽度差由 host_patch.json 的 `n*sizeof(Uint16)` 抹平 (C28x 的 char 是 16 位), 补丁只作用于宿主副本, 交付源码与目标端编译完全不受影响; (3) `EQep1Regs.QPOSCNT` 由 shim 提供, 驱动扫 895549822 / 3232341078 等值; (4) `frqFdbLpf.calc` 是 `LpfCalc`, 逐字照抄的外部依赖, 两侧用的是同一个实现, 所以低通内部不是比对对象 —— 低通**输出**才是; (5) 除数为零 (`frqPuQ15 == 0`) 是**前提不是用例**, driver 全程保证 `frqPuQ15 != 0`。

| 项 | 结果 |
|---|---|
| 差分用例数 | **205,651** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ABZPos`, `_EQep1Regs`, `_LpfCalc`, `_UpdateDataCore2Func`, `_UpdateDataFunc2Core0p5ms`, `_UpdateDataFunc2Core2ms`, `_ZCount`, `_aiDeal`, `_antiVibrateCoeff`, `_bUv`, `_checkSpeedFdbDir`, `_copyDataCore2Func05ms`, `_copyDataCore2Func2ms`, `_copyDataCore2Uf2ms`, `_copyDataFunc2CF2ms`, `_copyDataFunc2Motor05ms`, `_copyDataFunc2Motor2ms`, `_currentOc`, `_currentOcDisp`, `_currentPu`, `_driveCoeffFrqFdb`, `_dspMainCmd`, `_dspMainCmd1`, `_dspStatus`, `_dspSubCmd`, `_enCoderPosition`, `_errorCodeFromMotor`, `_errorsCodeFromMotor`, `_frq`, `_frq2Core`, `_frqCurAim`, `_frqCurAim2Core`, `_frqCurAimFrac`, `_frqDroop`, `_frqFdb`, `_frqFdbDisp`, `_frqFdbFlag`, `_frqFdbTmp`, `_frqPuQ15`, `_frqRun`, `_frqRunDisp`, `_frqTmp`, `_frqVFRun`, `_frqVFRunRemainder`, `_funcCode`, `_gPhiRt`, `_gPhiRtDisp`, `_gSendToFunctionDataBuff`, `_gSendToMotor05MsDataBuff`, `_gSendToMotor2MsDataBuff`, `_generatrixVoltage`, `_invPara`, `_invtStatus`, `_itDisp`, `_jerkFf`, `_motorCtrlTuneStatus`, `_motorErrInfor`, `_motorFc`, `_motorRun`, `_outCurrent`, `_outCurrentDisp`, `_outCurrentDispOld`, `_outPower`, `_outVoltage`, `_outVoltageDisp`, `_pmsmRotorPos`, `_powerAddDec`, `_rsvdData`, `_runFlag`, `_runStatus`, `_softOCTicker`, `_softOcDoFlag`, `_spdLoopOut`, `_speedMotor`, `_torqueCurrent`, `_torqueCurrentAct`, `_tuneCmd`, `_upperTorque`, `_vcSpdLoopKp1`, `_vcSpdLoopKp2`, `_vfSeparateVol` |
| `.text` 尺寸 | 1319 → 1319 (**+0**) |

### `src/function/f_eeprom.c`

- 变换: EEPROM 驱动层: I2C 位缓存区搬运 / 中断源分发 / 两次握手 / 读写状态机 / EepromDeal 大状态机。改写手法: (1) **两个状态枚举改成 #define + Uint16** (enum FUNCCODE_WRITE_STATUS / enum FUNC_CODE_RW_STATUS) —— 原文件里这两个枚举只被当作整型用, 改成宏后所有枚举常量仍是整型字面量, 判值与写入字段不变; (2) **三处重试惯用法 `while (f() != OK) { if (++t >= 100) break; }` 改成计数 for** —— 原写法是"先调再判", 新写法是"判完再调", 调用次数上界同为 100 且都在第一次成功后立即退出, 寄存器副作用逐次一致; (3) **唯一 `#elif 1` 死臂改成 `#else`** (10 处), 多臂链 (InitSetI2ca 的 4 选 1) 保留原 `#elif 1` 不动, 否则预处理会断; (4) 函数级 `Uint16 tmp = 0;` 的初始化下沉到 for 的初始化子句; (5) 前导空行、include 次序、宏对齐、声明分组归一。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。

- 必须守住的不变量:

  - **函数名与文件级符号不变**: RwI2cBus / I2cIntDeal / RwEeprom / ReadFuncCode / WriteFuncCode / EepromDeal / UpdateIndexForEepromDeal / EepromOperateTimeDeal / InitI2CGpio / InitSetI2ca / I2cDealBeforeInit / I2CNoACK / I2CStop / I2CRcvByte / GetFcIndexFromEepromIndex / GetEepromIndexFromFcIndex
  - 目标端外部符号集合与改写前**完全相同** (48 个, target_check.py 逐项核对): 没有少读/少写任何一个全局量, 没有漏掉任何一次函数调用
  - ReadFuncCode 的三个函数级 static (readTime / readRightTime / bVerifyWrong) 与 WriteFuncCode 的三个 (writeStatus / bVerifyWrong / number) 名字、生命周期、推进次序不变 —— 它们是跨 case_begin() 保留的隐藏状态
  - I2C 寄存器写入序列 (I2CSAR -> I2CCNT -> I2CDXR -> I2CMDR) 与每条命令字 (0x6E20 / 0x6C20 / 0x6620 / 0x4000 / 0x4020 / 0x6040 / 0x2040) 逐字节不变
  - EEPROM_TYPE == EEPROM_24LC32: 活代码走 `#if` 臂, 24LC08/24LC16 的 `#else` 臂保持不可编译
  - DSP_CLOCK == 60: I2CPSC = 5、I2CCLKL = I2CCLKH = 25

| 项 | 结果 |
|---|---|
| 差分用例数 | **103,745** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_DSP28x_usDelay`, `_EepromDeal`, `_EepromOperateTimeDeal`, `_GetEepromIndexFromFcIndex`, `_GetFcIndexFromEepromIndex`, `_GetFuncCodeInit`, `_GetFuncCodeInitOriginal`, `_GpioCtrlRegs`, `_GpioDataRegs`, `_I2CNoACK`, `_I2CRcvByte`, `_I2CStop`, `_I2cDealBeforeInit`, `_I2cIntDeal`, `_I2cMsg`, `_I2caRegs`, `_InitI2CGpio`, `_InitSetI2ca`, `_ReadFuncCode`, `_RestoreCompanyParaOtherDeal`, `_RwEeprom`, `_RwI2cBus`, `_UpdateIndexForEepromDeal`, `_WriteFuncCode`, `_aiaoChckReadOK`, `_eeprom2Fc`, `_eepromOperateTime`, `_endIndexRwFuncCode`, `_endIndexWriteSeries`, `_errorEeprom`, `_fcNoAttri2Eeprom`, `_funcCode`, `_funcCodeAttribute`, `_funcCodeOneWriteIndex`, `_funcCodeOneWriteNum`, `_funcCodeRead`, `_funcCodeRwMode`, `_funcCodeRwModeTmp`, `_funcCodeRwStatus`, `_funcCodeWrite`, `_index4EepromDeal`, `_number4EepromDeal`, `_powerOnStatus`, `_rwFuncCodeRepeatDelayTicker`, `_startIndexWriteSeries` |
| `.text` 尺寸 | 1308 → 1323 (**+15**) |

### `src/motor/MotorEncoder.c`

- 变换: 编码器/测速层: QEP 初始化、UVW/AB/RT 三种判向、UVW 相位、VC 反馈、M 法/T 法测速、转速平滑、M 法增量、MT 计时、旋变位置与速度。改写手法: (1) **删掉 InitRtInterface 里 `#ifdef TMS320F2808` 的死分支** (本工程是 28035) 并把 `#else` 展开 —— 那些行一次都没被编译过; (2) **删掉 RotorTransCalVel 里的 `#if 0 ... #else ... #endif`**, 只留活臂 (两臂的 `DetaPos` 那一行逐字相同, 所以它才是“唯一锚点”问题的现场); (3) **GetRotorTransPos 的 12 步手工展开改成 `for(mShift=15; mShift>=4; mShift--)` 循环**, 把唯一那句 `asm(" RPT #5 || NOP ");` 放进 `if(mShift > 4)` 里 —— 源码里只剩 1 处 asm, 但**动态执行次数仍是 11 次**, 与改写前逐次相同, 目标端延时不变 (这是 .text 缩小 90 字的主因, 不是行为变化); (4) 三处有界计数 `if(cnt<32767) cnt++;` 改成钳位三目; (5) `else if` / `else` 的返回链改成平铺 `if` + 末尾 `return`; (6) **GetUvwPhase 里三段逐字相同的 ADC 门限判断提成一个文件级 static 函数** `UvwLogicLevel(Uint adcValue, Uint activeLevel)` (static, 不进外部符号表); (7) 全文件缩进与运算符两侧空格归一。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。`Ulong mUlong;` **刻意保留** —— 它只在死臂里被引用, 删掉会让目标端告警集合变化 (两侧各 1 条告警正是它)。

- 必须守住的不变量:

  - **函数名与文件级符号不变**: InitRtInterface / InitSetQEP / ReInitForPG / JudgeABDir / JudgeUVWDir / JudgeRTDir / GetUvwPhase / VCGetFeedBackSpeed / GetSpeedMMethod / GetSpeedMTMethod / SpeedSmoothDeal / GetMDetaPos / GetMTTimeNum / GetRotorTransPos / RotorTransSamplePos / RotorTransCalVel 十六个入口名一字不动 (新加的 UvwLogicLevel 是文件级 static, 不占外部符号)。
  - 目标端外部符号集合与改写前**完全相同** (44 个, target_check.py 用 nm2000 逐项核对, `符号集合相同: True`): 没有少读/少写任何一个全局量, 没有漏掉任何一次函数调用。两侧编译告警各 1 条 (`mUlong` 定义未使用 —— 它在死臂里), **新增 0 条、消失 0 条**。`.text` 尺寸 ref=1511 new=1421 字, 差 **-90** 字 (结论: 通过)。
  - **差分测试: 冻结基准与改写后各跑 n=20000, mismatches=0**, 用例数 56563, hosttraps=2674 / skipped=2674 (全是宿主整数除零: PulseNum*FullFreq01 为 0 的用例, 两侧同样跳过), 见 docs/_difftest/MotorEncoder.json。驱动共 19 相, 覆盖 16 个入口, 外加 `speed edge states` 相 (见下)。
  - **负向对照 117/117 全抓到, 0 BROKEN, 0 HUNG**, 见 docs/_difftest/_negctl_partial_MotorEncoder.json (由 4 份 `_s<k>` 切片台账合并, 合并前断言过两两不重叠且并集恰好 117 条)。
  - **串行输入脚这条覆盖洞已关闭 (本模块最有价值的产出)**: `GetRotorTransPos` 读 `ROTOR_TRANS_SO` 十二次, 而它在测试台里原本是一个**固定内存位** —— 12 次读到同一个位, `mData` 只可能是 0x0000 / 0xFFF0, 于是 12 个移位量互相不可区分 (`<<13` 改成 `<<12` 结果完全相同)。修法: host_patch 把该脚改成 `gHostSoBits[gHostSoIdx++]`, driver 按 MSB-first 喂 12 位图案 (`set_so_pattern()`, 只喂 0/1, 因为芯片上那是一位的引脚)。补洞后 `GetRotorTransPos` 的用例数从 10368 涨到 **31104**, ME80~ME91 这 12 条逐位变异**全部 CAUGHT**。剩下两条诚实的限制: (a) 两个移位量**互换**是真等价 (`|=` 可交换), 所以不能当变异; (b) 只有 F_RTPOS 相会装图案。
  - **ME45 已从清单里删除 (不是标记排除)**: `tempLong = (long)SpeedTemp * TransRatio` 两个操作数互换是**对任意输入都等价**的。证: `SpeedTemp` 是 `int` (16 位), `TransRatio` 是 `Uint` (16 位无符号, include/motor/MotorEncoder.h:72), 两式的**结果类型都是 `long` (32 位)** —— 32 位 long 能表示全部 16 位 unsigned int, 所以两种写法下两个操作数都被转成同一个 32 位整数, 而整数乘法在模 2^32 下可交换, tempLong 逐位相同, 第 411 行 `SpeedEncoder = (llong)tempLong * 16777 >> 24` 的输入也就相同。删除理由与 FM34 / IF209 同: 一条处处等价的变异留在清单里, 会让 test_difftest_negctl.py 从 CASES 推导出的期望集合永远差一条。(若 `TransRatio` 是浮点则 `(long)TransRatio` 会截断, 就不等价了 —— 实测它是 `Uint`。)
  - **五条边界变异 (ME54 / ME59 / ME60 / ME66 / ME69) 首轮全 MISSED, 逐条查明都是测试台有洞, 补洞后全部 CAUGHT —— 这一条再次纠正了“MISSED 就是等价”的默认判断**:
    (a) ME54 / ME66 (`if(abs(v-目标) < 8)` 改成 `< 7`): 只在距离**正好是 7** 时分歧, 而驱动从没构造出这个距离。两个函数都先 `v = Filter4(目标, v_prev)` = `(3*v_prev + 目标)>>2` 再把 v 朝目标挪一步, 所以取 `v_prev = 32757`、`目标 = 32767` 得 32759 → 挪到 **32760**, 距离正好 7 (`v_prev = 32758` 得 6, 也分歧)。目标要取到饱和的 32767 —— M 法下驱动既有的 Poles=4/PulseNum=1024/FullFreq01=10000/MDetaPos=1000 就够, **T 法下不够** (同一组常数归一化后 (m_Llong>>15)=23437 < m_Long=40000, 走除法支路, m_Speed 只有 19200), 要 PulseNum=600 才饱和。新增 `phase_speed_edge()` 相。
    (b) ME60 (`if((m_Llong>>15) >= m_Long)` 改成 `>`): 只在**恰好相等**时分歧。(c) ME59 (`while((m_Long>>16) != 0)` 改成 `!= 1`): 循环少移一位。这两条的判别输入是**穷举搜出来的, 不是猜的** —— build/_me_mt_search.py 给归一化循环建了个模型 (Ulong=32 位、Ullong=64 位、DSP_CLOCK=60), 在参数网格上算每条变异的“原式值 / 变异值”, 只留两者不等且**不挂死**的组合。顺带发现: ME59 在 `m_Long` 归到 0 时是**死循环**, 那也是一种差异, 但会以 HUNG 而非 CAUGHT 记账, 所以不用它。
    (d) ME69 (`if(1000 > SpeedMaxErr)` 改成 `999 >`): 唯一分歧点是 `SpeedMaxErr>>1` 之后**正好等于 999** (即改前是 1998 或 1999), 而 maxerr 扫值表里没有这两个数。补进表里即抓到。
    补洞后重跑全量: 117/117 CAUGHT, 0 BROKEN, 0 HUNG。
  - **宿主 `abs` 是静默空操作 —— 这是本次发现的最严重的保真缺陷, 它让六条变异隐身**: 工程在 SubPrgInclude.h 里声明 `extern int abs(int)`; C28x 上 `int` 是 16 位, 而宿主 shim 的 `#define int short` 把它变成 `short abs(short)`, 但**实际链接到的是 CRT 的 32 位 `int abs(int)`** —— 同符号名、32 位形参。调用方把 16 位实参零扩展 (`-1` → 0x0000FFFF), 32 位 abs 看到正数原样返回, 再截回 short 又变回负数。用工程自己的声明实测 (build/_me_absprobe.py): `abs(-1)=-1  abs(-5)=-5  abs(-100)=-100`。即 `abs(x) == x` 对一切负数成立 —— 调用等于没调。它让 ME45/ME48/ME54/ME59/ME60/ME66 六条变异隐身 (`m_UData = abs(gFVCSpeed.MDetaPos)` 删掉 abs 后两侧状态完全相同), 而差分测试照样报 mismatches=0 —— **失效方向是丢覆盖, 不是假阳性**, 所以直到有条变异拒绝被抓才暴露。修法: 在 mock 里定义 `int abs(int)` (目标文件里的定义压过静态库里的同名符号), 函数体按 C28x RTS 的 16 位回绕语义 (`-(-32768)` 回绕成 -32768)。修完 ME48 立刻从 MISSED 变 CAUGHT。

| 项 | 结果 |
|---|---|
| 差分用例数 | **76,063** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AdcResult`, `_BurrFilter`, `_CpuTimer1`, `_EQepRegs`, `_GetMDetaPos`, `_GetMTTimeNum`, `_GetRotorTransPos`, `_GetSpeedMMethod`, `_GetSpeedMTMethod`, `_GetUvwPhase`, `_GpioCtrlRegs`, `_GpioDataRegs`, `_InitRtInterface`, `_InitSetQEP`, `_JudgeABDir`, `_JudgeRTDir`, `_JudgeUVWDir`, `_ReInitForPG`, `_RotorTransCalVel`, `_RotorTransSamplePos`, `_SpeedSmoothDeal`, `_SysCtrlRegs`, `_VCGetFeedBackSpeed`, `_gBasePar`, `_gCpuTime`, `_gError`, `_gExtendCmd`, `_gFVCSpeed`, `_gIPMPos`, `_gIPMZero`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtInfo`, `_gMotorInfo`, `_gPGData`, `_gPGDir`, `_gPWM`, `_gPmParEst`, `_gRotorTrans`, `_gSpeedFilter`, `_gSpeedLine`, `_gUVWAngleTable`, `_gUVWPG`, `_gVCPar` |
| `.text` 尺寸 | 1511 → 1421 (**-90**) |

### `src/motor/MotorCurrentTransform.c`

- 变换: 定子电流 Clarke/Park/幅值-相角变换层: UVW->AlphBeta (Q15 缩放)、AlphBeta->DQ (Park)、DQ->幅值+相角、以及把三者和滤波串起来的 ChangeCurrent。改写手法: (1) `UVWToAlphBetaAxes` 的两个乘积经 `llong m_AlphBeta[2]` 局部数组中转, 并把 `>>15` 改写为"先 `*2L` 再 `>>16`" —— `floor(2n/65536) == floor(n/32768)` 对正负 n 都成立 (23170/32768 = 46340/65536), 数值逐位相同; (2) `AlphBetaToDQ` 的 M/T 两个结果先各自算进 `llong m_M / m_T`, 再统一右移 15; T 由 `(-((llong)m_sin*Alph)) + ((llong)m_cos*Beta)` 改写为 `((llong)m_cos*Beta) - ((llong)m_sin*Alph)` —— 64 位下加法可交换且不会溢出, 逐位相同; (3) `DQToAmpTheta` 先把 M/T 取成 `long` 局部量再平方相加, 去掉一层冗余括号; (4) `ChangeCurrent` 把 `(gPhase.IMPhase>>16)` 提成 `int m_Angle` (带显式强转), 并把 `UVWToAlphBetaAxes` 调用挪到它之后 (两者互不读写对方的数据, 顺序无关); (5) 全文件缩进归一、局部变量声明归位。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。**两处刻意不动**: (a) `ChangeCurrent` 里的 `temp` 保持 `int` —— 目标端 `int` 是 16 位, `VoltPhaseApply - Theta` 在 16 位里回绕, 改成 `long` 在宿主上仍 0 不一致 但目标端回绕点变了 (这正是"宿主等价、目标端不等价"的标准陷阱); (b) `void inline` 关键字保留 —— 去掉它会让 `UVWToAlphBetaAxes` 变成外部符号, target_check 的符号集合检查立刻报差异 (实测符号集合两侧都是同样 14 个)。

- 必须守住的不变量:

  - **函数名与外部符号不变**: `UVWToAlphBetaAxes` / `AlphBetaToDQ` / `DQToAmpTheta` / `ChangeCurrent` 四个入口名一字不动。目标端外部符号集合两侧完全相同 (14 个: 3 个本模块函数 + `_qsin`/`_qatan` 暂无/`_qsqrt` 和被读写的 9 个全局量, target_check.py 用 COFF storage class == C_EXT 逐项核对, `符号集合相同: True`)。两侧编译告警各 0 条, 新增 0 条、消失 0 条。`.text` 尺寸 ref=219 new=242 字, 差 **+23** 字 (记录, 不作为门禁)。
  - **差分测试: 改写前后各跑 n=20000, cases=130917, mismatches=0** (docs/_difftest/MotorCurrentTransform.json)。四相: `sweep_uvw()` 18x18x18 直驱 UVW->AlphBeta、`sweep_dq()` 18x18x15 直驱 Park、`sweep_amp()` 15x15 直驱幅值/相角、`sweep_change()` 10x10x10x10x6 直驱 ChangeCurrent, 外加 20000 组全位宽随机值 (每轮覆盖三个入口)。
  - **负向对照 23/23 全 CAUGHT, 0 BROKEN, 0 HUNG**, 见 docs/_difftest/_negctl_partial_MotorCurrentTransform.json。23 条变异覆盖三个函数的 Q15 系数、右移位数、加减符号、操作数互换、滤波阶数 (Filter2/Filter8)、角度定标、结构体指针互换。
  - **测试台原本有个洞, 已修 (本模块最有价值的产出)**: `UVWToAlphBetaAxes` 是 `void inline`。目标端 (TI cl2000) 对 C11 `inline` 定义**不发外部符号**, 所以 `new_case.py` 按 nm2000 符号表推导的改名清单里没有它; 但 MSVC 把它发成 linkonce/COMDAT 外部符号, 于是 `ref_tu.obj` 和 `new_tu.obj` 各有一份同名定义, 链接器**不报 LNK2005, 而是静默折叠成一份** —— new 侧那个函数体整个是死代码。首轮负向对照的现象非常典型: **函数体内的 6 条变异 (MC1~MC6) 全 MISSED, 函数外的全部 CAUGHT**。最小复现 `build/_me_repro_inline.py`: 两个 TU 各写一个同名 `void inline` (分别写 111 / 222), 链接通过, 但第二个调用方打印出 111。修法: (a) `ref_tu.c`/`new_tu.c` 给该函数按例改名 (`ref_`/`new_` 前缀), 两份定义名字不同就折不起来; (b) driver 增加 `sweep_uvw()` 相 **直接驱动**该函数, 覆盖不再依赖 ChangeCurrent 传递 (用例数 125085 -> 130917)。修完 MC1~MC5 立刻全部 CAUGHT。全库排查确认: `inline` 关键字在 37 个冻结基准里只出现这 一次, 其余用例不受影响; 但规则本身要记住 —— **TU 改名清单不能只从目标端 nm2000 推, 还要问宿主编译器实际发了什么符号**。
  - **两条 MISSED 变异经证明是等价后从清单删除 (不是标记排除)**: (a) 原 MC19 `Filter2((gIMTQ24.M>>12), gIMTQ12.M)` 两个参数互换 —— `Filter2(x,total) = (((Ulong)total)<<16 + ((Ulong)x)<<15 - ((Ulong)total)<<15) >> 16`, 展开后两种写法都是 `2^15*(x+total)`, 中间量按模 2^32 精确相等, 末次右移对同一个 Ulong 做, 逐位相同。对照: 同一条语句上的 MC20 (Filter2->Filter8) 被抓到, 证明语句本身被覆盖。(b) 原 MC6 `qsin(16384 - angle)` -> `qsin(16384 + angle)` —— 穷举 65536 个 angle, 两侧逐位相同 (build/_me_check_qsin_sym.c)。这是 qsin 的**归约**决定的: 两个参数落到 同一个象限折叠值和同一个符号位 (数学上 sin(pi/2+x) = sin(pi/2-x) = cos x)。**对照组**: 同一形状的变异打在别的偏移上 (18000-a vs 18000+a) 有 65534/65536 个角度 两侧不同 —— 所以"MC6 抓不到"不是测试台不敏感, 而是这一处真的等价。

| 项 | 结果 |
|---|---|
| 差分用例数 | **70,932** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AlphBetaToDQ`, `_ChangeCurrent`, `_DQToAmpTheta`, `_atan`, `_gIAlphBeta`, `_gIAlphBetaQ12`, `_gIAmpTheta`, `_gIMTQ12`, `_gIMTQ24`, `_gIUVWQ24`, `_gOutVolt`, `_gPhase`, `_qsin`, `_qsqrt` |
| `.text` 尺寸 | 219 → 242 (**+23**) |

### `src/motor/MotorDebug.c`

- 变换: 上位机调试数据记录层: `gDebugBuffer[1000]` 环形缓冲 + `gDebugPoint`/`gDebugFlag` 的写入、回卷、半区回退与清零, 以及按 `TestData12/13/14/15/17` 和 `gMainCmd.Command.bit.Start` 决定"这一拍要不要记、记几条曲线、是否复位缓冲" 的 `DebugSaveDeal`。改写手法: (1) 删掉 `#ifdef TMS320F2808 -> 5000` 这条死分支和外层 `#ifdef MOTOR_SYSTEM_DEBUG` 的 `#else -> C_DEBUG_BUFF_SIZE 1` 死分支 —— 本文件第 10 行自己 `#define MOTOR_SYSTEM_DEBUG`, 而本工程是 28035 (`TMS320F2808` 全工程从未定义), 两条分支预处理后均不可达, 宏值恒为 1000; (2) `switch(gTestDataReceive.TestData12)` 的 1/2/default 三档改写为 `if/else if/else` 链, `save` 的三处取值逐个相同; (3) `saveIndexP` 的 `==1/==2/==3/==4` 四条并列分支改写为 `int * pVDTable[4]` 局部数组 + `for(i = 0; i < (Uint)saveIndexP; i ++) { SaveDebugData16(*pVDTable[i]); totalN ++; }` —— 原四条分支各自"存一个通道、加一个计数", 与循环逐项展开逐条相同; `saveIndexP` 取 0 或 >4 时两条路径都不保存、`totalN` 都为 0 (原代码四个 if 都不进; 新代码被 `saveIndexP >= 1 && saveIndexP <= 4` 挡住); (4) `for(i = 0; i < 32; i++) if(saveIndexFunc & (1L<<i)) totalN++;` 改写为 `Ulong m_Mask = saveIndexFunc; while(m_Mask){ totalN += (Uint)(m_Mask & 1L); m_Mask >>= 1; }` —— `saveIndexFunc` 是 `Ulong` (目标端 32 位), 置位数逐位相同, 且不再写 `1L<<31` 这种对长整型做 31 位左移的边界表达; (5) `gTestDataReceive.TestData17/2` 与 `C_DEBUG_BUFF_SIZE/2` 提成 `Uint m_HalfPeriod` / `Uint m_Half` 局部量 (目标端 `Uint` 是 16 位, 500 与 498 都在范围内); (6) 局部变量声明归位、缩进归一、`SaveDebugData32` 里的半区回退写成单行 `if`。`#pragma DATA_SECTION(gDebugBuffer, "debug_buffer")` 原样保留。**没有**改任何判据常量、写入字段、写入次序、调用次序或类型宽度。

- 必须守住的不变量:

  - **函数名与外部符号不变**: `DebugSaveDeal` / `SaveDebugData16` / `SaveDebugData32` / `ResetDebugBuffer` 四个入口名一字不动。目标端 (C28x) 外部符号集合两侧完全相同, 19 个: `_DebugSaveDeal`/`_ResetDebugBuffer`/`_SaveDebugData16`/`_SaveDebugData32`/`_SaveTmCnt`/`_gDebugBuffer`/`_gDebugFlag`/`_gDebugPoint`/`_gMainCmd`/`_gTestDataReceive`/`_pVD1`~`_pVD4`/`_saveCurveNum`/`_saveIndexFunc`/`_saveIndexP`/`_saveRstMem`/`_startSave` (target_check.py 用 COFF storage class == C_EXT 逐项核对, `符号集合相同: True`)。两侧编译告警各 0 条, 新增 0 条、消失 0 条。
  - **交付目标文件的段表与原工程逐项相同 (非调试段)**: `ofd2000 -g` 对比 `build/Debug/MotorDebug.obj` 与原工程拷来的 `build/_verify/baseline_tree/Debug/MotorDebug.obj`, 12 个段的 id/名称/尺寸/对齐/alloc 全部一致 —— `.text 0xd0`、`.cinit 0x4`、`.ebss 0x12`、`debug_buffer 0x3e8 (1000 字)`; 唯一不同的是 `.debug_info` (0x1118 vs 0x110c, DWARF 调试信息, 源码改写必然变)。`debug_buffer` 段尺寸 1000 字证明 `#pragma DATA_SECTION` 仍然生效, 缓冲区既没被挪回 `.bss` 也没变尺寸。测试台口径下 `.text` 尺寸 ref=271 new=224 字, 差 -47 (删死分支 + 四条并列分支改循环的结果, 记录, 不作为门禁)。
  - **差分测试: 改写后交付件 nrand=200000, cases=758330, mismatches=0** (docs/_difftest/MotorDebug.json)。测试台四个入口全部直驱 (`DebugSaveDeal`/`SaveDebugData16`/`SaveDebugData32`/`ResetDebugBuffer`), 外加 `sweep_dsdeal_gates` (td17 x 半周期附近 x td12 x Start x td14 x td13 x pos)、`sweep_save_block` (9 x 7 x 10 x 3 x 6 x 2)、`sweep_save16`/`sweep_save32`/`sweep_reset` 直驱、`sweep_reset_latch` (5 x 4 x 8 条**四步序列**), 以及随机状态 + 随机操作序列 (1~6 步, pos 半数过门)。`DebugSaveDeal` 是有状态的 (`SaveTmCnt`/`saveRstMem`/`gDebugPoint` 跨调用保持), 所以驱动相写成序列而不是单次调用。
  - **负向对照 34/34 全 CAUGHT, 0 BROKEN, 0 HUNG**, 见 docs/_difftest/_negctl_summary.json (该轮并入时全库总账 3120/3120 CAUGHT, SubPrg 加入后当前为 3174/3174, 其中 MotorDebug 贡献 MD1~MD6、MD8~MD35 共 34 条)。34 条变异覆盖: 半周期门的 `>=`/`>` 与分母、选档判据、`switch` 三档的保存位、回卷判据与 `TestData14` 的回卷方向、`if(save)` 的取反、`saveIndexP` 选档、单/四通道计数、指针组 vs 位掩码的 判据、位掩码循环位数与掩码形状、`saveCurveNum` 回写、计数加减、`saveRstMem` 锁存的门与增减、三个写入函数的钳位/回卷/递增、`gDebugFlag` 置位与判据、缓冲区尺寸宏。
  - **测试台原本有两个洞, 已修 (本模块最有价值的产出)**: 首轮 35 条变异只有 20 CAUGHT, 15 条 MISSED 全落在 `if(save){ ... }` 曲线计数块和 `saveRstMem` 复位锁存里。逐条分类后**没有一条是等价变异被漏判**, 两条都是测试台的洞: (a) `sweep_dsdeal` 只扫门限量, `saveIndexP`/`saveIndexFunc`/`TestData15` 恒为 0 —— 指针路径一条都不存、位掩码路径一位都不数, `saveCurveNum` 恒为 0, 整块代码不可见 (这正是"常量必须同时是输入"的坑); (b) 单次调用只能打到 `saveRstMem` 锁存的**一条边**, 上升沿 (TestData15 置 1 且锁存为 0 -> 复位) 与下降沿 (锁存非 0 且 TestData15 归 0 -> 解锁) 必须序列驱动。修法: 新增 `sweep_save_block()` (把三个被读量全部扫到, `saveIndexFunc` 含 0xFFFFFFFF/0x80000000/0xAAAAAAAA/0xFFFF0000 等高位形状)、`sweep_reset_latch()` (按四步模式序列驱动)、随机用例的 pos 半数过门。重跑 34/35, 剩下 1 条 MD7 证明等价后删除。
  - **一条 MISSED 变异经证明等价后从清单删除 (不是标记排除)**: 原 MD7 `case 1: save = 1;` -> `save = 2;`。`save` 是 `DebugSaveDeal` 的局部量 (`Uint save;`), 写完 switch 之后**只被 `if(save)` 读一次**, 既不回写全局量也不进函数返回值 —— 所以只影响真值时 1 和 2 完全等价。对照组: MD6 (case 0 的 0 -> 1) 与 MD9 (default 的 0 -> 1) 都 CAUGHT, 因为那两个改的是 0/非 0 本身; MD8 (case 2 的 `? 1 : 0` -> `? 0 : 1`) 也 CAUGHT, 因为它改的是同一个真值。证明原文留在 tools/difftest/negative_control.py 的 `CASES['MotorDebug']` 注释块里。
  - **宿主保真靠 host_patch**: `gDebugBuffer` 在宿主上被 host_patch.json 两侧同时折成 `Uint16` (目标端 `Uint` 是 1 个字, 宿主是 4 字节)。不折宽的话 `SaveDebugData32` 的 `p = (Ulong *)(&gDebugBuffer); p[i] = data;` 在宿主只改一个 32 位元素, 目标端改两个 16 位字, 缓冲区内容永远比不出真话。
  - **按工程真实优化选项编译, 改写收敛到同一份机器码** (docs/_difftest/MotorDebug.o2.json): `-o2`(Debug) / `-o3`(Release) 下 `dis2000 -1` 反汇编**逐行相同**, 两侧各 246 行, ndiff=0; 关优化 (build_and_compare 口径) 下 `.text` 有 -47 字差异。即本模块属于 "关优化下有痕迹、真实选项下完全收敛"这一类 —— 与 `f_vf.c`/`f_frqSrc.c` 同级, 是全库最强的结果。

| 项 | 结果 |
|---|---|
| 差分用例数 | **758,330** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_DebugSaveDeal`, `_ResetDebugBuffer`, `_SaveDebugData16`, `_SaveDebugData32`, `_SaveTmCnt`, `_gDebugBuffer`, `_gDebugFlag`, `_gDebugPoint`, `_gMainCmd`, `_gTestDataReceive`, `_pVD1`, `_pVD2`, `_pVD3`, `_pVD4`, `_saveCurveNum`, `_saveIndexFunc`, `_saveIndexP`, `_saveRstMem`, `_startSave` |
| `.text` 尺寸 | 271 → 224 (**-47**) |

### `src/subprogram/SubPrg.c`

- 变换: 电机控制公共算法库, 6 个纯函数 (`atan` / `PID` / `PIDLongRegulate` / `PID32` / `BurrFilter` / `SlipFilter`), 不碰外设、不碰全局量, 全部输入输出都走 struct。改写手法全部是**保持求值语义的结构变换**: (1) `atan`: x==0 的两级写法收成提前返回 (`return (y<0)?(-16384):16384;`), 尾部的 `if(x<0) m_Angle = m_Angle + 32768;` 改写为 `m_Angle = qatan(m_Input) + ((x < 0) ? 32768 : 0);` —— 同为 int 加法, 加 0 不改变值; (2) `PID`: 把 `m_KpSat`/`m_KiSat` 两个界与 `m_Max`/`m_Min` 的求值顺序重排为 "先两个界、再两个限幅基量、再 KP 链、再 KI 链" (这些语句互不依赖, 且都只是移位, 不受 `SETOVM` 的 OVM 标志影响, `SETOVM`/`CLROVM` 的包裹范围原样保留); 抗积分饱和的 `if(A&&B)...else if(C&&D)...` **保持原形状** —— 改成两个独立 if 不等价, 因为第一个分支会改 `pid->Total`, 从而影响第二个分支的 `pid->Total < 0` 判据; (3) `PIDLongRegulate`: `<<4` 补上与 `*` 的优先级括号 (优先级本来就如此), `m_OutKi` 提到分支前算 (分支不读它); (4) `PID32`: `m_OutKi` 提到分支之前算 (分支只读 `m_OutKp`); (5) `BurrFilter`: 末尾两条顺序执行的钳位 `if` 改写为两条**独立**的三元 (`Err = (Err>Max)?Max:Err` 与 `if(Err>Max) Err=Max;` 逐分支相同; 不能合成 else-if, 因为 Max 与 Min 可能反序, 合成后第二个钳位会被跳过); (6) `SlipFilter`: 历史搬移循环从 `for(i=0;i<15;i++) d[i]=d[i+1]` 改成 `for(i=1;i<16;i++) d[i-1]=d[i]` (同为整体左移一格), `(16-1)` 写成 `15`。**没有**改任何判据常量、移位位数、饱和界、写入字段、写入次序或类型宽度。原工程的 bug 原样保留: `PIDLongRegulate` 的输出上限判据读的是**输入** `pid->Out` 而不是刚算出的 `m_Out` (变异 PL4 就是把它改成 `m_Out`, 已验证 CAUGHT)。

- 必须守住的不变量:

  - **函数名与外部符号不变**: 6 个入口名 (`atan`/`PID`/`PIDLongRegulate`/`PID32`/`BurrFilter`/`SlipFilter`) 一字不动。目标端 (C28x) 外部符号集合两侧完全相同, 7 个: `_BurrFilter`/`_PID`/`_PID32`/`_PIDLongRegulate`/`_SlipFilter`/`_atan`/`_qatan` (target_check.py 按 COFF storage class == C_EXT 逐项核对, `符号集合相同: True`)。两侧编译告警各 0 条, 新增 0 条、消失 0 条。
  - **差分测试: 改写后交付件 cases=517311, mismatches=0** (docs/_difftest/SubPrg.json, nrand=200000)。测试台对 6 个入口全部直驱: 每个入口都有"同一份输入装进两侧各自的 struct、各调一次、整块 memcmp"的驱动相, 另有 `sweep_pid32` 两段 (门两侧的 `Deta` 符号 x `FilterTime` 边界 x 钳位边界)、`sweep_save16/save32/reset` 式的枚举扫值, 以及随机状态 + 随机操作序列 (1~6 步)。
  - **负向对照 54/54 全 CAUGHT, 0 BROKEN, 0 HUNG**, 见 docs/_difftest/_negctl_summary.json (全库总账 3587/3587 CAUGHT, 其中 SubPrg 贡献 54 条: AN1~AN6 定 atan 的零判据、两支角度、定点输入移位与 x<0 补偿; PI1~PI14+PI16 定 PID 的 KP/KI 选源、饱和移位、还原移位、衰减量、抗饱和逻辑、累积方向、输出合成、饱和上下限与回写; PL1~PL9 定 PIDLongRegulate 的缩放、衰减、KI 源、输出上限判据 (含原工程 bug 的形态)、上下限赋值、抗饱和逻辑与累积方向; P31~P38 定 PID32 的两处缩放、`Deta>0` 判据、上下限的合成与钳位、输出饱和量; BU1~BU8 定 BurrFilter 的跟随判据、加倍/减半、半额判据、两个钳位、求差方向与跟随本身; SL1~SL8 定 SlipFilter 的搬移范围、新样本落点、加权起点、加权个数、权重递增、权重和方向与归一幅)。
  - **首轮 55 条变异 53 CAUGHT / 2 MISSED, 逐条分类后关闭** (本模块的负向对照纪律记录): PI15 (`long vMax = 0x7FFFFFFF;` -> `0x7FFFFFFE`) 证明为 **(b) 语义等价** —— 判据是宽度事实而非输入分布: vMax 全文只出现在两处 `__IQsat` 的界里, KP 项界为 `vMax>>(4+QP)` (QP 是 Uint, 移位量恒 >=4, 两界右移后逐位相同), KI 项界为 `vMax>>QI` (QI>=1 时同上), 唯一可能分叉的 QI==0 情形被 `|KI*Deta| <= 32768*32768 = 2^30 < 0x7FFFFFFE` 挡死 (KI/Deta 在目标端与宿主都被钉成 16 位 int), `__IQsat` 永不触发。实测证据 build/_me_pid_vmax_bound.c (输出 build/_me_pid_vmax_host.log): `shift k>=1 identical: yes`、`bound unreachable: yes`、对照组把界换成真能观测的 0x07FFFFFF 后同一个 `__IQsat` 立刻分出不同结果 (排除"饱和分支压根不执行")。已按纪律**从清单删除**, 证词留在 tools/difftest/negative_control.py 的 `CASES['SubPrg']` 注释块。 P33 (`if(pid->Deta > 0)` -> `>=`) 属于 **(a) 测试台有洞**: 首轮 `sweep_pid32` 只扫了 `Deta > 0` 一侧, `Deta == 0` 的 else 支从未被驱动。已在 driver.c 的 `sweep_pid32` 增加第二段把 `Deta` 的 0 / 两侧符号 x `Max/Min` 边界全部扫到, 重跑即 CAUGHT, 变异保留在清单里。
  - **宿主保真靠 host_patch + shim 宽度钉死**: (a) `#define  SETOVM asm(" SETC OVM")` 与 `CLROVM` 在宿主上被 host_patch.json 两侧同时置空 (MSVC 不认 C28x 内联汇编); 本模块没有任何语句依赖 OVM 标志位, 置空后两侧语义不变; (b) 用例私有 shim 的 DSP2803x_Device.h 末尾 `#define int short` (否则 `typedef unsigned int Uint;` 在宿主上是 32 位), 把 `int` 钉成 16 位, 与目标端一致 —— 这是 PID/LongRegulate/PID32 里 `(long)KP * (long)Deta` 的乘积宽度不出偏差的前提; (c) `SubPrgInclude.h` 的 `(long int)` -> `(long)` 两条原型补丁 (同 MotorEncoder 用例同源)。
  - **按工程真实优化选项编译, 改写未收敛到同一份机器码** (docs/_difftest/SubPrg.o2.json): `-o2`(Debug) / `-o3`(Release) 下 `dis2000 -1` 反汇编分别有 427 / 422 行不同 (`.text` 0x1ae->0x1a7 / 0x1ad->0x1a9 字), 主要来自语句重排后寄存器分配的变化 (`SlipFilter` 的搬移循环被优化成 `RPT #14 ||PREAD` 的形式与原来不同)。即本模块与多数改写模块同级: **行为等价由差分测试 + 负向对照担保**, 不像 `f_vf.c`/`f_frqSrc.c`/`MotorDebug.c` 那样在真实优化选项下逐行收敛。

| 项 | 结果 |
|---|---|
| 差分用例数 | **517,311** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_BurrFilter`, `_PID`, `_PID32`, `_PIDLongRegulate`, `_SlipFilter`, `_atan`, `_qatan` |
| `.text` 尺寸 | 571 → 567 (**-4**) |

### `src/motor/MotorInvProtect.c`

- 变换: 执行机构保护模块: 软件错误处理、五张温度表的温度检查、输出缺相、输入缺相、风机控制、变流/电机两侧过载积分、CBC 逐周期限流、负载丢失与缓冲电阻过热。改写只做**结构变换**: 算式一个字不改, 只动控制流与访问路径 —— (1) InvDeviceControl 删掉一个空 `{ }` (原注释被剥掉后留下的壳), 调用顺序原样; (2) SoftWareErrorDeal: gError 经局部指针 pErr, 折掉一个外层已保证的非零判断, 过压/欠压改"过压提前 return"; (3) TemperatureCheck: 两处 if/else 赋值收成三元, 二分查找 `while` 改 `for` (自增表达式等价, 循环体次数不变), 命中判定的 `else if` 改 `if` (前一分支必 break); (4) OutputPhaseLoseDetect: 最值由两组嵌套三元改成"先取 U 再逐相折叠"; (5)/(10) 各结构体统一经局部指针访问; (6) InputPhaseLoseDetect: 尾部 if/else 按德摩根律取反; (7) ControlFan: 尾部 if/else-if 改"命中即 return"; (8)/(9) 预警位与 CBC 三相标志累加的 if/else 收成三元; (11) LoadLoseDetect: 九项 `||` 守卫先算进局部量再判, m_Limit 下限的 if 化; (12) BufferResProtect: 过热置位改 `|= (cond) ? ... : 0`, **先判旧值再自减的顺序保留**。

- 必须守住的不变量:

  - 函数名 InvDeviceControl / SoftWareErrorDeal / TemperatureCheck / OutputPhaseLoseDetect / OutputLoseAdd / OutputLoseReset / InputPhaseLoseDetect / ControlFan / OverLoadProtect / CBCLimitCurProtect / SetCBCEnable / SetCBCDisable / LoadLoseDetect / BufferResProtect 与全部全局量名不变
  - 所有算术表达式的运算集合、次序、宽度不变 —— 移位量/位宽/整数提升/宏展开逐字保留, 只改控制流与访问路径
  - InvDeviceControl 的调用顺序不变, 参数不变
  - 三张过载表 (gInvOverLoadTable / gInvOverLoadTableForP / gMotorOverLoadTable) 与 五张温度表的数值逐字不变, 两个递减表的数值也逐字不变
  - 过载积分的钳位/查表/增量公式逐字不变 (含 `m_LDeta` 的 256 倍钳位判据)
  - BufferResProtect 的"先判旧值再自减"顺序不变 (换序会改变过门限点)
  - CBCLimitCurProtect 的三相计数钳位、m_Sub 的"先 1 后判 STOP 改 2"、三相标志累加语义不变
  - gTemperature / gError / gFanCtrl / gOverLoad / gCBCProtect / gLoadLose / gPhaseLose / gInLose 的写入条件、写入值、写入顺序不变
  - 输出文件为纯 ASCII、只含 CRLF、无注释 (与交付树其余文件同一把尺子)

| 项 | 结果 |
|---|---|
| 差分用例数 | **61,694** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AdcResult`, `_BufferResProtect`, `_CBCLimitCurPrepare`, `_CBCLimitCurProtect`, `_ControlFan`, `_EPwm1Regs`, `_EPwm2Regs`, `_EPwm3Regs`, `_GpioDataRegs`, `_InputPhaseLoseDetect`, `_InvDeviceControl`, `_LoadLoseDetect`, `_OutputLoseAdd`, `_OutputLoseReset`, `_OutputPhaseLoseDetect`, `_OverLoadProtect`, `_SetCBCDisable`, `_SetCBCEnable`, `_SoftWareErrorDeal`, `_TemperatureCheck`, `_gBuffResCnt`, `_gCBCProtect`, `_gComPar`, `_gError`, `_gFanCtrl`, `_gIUVWQ12`, `_gIUVWQ24`, `_gInLose`, `_gInvInfo`, `_gInvOverLoadDecTable`, `_gInvOverLoadTable`, `_gInvOverLoadTableForP`, `_gLineCur`, `_gLoadLose`, `_gMainCmd`, `_gMainStatus`, `_gMotorInfo`, `_gMotorOverLoadDecTable`, `_gMotorOverLoadTable`, `_gOverLoad`, `_gPWM`, `_gPhase`, `_gPhaseLose`, `_gSubCommand`, `_gTempTableBSMXX`, `_gTempTableP44X`, `_gTempTableP8XX`, `_gTempTableSEMIKON`, `_gTempTableWAIZHI`, `_gTemperature`, `_gUDC` |
| `.text` 尺寸 | 1554 → 1693 (**+139**) |

### `src/motor/MotorMain.c`

- 变换: MotorMain: the 2 ms / carrier / ADC-ISR task layer (15 entry points). Structural rewrite only, no arithmetic touched. (1) Every switch rewritten as an if / else-if chain over the SAME case order with the default arm last (Main2msMotorB first switch: 7 arms; Main2msMotorB second switch: 8 arms; RunCaseRun2Ms: 6 arms; ADCOverInterrupt: 7 arms). (2) Ternary -> if/else: Main2msMotorB's gOutVolt.VoltPhaseApply (16384 / -16384) and CalRatioFromVot's gRatio clamp. RunCaseRun2Ms' '?:' on gFeisu.SpeedLast becomes a temp + if/else that still performs exactly one write to SpeedLast. (3) Guard-clause / early-return: Main0msMotor returns early when PGMode != 0; RunCaseStop's ShortGnd if/else loses the else because the if arm returns; Main2msMotorB's 'MotorType != PM' branch is inverted to test the positive first. (4) SendDataPrepare's outer 4-term guard is inverted to if(!(...)) so the compute arm reads first and the zero arm is the else. (5) Repeated side-effect-free reads factored into a local at the point of use, with no call between the two uses: gBasePar.FcSetApply (RUN_SYNC_TUNE arm, type Uint) and gVFPar.FreqApply (ASYNC_VF/SYNC_VF arm of RunCaseRun2Ms, type int). (6) Magic numbers given block-scope const names: 80 (KP scale divisor), 16384 (half-Q15), 50 / 1500 / 4096 (InitForMotorApp defaults), 8192 / 287 (CalRatioFromVot). No file-scope object was added. (7) Locals renamed: m05HzPu -> m_Hz50Pu, tempU -> m_PosSum, mAiCounter -> m_SampleCnt, mTotal1..3 -> m_Sum1..3, mRatio -> m_VoltRatio, m_Ratio -> m_RatioTmp; every local keeps its original type.

- 必须守住的不变量:

  - Function names and the file-level symbol set are unchanged: the 15 entry points and the six forward declarations are literal, and the four #include lines are literal (MotorInclude.h, MotorDataExchange.h, SystemDefine.h, MotorDefine.h). No file-scope object, no new static function and no new static variable was added.
  - Target-side (C28x) check passes: pass=true, rc_ref=0, rc_new=0, warnings 15 / 15 with warn_added=[] and warn_gone=[], symbols_ref == symbols_new (symbols_equal=true, 145 C_EXT names: the module's 15 entry points plus 130 referenced callees and globals; the _gComPar reference is kept because the SpdSearchMethod if/else keeps both arms). .text ref=1532 new=1525, delta -7 words (recorded, not a gate).
  - Differential test: docs/_difftest/MotorMain.json, cases=22487, mismatches=0, rc=0 (identical case count and zero mismatches before and after the rewrite).
  - The unconditional m20HzPu = (200L<<15) / gBasePar.FullFreq01 kept exactly, including the division, because its operands are the input and a divisor of zero is a real behavioural case. The local was deliberately NOT renamed: its name appears in the target warning text ('variable "m20HzPu" was set but never used'), so renaming it turned one baseline warning into an 'added' one and failed target_check. Every other renamed local is actually read and therefore warning-free.
  - Every side-effecting macro is invoked exactly as often and in the same order as before: DisableDrive / EnableDrive / TurnOffFan, the single DINT/EINT pair in SendDataPrepare (observed as gHostCpuCtl), RT_SAMPLE_START / RT_SAMPLE_END, ROTOR_TRANS_RD / ROTOR_TRANS_SCLK (inside the inactive TMS320F28035 block), GetOverUdcFlag() and speed_DEC. The Filter4 / Filter32 call sites are byte-identical, so their multi-evaluation of the 'total' argument is unchanged, and the one asm(" NOP") in the PWM-disabled arm is preserved (observed as gAsmCount).
  - All explicit casts keep their original form and position: (long), (Ulong), (Uint), (int); no integer arithmetic was simplified and no shift / multiply was moved. The guarded divisions (the two m05HzPu = (50L<<15)/gBasePar.FullFreq01 computations) stay inside their guards; the unconditional divisions (m20HzPu, mTotal/mAiCounter, gUDC.uDC / gInvInfo.BaseUdc, CalRatioFromVot's two) are unchanged.
  - Write set, write order and call order into other modules' globals are unchanged; the differential harness compares the whole 56-global image plus the 73-entry call ledger, so a dropped or reordered write or call could not pass.
  - Delivered file is pure ASCII, CRLF line endings, zero comments.

| 项 | 结果 |
|---|---|
| 差分用例数 | **22,517** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ADCOverInterrupt`, `_AlphBetaToDQ`, `_BrakeResControl`, `_CalCarrierWaveFreq`, `_CalDeadBandComp`, `_CalIMSet`, `_CalOutputPhase`, `_CalRatioFromVot`, `_CalTorqueLimitPar`, `_CalTorqueUp`, `_CalUdcLimitIT`, `_CalWsAndSynFreq`, `_CalWsAndSynFreq_380`, `_CalcABVolt`, `_ChangeCurrent`, `_ChangeMotorPar`, `_ChgParForEst`, `_GetCurExcursion`, `_GetCurrentInfo`, `_GetMDetaPos`, `_GetMTTimeNum`, `_GetRotorTransPos`, `_GetUDCInfo`, `_GetUvwPhase`, `_GpioDataRegs`, `_HVfDeadBandComp`, `_HVfOscDampDeal`, `_HardWareOverUDCDeal`, `_IPMCheckInitPos`, `_InitForMotorApp`, `_InvCalcPower`, `_InvDeviceControl`, `_LmIoInPeriodInt`, `_Main05msMotor`, `_Main0msMotor`, `_Main2msMotorA`, `_Main2msMotorB`, `_Main2msMotorC`, `_Main2msMotorD`, `_OutPutPWMVF`, `_OutputLoseAdd`, `_PWMZeroInterrupt`, `_ParGet05Ms`, `_ParGet2Ms`, `_ParSend05Ms`, `_ParSend2Ms`, `_PmChkInitPosRest`, `_PmDecoupleDeal`, `_PmFluxWeakDeal`, `_PrepImCsrPara`, `_PrepPmsmCsrPrar`, `_PrepareAsrPar`, `_PrepareForSpeedCheck`, `_PrepareParForRun`, `_PrepareParForTune`, `_ResetParForVC`, `_RunCaseDcBrake`, `_RunCaseGetPar`, `_RunCaseIpmInitPos`, `_RunCaseLowPower`, `_RunCaseRun2Ms`, `_RunCaseShortGnd`, `_RunCaseSpeedCheck`, `_RunCaseStop`, `_RunStateParChg2Ms`, `_SVCCalFlux_380`, `_SVCCalRotorSpeed`, `_SVCCalRotorSpeed_380`, `_SendDataPrepare`, `_SoftPWMProcess`, `_SvcCalOutVolt`, `_SvcCalOutVolt_380`, `_SynInitPosDetect`, `_SystemParChg05Ms`, `_SystemParChg2Ms`, `_TurnToStopStatus`, `_VCCsrControl`, `_VCGetFeedBackSpeed`, `_VFAutoTorqueBoost`, `_VFOverMagneticControl`, `_VFSpeedControl`, `_VFWSCompControl`, `_VFWsTorqueBoostComm`, `_VcAsrControl`, `_VfFreqDeal`, `_VfOscIndexCalc`, `_VfOverCurDeal`, `_VfOverUdcDeal`, `_VfVarInitiate`, `_gADC`, `_gAI`, `_gBasePar`, `_gCBCProtect`, `_gComPar`, `_gCpuTime`, `_gCtrMotorType`, `_gCurSamp`, `_gDCBrake`, `_gDeadBand`, `_gError`, `_gEstBemf`, `_gExtendCmd`, `_gFVCSpeed`, `_gFcCal`, `_gFeisu`, `_gFeisuNew`, `_gFluxR`, `_gFluxWeak`, `_gGetParVarable`, `_gHVfOscDamp`, `_gIAlphBeta`, `_gIAmpTheta`, `_gIMTQ12`, `_gIMTQ12_obs`, `_gIMTQ24`, `_gIMTQ24_obs`, `_gIMTSet`, `_gIPMInitPos`, `_gIPMPos`, `_gIPMPosCheck`, `_gIPMZero`, `_gIUVWQ12`, `_gIUVWQ24`, `_gImAcrQ24`, `_gInvInfo`, `_gItAcrQ24`, `_gLineCur`, `_gMainCmd`, `_gMainStatus`, `_gMotorInfo`, `_gOutVolt`, `_gPGData`, `_gPWM`, `_gPmParEst`, `_gRatio`, `_gRotorTrans`, `_gSpeedFilter`, `_gTestDataReceive`, `_gUDC`, `_gUVCoff`, `_gUVWPG`, `_gVCPar`, `_gVFPar`, `_gVarAvr`, `_gWsComp` |
| `.text` 尺寸 | 1532 → 1525 (**-7**) |

### `src/system/main.c`

- 变换: 整机入口: main() 的 0.5ms/2ms/0ms 调度主循环 + 三个中断服务程序 (`ADC_Over_isr` / `EPWM1_zero_isr` / `EPWM1_TZ_isr`)。改写手法: (1) **删死代码**: `#if 0 ... #endif` 里的 `cpu_timer0_isr` (整块不可达), 以及正文里 10 处 `#ifdef CPU_TIME_DEBUG ... #endif` 计时段 —— 工程两种构建(project/md380.pjt 的 Debug/Release 选项) 都只定义 `_DEBUG / DSP2803X / LARGE_MODEL`, `CPU_TIME_DEBUG` 从未定义, 这些块预处理后一行不剩。(`gCpuTime` 的 `Det05msClk` / `tmp0Ms` 两个写在 #ifdef 外面, 是活代码, 原样保留。) (2) 主循环 `while(1)` 改写成等价的 `for(;;)`; (3) 相位分配的四级 `if((m_LoopFlag & 0x03) == k) ... else if ...` 链改写为 `m_Phase = m_LoopFlag & 0x03;` + `switch(m_Phase)` 的 case 0/1/2/3 —— 数值被 0x03 掩过, switch 的四支与原链逐支对应, 函数调用序列逐条相同; (4) 三个 ISR 只做空白归一 (原文件制表符/空格混用), **语句序列一动没动** —— 每个 ISR 都是一串不可重排的副作用 (EALLOW/EDIS 配对、清中断标志、开/关中断、取时间、调回调、写 PIEACK), 任何重排都会改变目标端行为; (5) extern 声明块保持原样 (本模块对外接口)。**没有**改任何判据常量 (C_TIME_05MS / 65535 / 0x03)、写入字段、调用次序或类型宽度。

- 必须守住的不变量:

  - **函数名与外部符号不变**: `main` / `ADC_Over_isr` / `EPWM1_zero_isr` / `EPWM1_TZ_isr` 四个入口名一字不动。目标端 (C28x) 外部符号集合两侧完全相同, 33 个 (4 个入口 + 29 个 extern 引用: InitSysCtrl/InitInterrupt/InitPeripherals/InitForMotorApp/InitForFunctionApp/EnableDog/SetInterruptEnable/KickDog/ADCOverInterrupt/PWMZeroInterrupt/HardWareErrorDeal/Main05msMotor/Main0msMotor/Main2msMotorA~D/Main05msFunctionA~D/Main0msFunction/AdcRegs/EPwm1Regs/PieCtrlRegs/GpioDataRegs/CpuTimer1/gCpuTime)。target_check.py 按 COFF storage class == C_EXT 逐项核对, `符号集合相同: True`; 两侧编译告警各 0 条。`.text` 尺寸 ref=97 new=95 字 (删死代码 + 相位链改 switch 的结果)。
  - **差分测试: 改写后交付件 cases=200120, mismatches=0** (docs/_difftest/main.json, nrand=200000)。测试台对四个入口全部直驱: 三个 ISR 各按 5 个时钟步长 + 2 个回绕时钟直调; main() 由终止神谕驱动 —— mock 的 `gHostGetTime()` 在超过 driver 设定的调用预算时 longjmp 出来, 等价于"最多跑几拍"。时钟模型: 目标端 `GetTime()` 是 CpuTimer1 的**下计数器**, 全工程时间差都写 `base - now`, 宿主持一个按 driver 设定的步长递减的虚拟时钟, 于是 `m_DetaTime` 每拍长一个 step。扫 step 在 C_TIME_05MS(=30000) 及其整除因子 (15000/10000/6000/5000/3000/1000/100) 与倍数 (60000/90000/120000/200000/0x7FFFFFFF/0xFFFFFFFF) 上, 共 20 个步长 x 5 个预算 + 3 个回绕时钟 + 200000 个随机 (step 偏向门槛及其整除因子, budget 4~64)。
  - **负向对照 38/38 全 CAUGHT, 0 BROKEN, 0 HUNG** (docs/_difftest/_negctl_partial_main.json, 合并进主账后为 docs/_difftest/_negctl_summary.json)。38 条变异 (MN1~MN38) 覆盖: 0.5ms 门的 `>=`/`>` 与阈值 +1、时间基 `-=` 变 `+=` 与初值改常量、时间差方向取反、相位计数自增/自减/初值、相位掩码与四个 `==k` 支、喂狗调用删除、0ms 两任务次序、0ms 拍计数方向、Det05msClk 饱和上下限、0.5ms 主任务删除、各支内任务删除与成对次序、初值化五段的次序、三个 ISR 的 PIEACK 组号、ETCLR 写 1/0、TZ 中断里 DisableDrive 与错误处理的次序、EINT 的位置、DINT 删除、两个中断的执行时间差方向、ADC 的两个寄存器写删除、两个中断的应用回调删除。
  - **测试台首轮漏 2 条, 逐条分类后确认是 (a) 测试台有洞, 已堵**: MN25 (TZ 中断里 `DisableDrive(); HardWareErrorDeal();` 次序对调) 与 MN27 (ADC 中断里 `EINT;` 挪到 `gCpuTime.ADCIntBase = GetTime();` 之后)。病因相同: **寄存器写入与日志事件之间的先后在宿主上看不见** —— `DisableDrive()` 在真 SystemDefine.h 第 123 行是宏 (`GpioDataRegs.GPASET.bit.GPIO7 = 1`), 只改最终寄存器值; `GetTime()` 只推进虚拟时钟。两者互换位置后, 事件日志、计数器、寄存器终值全都一样。修法 (不动被测代码): (a) 给 `GetTime()` / `DisableDrive()` / `ADC_CLEAR_INT_FLAG` / `ADC_RESET_SEQUENCE` 四个原来是"只写不说"的操作补上事件日志 (写入本身一字不改); (b) mock 在**每个事件点**再存一份寄存器踪迹 `gHostRegTrace[][5]` (ADCINTFLGCLR/SOCPRICTL/ETCLR/PIEACK/GPASET), 于是连源码里直接写的 `PieCtrlRegs.PIEACK.all = ...` 相对邻句的次序也能比出来。改完先用同一份代码两侧对跑复核测试台 (cases=2120, mismatches=0), 再重跑全量: 38/38。
  - **宿主保真的两处特判**: (a) `#define int short` 本用例**没有**用 —— 本模块挂在 `int` 上的语义只有 `m_LoopFlag & 0x03` 与 `tmp0Ms ++`, 取值都在小范围, 两侧同一份 shim 同宽同行为; 真正参与门槛比较的 `m_DetaTime` 是 `Ulong` (32 位, 宿主与目标端一致)。(b) `KickDog` 在真工程里经 DSP2803x_GlobalPrototypes.h 的宏转成 `ServiceDog`, 本用例的 mock 直接定义 `KickDog` (调用点行为不变); `DisableDrive` 在目标端是 GPIO7 写, 在宿主被接上事件日志 (见上)。

| 项 | 结果 |
|---|---|
| 差分用例数 | **200,120** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ADCOverInterrupt`, `_ADC_Over_isr`, `_AdcRegs`, `_CpuTimer1`, `_EPWM1_TZ_isr`, `_EPWM1_zero_isr`, `_EPwm1Regs`, `_EnableDog`, `_GpioDataRegs`, `_HardWareErrorDeal`, `_InitForFunctionApp`, `_InitForMotorApp`, `_InitInterrupt`, `_InitPeripherals`, `_InitSysCtrl`, `_KickDog`, `_Main05msFunctionA`, `_Main05msFunctionB`, `_Main05msFunctionC`, `_Main05msFunctionD`, `_Main05msMotor`, `_Main0msFunction`, `_Main0msMotor`, `_Main2msMotorA`, `_Main2msMotorB`, `_Main2msMotorC`, `_Main2msMotorD`, `_PWMZeroInterrupt`, `_PieCtrlRegs`, `_SetInterruptEnable`, `_gCpuTime`, `_main` |
| `.text` 尺寸 | 97 → 95 (**-2**) |

### `src/system/mainInit_2803x.c`

- 变换: 整机 bring-up: 16 个入口, 全是"往外设寄存器写常量 + 挂 128 个中断向量 + 调 4 个后续初始化"。改写手法全部是**结构变换**, 表达式、常量、字段、类型一个字没动: (1) 同一个 EALLOW/EDIS 块内的互不依赖的寄存器写**重排** (InitFlash 的 4 个等待寄存器、InitPll 的 7 个 CLKCTL 位与 MCLKOFF/DIV 一对、InitPeripheralClocks 的 4 个时钟寄存器、InitSetGpio 的 13 个 GPIO 寄存器、InitSetAdc 的 4 个上电位与 14 个 SOC配置、SetInterruptEnable 的 3 个 PIEIER、InitInterrupt 的 5 个向量挂载); (2) `IER = 0x0000; IFR = 0x0000;` 两句对调 (紧邻且中间无事件); (3) copy_prg 的长度调整改括号 `(Uint)crp->size - 1` -> `(Uint)(crp->size - 1)`; (4) 两个指针自增循环改成下标循环 (`*(p++) = v` -> `p[i] = v`): InitPieCtrl 的 PIEIER/PIEIFR 交错清零、InitPieVectTable 的 128 格向量填充; (5) `while (c) { }` -> `while (c);`。

- 必须守住的不变量:

  - **函数名与外部符号不变**: 16 个入口 (`InitSysCtrl` / `InitInterrupt` / `SetInterruptEnable` / `InitPeripherals` / `copy_prg` / `InitFlash` / `KickDog` / `DisableDog` / `EnableDog` / `InitPll` / `InitPeripheralClocks` / `InitPieCtrl` / `InitPieVectTable` / `InitSetGpio` / `InitSetAdc` / `rsvd_ISR`) 与全局 `gErrorIntCnt` 一字不动。target_check.py 按 COFF storage class == C_EXT 核对, 两侧外部符号集合完全相同 (35 个), 编译告警各 1 条且文本相同 (`function "MovePrgFrFlashToRam" declared implicitly` —— 原工程如此, 该函数定义在 system/DSP2803x_CodeStartBranch.asm 里, 全工程没有原型)。`.text` 尺寸 ref=402 new=406 字 (差 +4, 来自重排后寄存器分配变化)。
  - **差分测试: 改写后交付件 cases=20283, mismatches=0** (docs/_difftest/mainInit_2803x.json, nrand=20000)。测试台对 16 个入口全部直驱, 并对三种输入做扫描: (a) **寄存器文件 4 种脏预填** (全 1 / 全 0 / 0xAAAA-0x5555 交替 / 种子随机) —— 只有一种模式会漏掉一半变异: 全 1 模式下"写 1"不可见, 全 0 模式下"写 0"不可见; IER/IFR 的预填额外把低 5 位 (M_INT1..M_INT5) 按 0 处理, 否则 `IER |= mask` 恒不变; (b) **PLLSTS 的两个选择位** MCLKSTS (0/1, 选整个外肢) 与 DIVSEL (0..3, 选内层预清), PLLLOCKS 钉 1 (见覆盖缺口); (c) **copy_prg 的 COPY_TABLE** src/dst/size 由 driver 填, size 扫 0/1/2/0x7FFF/0x8000/0xFFFF/0x10000/0x12345/0x7FFFFFFF/0xFFFFFFFE/0xFFFFFFFF; InitPll 的 val 扫 0..65535 的 26 个代表值 (PLLCR.DIV 只有 4 位, 截断本身就是行为)。
  - **可比的不只是寄存器终值**: `EALLOW/EDIS/DINT/EINT` 在 shim 里被换成 `gHostCtlEvent(i)` —— 既自增共享计数表, 又往事件日志追加一条 {事件号, 那一刻 7 个寄存器块 + IER/IFR + CpuTimer1Regs + 128 格向量分类的 32 位指纹}。因此"寄存器写值相对 EALLOW/EDIS 的先后""多一对还是少一对 EALLOW/EDIS""向量表哪一格没填"全部可观测。5 个被调函数 (InitSetPWM/InitCpuTimers/MovePrgFrFlashToRam 记三个参数/Device_cal) 进事件日志, 被取地址的 5 个 ISR 是 mock 提供的共享桩 (两侧写同一个指针, 直接可比值)。
  - **负向对照 69/69 全 CAUGHT, 0 BROKEN, 0 HUNG** (docs/_difftest/_negctl_partial_mainInit_2803x.json, 已并入主账 docs/_difftest/_negctl_summary.json)。69 条变异 (MI1~MI69) 覆盖全部 16 个入口的寄存器常量、位域写 0/1、循环次数、指针基址、调用删除、语句对调、判据反向。首轮 69 跑出 8 条 MISSED, 逐条分类后**全部是测试台有洞, 没有一条是等价变异**, 已全部堵上并重跑: **MI28/MI29/MI34** (DIVSEL 预清值、MCLKOFF 置位值、InitPll 尾部两个 EALLOW/EDIS 块次序) —— 病因是 shim 里 EALLOW/EDIS 只自增计数不记事件, 于是"写值与 EALLOW 的先后"完全不可见; 补上事件日志与逐事件寄存器指纹后三条立刻 CAUGHT。**MI10** (`IER |=` 掩码漏 M_INT5) 与 **MI68** (ADC 中断标志清除被删) —— 病因是寄存器预填值把要写的位已经置成 1 (`IER` 低 5 位全 1 / ADCINTFLGCLR.bit.ADCINT1 已 1), 改为 4 种预填变体 + IER 低 5 位按 0 后 CAUGHT。**MI2/MI18/MI20** 三条原来的写法本身不构成 bug: `DSP_CLOCK/10` 与 `DSP_CLOCK/9` 在这块板子上都等于 6; `READ_RAND_FLASH_WAITE` 与 `READ_PAGE_FLASH_WAITE` 都是 3; KickDog 的第一个 WDKEY 写总被第二个 (0x00AA) 覆盖, 中间没有任何事件 —— 已分别改成真差异 (分频系数 +1 / 改用 READ_OTP_WAITE=8 / 删 EALLOW-EDIS 对), 重跑全部 CAUGHT。
  - **改写过程中差分测试当场抓到一个真错误并逼回正路**: copy_prg 的第一版把 `size = (Uint)crp->size - 1;` 折成调用实参 `(Uint)(crp->size - 1)`, 丢掉了"赋给 16 位 `Uint size` 时发生的截断" —— size=0 时 ref 传 65535、new 传 4294967295, 20 处不一致。截断必须保留: 目标端 `Uint` 是 16 位而 `crp->size` 是 32 位。改为 `size = (Uint)(crp->size - 1);` 后 20283 例 0 不一致。
  - **宿主保真三处特判**: (a) `Device_cal` 是 ROM 地址 0x3D7C80 的函数指针, 宿主直跳必段错误, host_patch 把它改指到 mock 空壳 (目标端编译用的还是原文件); `MovePrgFrFlashToRam` 同理, 它在 system/DSP2803x_CodeStartBranch.asm 里。(b) `StartCpuTimer1()` 在真 DSP2803x_CpuTimers.h 里是**宏** (写 CpuTimer1Regs.TCR.bit.TSS), 不是调用 —— mock 因此**不**给它造桩, CpuTimer1Regs 整块进快照, 那一次写才是被观测的对象。(c) `asm(" RPT #7 "" || "" NOP")` 与 `asm(" RPT #28 || NOP")` 是 C28x 内联汇编, MSVC 不认, host_patch 删掉 (两侧同样删, 只影响硬件延时, 不影响两侧可比性); `#pragma CODE_SECTION` 宿主忽略。

| 项 | 结果 |
|---|---|
| 差分用例数 | **20,283** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ADC_Over_isr`, `_AdcRegs`, `_CpuTimer1Regs`, `_DisableDog`, `_EPWM1_TZ_isr`, `_EPWM1_zero_isr`, `_EnableDog`, `_FlashRegs`, `_GpioCtrlRegs`, `_GpioDataRegs`, `_InitCpuTimers`, `_InitFlash`, `_InitInterrupt`, `_InitPeripheralClocks`, `_InitPeripherals`, `_InitPieCtrl`, `_InitPieVectTable`, `_InitPll`, `_InitSetAdc`, `_InitSetGpio`, `_InitSetPWM`, `_InitSysCtrl`, `_KickDog`, `_MovePrgFrFlashToRam`, `_PieCtrlRegs`, `_PieVectTable`, `_SCI_RXD_isr`, `_SCI_TXD_isr`, `_SetInterruptEnable`, `_SysCtrlRegs`, `_copy_prg`, `_gErrorIntCnt`, `_prginRAM`, `_rsvd_ISR` |
| `.text` 尺寸 | 402 → 406 (**+4**) |

### `src/motor/MotorSVC.c`

- 变换: SVC (定子电压定向) 磁链观测器与转速估算器, 4 个入口: ResetSVC / SVCCalFlux_380 / SVCCalRotorSpeed / SVCCalRotorSpeed_380。改写手法: (1) ResetSVC 的 20 个清零赋值按目标对象重新分组 (原顺序是 FluxR/FluxS/SVCSpeed/MainCmd/低通/comp 交错), 全部写互不重叠的对象; (2) `gVCPar.SvcMode` 原本在 SVCCalFlux_380 里读两次, 缓存到 m_SvcMode 再判; (3) 两条支路里逐字重复的纯子表达式提成局部量: `((long)gMotorExtPer.LM*5000L/(long)gBasePar.FullFreq01)` -> m_LmScale, `((long)gIMTSetApply.M>>12)` -> m_MSet, `((long)gIMTSetApply.T>>12)` -> m_Tset; (4) Alpha / Beta 两条支路原来共用一个 m_Long 顺序执行, 拆成 m_LongA / m_LongB 使两条支路互不依赖 (每条链都以一次新赋值开头, 旧值不被读); (5) `m_LowVolt = 300; if(InvTypeApply>=22) m_LowVolt = 80;` 改成条件表达式; (6) I0 / (M>>12) 两路除数先选进 m_Div 再统一做一次除法; (7) `m_IntData` 的两条三元表达式改成 if / else if / else; (8) FirstCnt 的 `if(...){...return;} else if(...) else` 改成顺序 if; (9) 平方和表达式去掉冗余括号。**所有常量、移位量、掩码、字段、类型、运算顺序一个字没动。**

- 必须守住的不变量:

  - **函数名与外部符号不变**: 4 个入口 (`ResetSVC` / `SVCCalFlux_380` / `SVCCalRotorSpeed` / `SVCCalRotorSpeed_380`) 与 8 个数据符号 (`gFluxR` / `gFluxS` / `gCalFlux` / `gSVCSpeed` / `gABFluxS_LowFilter` / `gABFluxS_comp` / `gIMTQ24_obs` / `gIMTQ12_obs`) 一字不动。target_check.py 按 COFF storage class == C_EXT 核对, 两侧外部符号集合完全相同 (30 个), 编译告警各 1 条且文本相同 (`function "atan" declared implicitly` —— 原工程如此: 全工程没有 atan 的双参原型, 它的定义在 src/subprogram/SubPrg.c 里)。`.text` 尺寸 ref=909 new=857 字 (差 -52, 来自三处子表达式提成局部量)。
  - **差分测试: 改写后交付件 cases=200404, mismatches=0** (docs/_difftest/MotorSVC.json, nrand=20000)。测试台对 4 个入口全部直驱, 并做三件事: (a) 每次调用把 8 个模块全局量与 14 个共享输入全局量整体 memcmp; (b) 每段输入先 memset 0xA5 再铺真实值, 于是"某字段不再被写"可见; (c) `do_seq` 序列相用上一次调用产出的状态决定下一次调用的 Timer, 逐步快照逐字段比较 —— 抓"状态演化错了但单步自洽"。
  - **16 位存储宽度被钉死**: case shim 用 `#define int short` 把 int/Uint 折成目标的 16 位, main() 里断言 `sizeof(Uint)==2 && sizeof(int)==2 && sizeof(long)==4 && sizeof(Uint32)==4 && sizeof(llong)==8`, 不成立就 `return 2`。没有这条, 下面三处纯截断型差异会全部藏起来: `m_DetaTimer = (((Timer - GetTime()) & 0xFFFFFF)>>3)` 的 24->16 位折叠、`DetaPhase = gFluxR.Theta - SvcLastFluxPos` 的 16 位回绕、`(Uint)((long)gPhase.IMPhase>>16)`。`bench_selfcheck()` 再用三组独立手算值复核这几处截断, 测试台自己漂了会直接 FATAL。
  - **无符号语义一字未改**: `gSVCSpeed.SvcSynSpeed = (((long)gSVCSpeed.SvcSynSpeed * (1024 - m_ULong) + (long)gMainCmd.FreqSyn * m_ULong) >> 10) + m_IntData;` 里 `(1024 - m_ULong)` 是 Ulong, 使整个乘法/加法/右移全部走**无符号**语义, `>>10` 是逻辑右移而不是算术右移。这是原工程的既有语义, 改写里逐字保留。同理 `((Ulong)m_ULong<<7)/(abs(gMainCmd.FreqSyn))` 也是无符号除。
  - **两处读未初始化局部量被原样保留** (原工程固有问题, 按契约不改): `SVCCalFlux_380` 的 `m_DetaTimer` 在 SvcMode != 0 时不赋值却参与 `gSVCSpeed.DetaTimer += m_DetaTimer;`; `SVCCalRotorSpeed_380` 的 `m_Speed` 在 VCSpeedFilter > 1 时先读后写。测试台在每次调用前用 0x5A **常量**铺 8 KB 栈 (位置相关的填充会让两侧同一槽位拿到不同值), 于是两侧读到同一个 0x5A5A5A5A, 比较的是表达式而不是垃圾值。
  - **负向对照 171/171 CAUGHT, 0 BROKEN, 0 HUNG** (docs/_difftest/_negctl_partial_MotorSVC.json, 4 切片 43/43/43/42 合并; 总台账 docs/_difftest/_negctl_summary.json 3865/3865)。变异锚点用 (行号, 行内子串) 由 build/_svc_add_negctl.py 构造, 生成时已断言: 每条锚点在基准文件里只出现一次、替换确实改了源码、任意两条变异产生的源码互不相同; build/_audit_anchors.py 对 MotorSVC 报 0 违规。
  - **I0 分支的除数提取不改变除法语义**: 原式 `((long)gMotorExtPer.R2<<11)/gMotorExtPer.I0` 在宿主上是 `long / unsigned int` (16 位无符号提升到 32 位无符号) = 无符号除, 而提成 `long m_Div` 后是有符号除。分子 `(long)R2<<11` 恒为正 (R2 是 16 位无符号, 最大 65535<<11 = 1.34e8 < 2^31), 分母也恒为正 (进这一支的前提是 I0 > M>>12), 两种除法的商完全相同。

| 项 | 结果 |
|---|---|
| 差分用例数 | **502** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CpuTimer1`, `_ResetSVC`, `_SVCCalFlux_380`, `_SVCCalRotorSpeed`, `_SVCCalRotorSpeed_380`, `_atan`, `_gABFluxS_LowFilter`, `_gABFluxS_comp`, `_gABVoltSet`, `_gBasePar`, `_gCalFlux`, `_gFluxR`, `_gFluxS`, `_gIAlphBetaQ12`, `_gIMTQ12`, `_gIMTQ12_obs`, `_gIMTQ24_obs`, `_gIMTSet`, `_gIMTSetApply`, `_gInvInfo`, `_gMainCmd`, `_gMotorExtPer`, `_gMotorInfo`, `_gPhase`, `_gRatio`, `_gSVCSpeed`, `_gTestDataReceive`, `_gVCPar`, `_qsin`, `_qsqrt` |
| `.text` 尺寸 | 909 → 857 (**-52**) |

### `src/motor/MotorPWM.c`

- 变换: PWM 输出级: 5 个入口 InitSetPWM / OutPutPWMVC / OutPutPWMVF / DeadBandComp / SendPWM。这是全工程第一个直接写外设寄存器的模块 (EPwm1/2/3Regs), 也是第一个带 `#if 0` 死代码块的模块。改写手法: (1) InitSetPWM 里三个通道逐字重复的 12 条公共寄存器写 (TBPRD/TBPHS/TBCTL/HSPCLKDIV/CMPCTL/CMPA/AQCTLA/AQSFRC/AQCSFRC/DBCTL/DBFED/DBRED) 提成 SetPwmTimerBase(pReg, tbCtl), 三处 TZSEL/TZCTL/TZEINT 提成 SetPwmTripZone(pReg), 三处 `#ifdef TMS320F2808` 收成一个 C_TZ_SEL 宏加一处 `#ifndef`; (2) OutPutPWMVC 三相正弦的三段相同算式提成 OutputOnePhase(), 原来靠 `phase = phase - 21845;` 连改两次, 改成把 `phase - 21845` 与 `phase - 21845 - 21845` 当实参传 (trunc16 对减法同态, 逐位相同); (3) OutPutPWMVF 的 `m_pUVW = (Ulong *)&gPWM.U; *(m_pUVW + sel) = len;` 三字指针游走改成 SetPhaseLength() 的显式 switch; (4) DeadBandComp 的三相死区补偿 + __IQsat 提成 DeadBandCompOne(); (5) SendPWM 的三相"清标志 -> 判零 -> 改 AQCTLA/CMPCTL" 提成 SendOneChannel(); (6) `phase = (phase > 65531)?65531:phase;` 改成 if; 嵌套 if 的悬挂体补齐大括号。**所有常量、移位量、掩码、比较运算符、iTrig 五张表的初值一个字没动。**

- 必须守住的不变量:

  - **函数名与外部符号不变**: 5 个入口 (`InitSetPWM` / `OutPutPWMVC` / `OutPutPWMVF` / `DeadBandComp` / `SendPWM`) 与 10 个数据符号 (`gOutVolt` / `gPWM` / `gRatio` / `EPWMFlagU/V/W` / `iTrig1_Cpwm` / `iTrig1_Dpwm` / `iTrig2_Pwm` / `iTrig3_Cpwm` / `iTrig3_Dpwm`) 一字不动; 新增的 4 个 helper 全部是 `static` (不是 `inline` —— 见 docs/equivalence_evidence.md 里 `void inline` 那条: MSVC 会把两个 TU 的同名 COMDAT 静默折叠成一个, 函数体另一侧整个变死代码)。target_check.py 按 COFF storage class == C_EXT 核对, 两侧外部符号集合完全相同 (23 个), 编译告警各 3 条且文本相同。`.text` 尺寸 ref=596 new=584 字 (差 -12, 来自 helper 复用)。
  - **差分测试: 改写后交付件 cases=217585, mismatches=0** (docs/_difftest/MotorPWM.json)。测试台把 EPwm1/2/3Regs 三块**真实**外设寄存器结构体 (来自真 include/device/DSP2803x_EPwm.h, 不是手写替身) 整体 memcmp, 于是"写对了值但写错了寄存器"可见; 每次调用前把整块寄存器 image 连同 gADC/gDeadBand/gPhase 一起存/取, 因为 SendPWM 会**回读** `EPwm*.CMPA.half.CMPA != 0` 再决定写 AQCTLA 的哪个值 —— 上一拍的寄存器内容是这一拍的输入。
  - **EPWM 寄存器块布局被钉住**: main() 断言 `sizeof(struct EPWM_REGS) == 116` (宿主值, 由真头文件算出来)。整块 memcmp 依赖这个布局, 头文件一改它就会悄悄变成"比较别的东西", 所以钉死。`PWM_OUT_STRUCT` 故意**不**按尺寸钉: 它含一个 enum, C28x 存 16 位而 MSVC 存 32 位, sizeof 两侧本来就不同; 两侧用的是同一个宿主结构, padding 完全一致, 既造不出也藏不住差异。
  - **16 位存储宽度被钉死**: case shim 用 `#define int short` 把 int/Uint 折成目标的 16 位, main() 断言 `sizeof(Uint)==2 && sizeof(int)==2 && sizeof(long)==4 && sizeof(Uint32)==4`, 不成立就 `return 2`。没有这条, 下面这些纯截断型差异会全部藏起来: `m_iT1Length`/`m_iT2Length`/`m_iZeroLength` 都是 Uint, 而 `((long)SinAlfa60 * (long)gRatio)>>15` 是 32 位结果 —— 赋值时的16 位截断正是这个模块的主要行为之一; `gPWM.U = (((long)SinU * (long)gPWM.gPWMPrdApply)>>13)` 里 SinU 也是 16 位。bench_selfcheck() 用两组独立手算值 (`(Uint)0x00018000 == 0x8000`、`(Uint)(((long)30000*6000)>>12) == 43945`) 复核这两处截断, 测试台自己漂了会直接 FATAL。
  - **指针游走改显式 switch 不改变语义**: 原式 `m_pUVW = (Ulong *)&gPWM.U;` 依赖 PWM_OUT_STRUCT 里 U/V/W 三个 long 紧邻 (成立), 按下标 0/1/2 写三个字; 五个 iTrig 表的 12 个扇区逐格核过, 每个扇区取到的三个下标恰好是 {0,1,2} 的一个排列, 所以三条 `*(m_pUVW + sel)` 正好覆盖 U/V/W 且不重不漏。显式 switch 的 default 不可达 (见 dead_code)。
  - **InitSetPWM 的 TZEINT 不对称被保留**: 原文件三个 `#ifdef TMS320F2808` 块并不对称 —— 2808 变体下 EPwm2/EPwm3 **不写** TZEINT, 而 EPwm1 两个变体都写。改写把公共部分收进 SetPwmTripZone() 并在其中用 `#ifndef TMS320F2808` 决定是否写 TZEINT, 再在 EPwm1 处无条件补一次 0x0004: 两个变体下三种通道的最终寄存器值都与原文逐位相同。
  - **负向对照 174/174 CAUGHT, 0 BROKEN, 0 HUNG** (docs/_difftest/_negctl_partial_MotorPWM_s*.json, 4 切片合并)。变异锚点用 (行号, 行内子串) 由 build/_pwm_add_negctl.py 构造, 生成时已断言: 每条锚点在基准文件里只出现一次、替换确实改了源码、任意两条变异产生的源码互不相同; build/_audit_anchors.py 对 MotorPWM 报 0 违规。首轮是 172 CAUGHT / 4 MISSED / 1 BROKEN, 逐条分类后: 3 条 (PWM60/63/112) 证明等价后删除, 1 条 (PWM107) 换成良定义的变异, 1 条 (PWM98) 是替换文本写错导致变体编不过。
  - **`#if 0` 块与三个未用局部量原样保留**: DeadBandComp 的 `#if 0` 块里用到 `m_Max`/`m_Min`/`m_Deta`, 所以那三个声明必须留着 —— 否则把 `#if 0` 改成 `#if 1` 就编不过。代价是两侧各多一条"变量未使用"告警, 与原文一致。

| 项 | 结果 |
|---|---|
| 差分用例数 | **217,585** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_DeadBandComp`, `_EPWMFlagU`, `_EPWMFlagV`, `_EPWMFlagW`, `_EPwm1Regs`, `_EPwm2Regs`, `_EPwm3Regs`, `_InitSetPWM`, `_OutPutPWMVC`, `_OutPutPWMVF`, `_SendPWM`, `_gADC`, `_gDeadBand`, `_gOutVolt`, `_gPWM`, `_gPhase`, `_gRatio`, `_iTrig1_Cpwm`, `_iTrig1_Dpwm`, `_iTrig2_Pwm`, `_iTrig3_Cpwm`, `_iTrig3_Dpwm`, `_qsin` |
| `.text` 尺寸 | 596 → 584 (**-12**) |

### `src/function/f_debug.c`

- 变换: 示波器/调试采样模块。9 个外部入口, 但 8 个是空体: UniversalDebug32 / PowerOnDebug / InitKeyDebug / InitI2cDebug / ChgAttr / DealKeyDebug / TestI2cDebug 的函数体整块在 `#if 0` 里, InitDebug 只是调用其中三个空函数。实测 .text 102 字, 其中 UniversalDebug 占 95 字 (93%)。改写手法: (1) UniversalDebug 的六路 switch (case 1..5 + default) 改成两张 static 查表 gScopeBuf[] / gScopeLen[] —— 守卫 `sn >= SCOPE_NUMBER` 已把 sn 限在 [0,6), switch 的六支恰好覆盖这六个值且每支只赋那一对值, 原 default 分支不可达 (它做的事等价于"保留初值", 而表里第 0 项就是初值那一对); (2) 上升沿清数组的 `for (j = scopeNum-1; j >= 0; j--)` 改成递减 `while (k != 0U) { k--; ... }`, 去掉 int16 循环变量, 边界与写入顺序都相同; (3) 下标边界与回绕那段把 `scopeIndex[sn]` 读进局部 idx 再写回, 消掉`*(pScope + scopeIndex[sn])` 里的重复全局读和 `++scopeIndex[sn]` 的读-改-写嵌套 (安全性依赖两条已核对的事实: pScope 指向 scope0..5, 与 scopeIndex[] 不别名; matlabDebug[] 全工程只有初始化式一处写入, 所以 *matlabDebug[sn] 也指不到 scopeIndex[])。**所有常量、比较运算符、`#if` 配置阶梯、matlabDebug[] 与 scopeInterval[] 的初始化式一个字没动; 八个空体函数与全部 `#if 0` 块整块保留。**

- 必须守住的不变量:

  - **函数名与外部符号不变**: 9 个入口 (`UniversalDebug` / `UniversalDebug32` / `PowerOnDebug` / `InitDebug` / `InitKeyDebug` / `DealKeyDebug` / `InitI2cDebug` / `TestI2cDebug` / `ChgAttr`) 与 21 个数据符号 (`scope0..scope5` / `scopeIndex` / `scopeInterval` / `matlabDebug` / `matlabDebugRsvd` / `vDL` / `keyValueDebug` / `keyValueDebugIndex` / `I2CWrongTimer` / `sendDataI2C` / `rcvDataI2C` / `i`) 一字不动。target_check 实测两侧外部符号集合相同, 30 个符号。新增的两张查表都是 `static` (不是 `inline` —— 见 docs/equivalence_evidence.md 里 `void inline` 那条: MSVC 会把两个 TU 的同名 COMDAT 静默折叠成一个, 让另一侧的函数体变成死代码), 所以不进外部符号集。
  - **八个空体函数整块保留, 一个字符都没改**。它们的函数体全在 `#if 0` 里, 目标端各自只有一条 `LRETR`, 没有实现可以变; 任何改动只会在 .text 布局 上留痕迹而不改变行为。`InitDebug` 的三次调用同样保留 —— cl2000 在 -O0 下也会把对空体函数的调用整个删掉 (实测 `_InitDebug` 只有 1 个字), 改成加参数或加判断会让它变成多字, Tier A 立刻不等。
  - **`SCOPE_START_RUN_FLAG` 保持出现三次, 不提到局部变量**。它是 `(runFlag.bit.run)`, 一个非 volatile 全局的读。提成局部会把三次读变成一次: 单线程下结果相同, 但 `runFlag` 是运行状态, 若将来有中断在其中改写它, 原版三次读能看到 0->1 的变化而提取后的一次看不到。这不是等价变换, 只是"当前没有中断写它"的巧合, 契约要求功能不变, 所以不做。
  - **配置常量与初始化式不动**: `SCOPE_NUM_KK == 1`, `SCOPE_NUM0..3 == SCOPE_NUM_KK`, `SCOPE_NUM4 == SCOPE_NUM5 == 1`, `SCOPE_NUMBER == 6`, `scopeInterval[] == {2,2,2,2,2,2}`, `SCOPE_START_RUN_CLEAR` / `SCOPE_START_RUN_ARRAY_0` / `SCOPE_SAVE_ONLY_RUN` / `SCOPE_ARRAY_USER_OVER_LOOP` 全为 1, `matlabDebug[]` 前 3 项指向 `frq/frqRun/frqTmp`、后 3 项指向 `matlabDebugRsvd`。差分测试的 bench_selfcheck() 逐项断言了这些 (表指向哪里、interval 仍是出厂值), 改任何一项都会立刻报 FATAL 而不是静默通过。
  - **行为等价**: 差分测试 150226 例 0 处不一致, 覆盖跑满 (guard=13 idle=32768 edge=15 store=32985 storeMiss=264 wrap=32985 wrapMiss=0, 六个通道各 11305+ 次), 记录在 docs/_difftest/f_debug.json。改写前后覆盖数字逐项相同 —— 说明三处结构改动没有让任何一个分支变得不可达。
  - **负向对照 34/34 全部 CAUGHT**, 0 BROKEN, 0 HUNG, 记录在 docs/_difftest/_negctl_partial_f_debug.json。变异锚点用 (行号, 行内子串) 构造, 生成时已断言每条锚点在基准文件里只出现一次、替换确实改了源码、任意两条变异产生的源码互不相同 (build/_dbg_add_negctl.py); build/_audit_anchors.py 复核 34 条 0 违规。
  - **交付格式**: 纯 ASCII, 无注释, CRLF。11636 -> 11525 字节, 562 -> 566 行。`#if 0` 块里的代码原样保留 (它们不参与编译, 但属于交付源码, 改它们只会制造与行为无关的差异)。

| 项 | 结果 |
|---|---|
| 差分用例数 | **150,226** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ChgAttr`, `_DealKeyDebug`, `_I2CWrongTimer`, `_InitDebug`, `_InitI2cDebug`, `_InitKeyDebug`, `_PowerOnDebug`, `_TestI2cDebug`, `_UniversalDebug`, `_UniversalDebug32`, `_frq`, `_frqRun`, `_frqTmp`, `_i`, `_keyValueDebug`, `_keyValueDebugIndex`, `_matlabDebug`, `_matlabDebugRsvd`, `_rcvDataI2C`, `_runFlag`, `_scope0`, `_scope1`, `_scope2`, `_scope3`, `_scope4`, `_scope5`, `_scopeIndex`, `_scopeInterval`, `_sendDataI2C`, `_vDL` |
| `.text` 尺寸 | 162 → 120 (**-42**) |

### `src/function/f_comm_DP.c`

- 变换: DP (显示面板) SCI 通讯模块。14 个外部入口: 两个中断 `SCI_DP_RXD_isr` / `SCI_DP_TXD_isr`, 一个 inline 发送启动 `CommDpStartSend`, 收发帧处理 `CommDpRcvDataDeal` / `CommDpSendDataDeal` / `SciDpDeal` / `ErrAgainSend`, 初始化 `InitSetSciDp` / `UpdateSciDpFormat` / `InitSciaGpioDp` / `InitScibGpioDp` / `InitSciDpBaudRate`, 参数 `GetDataByteNum` / `DpDataDeal`。改写手法 (共 9 个差异块): (1) **两处逐语句相同的 8 句通道复位序列** (RXD 中断的 else 臂、`CommDpRcvDataDeal` 的 CRC 错误超限臂) 抽成 `static void ResetDpChannel(struct SCI_DATA_DP *p)` —— 这是本模块唯一真正重复的代码, 原来两份连空格数量都一致; (2) RXD 中断里 4 句的"收满一帧"序列抽成 `static void CompleteDpFrame(p, length)`, 顺带把 `(dataByteNum + 4)` 提成局部 `frameLen` (原式在同一个 if 里求值三次, 且 `dataByteNum` 是全局, 提取同时消掉一处宿主/目标整型宽度的潜在分歧); (3) **三处两行的 CRC 落盘** (高位先写、低位后写) 抽成 `static void StoreCrc(Uint16 *pBuf, Uint16 loIndex, Uint16 crc)`, 三处的下标表达式各不相同 (dataByteNum+3/+2、+2/+1), 由参数给出; (4) `InitScibGpioDp` 改为直接调用 `InitSciaGpioDp` —— 两个函数体**逐语句、逐常量、逐寄存器相同** (已逐行核对: 同一组 GPAPUD/GPAQSEL2/GPAMUX2 位、同一对 PieVectTable 赋值、同一句 IER |= M_INT9、同一对 PIEIER9 使能), 只差缩进; (5) `CommDpRcvDataDeal` 把 `dataByteNum + 2` 提成 `crcLow`, 三处分散的`+3` / `+2` / `+2` 改写成 `crcLow+1` / `crcLow` / `crcLow`, 让"CRC 低位下标"这个名字只出现一次; (6) `InitSetSciDp` / `UpdateSciDpFormat` 把 `p->pSciRegs` 提成 `volatile struct SCI_REGS *pReg` —— 写寄存器序列与写顺序一个字节没动; 小改: `ErrAgainSend` 的 `for` 改递减 `while`、`GetDataByteNum` 的 `dataByteNums[dpParameter.dpDataFormat - 1]` 提成局部 `format`。**所有常量、比较运算符、`#if` 配置阶梯、`dataByteNums[]` 初始化式、五个条件编译死区整块保留。**

- 必须守住的不变量:

  - **函数名与外部符号不变**: 14 个入口与 13 个数据符号 (`sciM380DpData` / `sendData_DP` / `rcvData_DP` / `dpSciCommRcvFlag` / `dpSciCommSendFlag` / `dpParameter` / `dataByteNum` / `dataByteNums` / `dpTest` 等) 一字不动。target_check 实测两侧外部符号集合相同, 27 个。新增的三个辅助函数都是 `static` (不是 `inline` —— 见 docs/equivalence_evidence.md 里 `void inline` 那条), 不进外部符号集。.text 476 -> 466 字 (-10): 抽函数消掉重复代码带来的净减少, 方向合理且幅度小。
  - **`CommDpStartSend` 的 `inline` 关键字保留**。这是全库第二个 `void inline` 函数 (第一个是 `MotorCurrentTransform`)。目标端 (TI cl2000) 对 C11 `inline` 定义**不发外部符号**, 去掉 `inline` 会凭空多出一个外部符号, 直接破坏上面的符号集合断言。宿主端 (MSVC) 会把它发成 linkonce/COMDAT, 两个 TU 各一份同名定义会被链接器**静默折叠成一个** —— 于是 `ref_tu.c` / `new_tu.c` 里必须手工加 `#define CommDpStartSend ref_/new_CommDpStartSend` (nm2000 推出的改名清单里没有它), 并且 driver 要**直接驱动**它。这两件事都做了, DP 变异里针对它的几条全部 CAUGHT, 说明没有落进 COMDAT 折叠陷阱。
  - **`InitSetSciDp` 的写寄存器序列与写顺序逐字节保留**。这个函数在 `NO_FIFO == 1` 分支下写了 7 次寄存器, 其中 `SCICTL1` / `SCICTL2` / `SCICCR` 各被写两次 (先 0x0001/0x00C2/0x0087, 后 0x0023/0x00C3/0x0087)。**前三次写会被后三次覆盖, 但它们在真机上有意义**: `SCICTL1 = 0x0001` 是 SWRESET 位, 写它会把 SCI 状态机复位, 0x0023 再释放复位 —— 这是"先复位再配置"的时序, 不是冗余。宿主测试台里寄存器是普通内存, 看不到这个脉冲 (见 coverage_gaps)。所以只做了 `p->pSciRegs` -> `pReg` 的指针提取, 七次写一个没删、顺序一个没换。
  - **`#define FIFO_NUM  16;` 的尾分号原样保留**。它带一个分号, 只在 `NO_FIFO == 1` 时才能编译 (那时 `FIFO_NUM` 只出现在被编掉的 `#else` 臂里, 从不展开)。这是原工程的一颗地雷: 一旦把 `NO_FIFO` 改成 0, `for (i = 0; i < FIFO_NUM; i++)` 会展开成 `i < 16;; i++` 而编不过。改它等于改配置阶梯的语义, 所以不动, 只记录。
  - **配置常量与初始化式不动**: `DEBUG_F_DP == 1`, `BUG_SCI_BACK_DATA == 0`, `NO_FIFO == 1`, `SCI_SEND_READ_NUMBER == 5`, `SEND_DATA_NUMBER == RCV_DATA_NUMBER == 32`, `RCV_CRC_ERR_NUMBER == 10`, `dataByteNums[] == {12,20,0,4,32}`, `dpParameter == {3,5}`。差分测试的 `bench_selfcheck()` 逐项断言了这些, 改任何一项会立刻报 FATAL 而不是静默通过。
  - **行为等价**: 差分测试 20991 例 0 处不一致, 覆盖跑满 (rxd pre=15 second55=15 secondNot=60 reset=60 guard=115 store=40 full=5 skip=15 flagOther=210; txd send=6 skip=8; rcv nosig=200 flagOther=150 crcOk=125 crcBad=125 reset=360 noReset=240; send yes=15 no=15; dpdeal early=2 pass=2; upd err=1 noerr=1; fmt 12/20/0/4/32 各 4 次; start=4 err=18 gpioA=2 gpioB=2 setDp=4 baud=4), 记录在 docs/_difftest/f_comm_DP.json。比较的是**整个模块状态**: `sciM380DpData` 逐字段 + `rcvData_SCI[]` / `sendData_SCI[]` 逐元素 + 文件级全局 + `SciaRegs` / `PieCtrlRegs` / `GpioCtrlRegs` / `PIE_VECT_TABLE` / `IER` 逐字节 memcmp。
  - **负向对照 106/106 全部 CAUGHT**, 0 BROKEN, 0 HUNG, 记录在 docs/_difftest/_negctl_partial_f_comm_DP.json (四切片 27/27/26/26, 已合并)。这是本工程第一个**首轮零 MISSED** 的模块。锚点用 (行号, 行内子串) 构造, 生成时已断言每条锚点在基准里只出现一次、替换确实改了源码、任意两条变异产生的源码互不相同 (build/_dp_add_negctl.py); build/_audit_anchors.py 复核 106 条 0 违规。两条锚点特意带制表符 (`			p->pSciRegs->SCIHBAUD`) —— `#if 0` 里的 switch 死支与活支有完全相同的赋值文本, 不带缩进会打错地方。
  - **交付格式**: 纯 ASCII, 无注释, CRLF。11768 -> 11355 字节, 456 -> 460 行 (行数变多是三个 static 辅助函数各自的函数头/花括号, 字节数变少是重复语句被消掉)。

| 项 | 结果 |
|---|---|
| 差分用例数 | **20,991** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CommDpRcvDataDeal`, `_CommDpSendDataDeal`, `_CrcValueByteCalc`, `_DpDataDeal`, `_ErrAgainSend`, `_GetDataByteNum`, `_GpioCtrlRegs`, `_InitSciDpBaudRate`, `_InitSciaGpioDp`, `_InitScibGpioDp`, `_InitSetSciDp`, `_PieCtrlRegs`, `_PieVectTable`, `_SCI_DP_RXD_isr`, `_SCI_DP_TXD_isr`, `_SciDpDeal`, `_SciaRegs`, `_UpdateSciDpFormat`, `_dataByteNum`, `_dataByteNums`, `_dpParameter`, `_dpSciCommRcvFlag`, `_dpSciCommSendFlag`, `_dpTest`, `_rcvData_DP`, `_sciM380DpData`, `_sendData_DP` |
| `.text` 尺寸 | 476 → 466 (**-10**) |

### `src/motor/MotorPmsmMain.c`

- 变换: PMSM 初始定位 / 零位检测 / Ld-Lq 辨识 / 弱磁 / 解耦模块。10 个入口: EQEP 零位中断 `PG_Zero_isr`, 初始位置标定 `IPMCheckInitPos` / `PmChkInitPosRest` / `RunCaseIpmInitPos`, 位置折算 `SynCalRealPos`, 电感辨识 `SynCalLdAndLq`, 电流环系数 `IPMCalAcrPIDCoff` / `PrepPmsmCsrPrar`, 弱磁 `PmFluxWeakDeal`, 解耦 `PmDecoupleDeal`。改写手法 (共 9 个差异块, 12470 -> 11342 字节): (1) `PG_Zero_isr` 的 ABZ/UVW 两条零位判据抽成 `static int ZeroErrCount(void)` —— 原来 `errZFlag` 是 ISR 里的局部变量, 在两条支路里各自 `++`, 最后 `if(errZFlag)`; 改写后由函数返回计数, `if(ZeroErrCount())`。`GetUvwPhase()` 的调用位置 (在 UVW 支路的第一条判据之后、第二条之前) 一字未动; (2) `PG_Zero_isr` 的 `if(RunStep != STATUS_GET_PAR) {...} else {...}` 改成早退加落空, 消掉一层嵌套 —— 两支互斥且都不在结尾之后, 等价; (3) 两处逐字相同的 QEP 回绕修正 (QEP 累计与 ABZ 累计) 抽成 `static void QepTotalWrap(long * pTotal)`, `mSign` 随之搬进辅助函数; (4) `IPMCheckInitPos` 的活代码从三层嵌套 `if/else` 改成三条早退, `abs(SpeedTemp) < 100` 取反成 `>= 100`; (5) `RunCaseIpmInitPos` 的 `case 1` 改成先判错再干活的早退形式; (6) `RunCaseIpmInitPos` 的 `case 2` 里那条 `if(abs(PowerOffPosDeg - RotorPos) < 3641) { }` 空语句整条删除 —— 两支都是空语句序列, 条件只读两个字段无副作用, 可证明等价 (见 dead_code); (7) `SynCalRealPos` 删掉第二次重复的 `m_4Pluse = ((long)PulseNum)<<2` —— 两次之间只有 `GetQepCnt()` 一个宏 (`(*EQepRegs).QPOSCNT`), 无副作用; (8) `PrepPmsmCsrPrar` 的 `if/else` 改成 `if { ...; return; }` 加落空, 并删掉从未被引用的局部 `int Nf`; (9) `PmFluxWeakDeal` 里两处逐字相同的 IdSet 钳位抽成 `static void FluxWeakIdClamp(int maxCur)`, 删掉函数末尾多余的 `return;`。所有算术表达式逐字符保留: `(long)x<<15`、`>>7`、`% (PulseNum<<2)`、`(m_CntMod<<14)/(int)PulseNum`、`m_Angle += 43691`、`(3*total+x)/4` 形式的 `Filter4` 调用、`__IQsat` 的四个限幅常量, 一个字符都没改 —— 这个模块的危险全在 16/32 位截断点上, 动表达式就是动语义。

- 必须守住的不变量:

  - 函数名与外部符号不变: 10 个入口与 5 个模块自有数据对象 (`gIPMPos` / `gFluxWeak` / `gPMInitPos` / `gIPMPosCheck` / `gPmDecoup`) 一字不动。target_check 实测两侧外部符号集合相同, 49 个。新增的三个辅助函数都是 `static` (不是 `inline` —— 全库那个 `void inline` 的 COMDAT 折叠陷阱见 rewrite_decisions D16), 不进外部符号集。`.text` 995 -> 1022 字 (+27), 方向合理: 多出三个函数框架, 少掉一段重复的 if/else 嵌套。
  - 所有算术表达式逐字符保留。这是本模块最强的一条不变式, 也是改写范围被刻意收窄的原因。C28x 上 `int` 是 16 位、`long` 是 32 位, 而模块里到处是先升位、算完再截断回 16 位的写法。举例: `m_Pos = (mCntMod<<14)/(int)gPGData.PulseNum;` 里 `m_Pos` 是 `int`, 所以商在赋值处被截断到 16 位; 如果改写时把它留在 `long` 里 (例如直接写 `gIPMZero.FeedPos = (mCntMod<<14)/(int)PulseNum * Poles`), 结果会在 `mCntMod` 较大时分叉, 而且只在少数输入上分叉。同理 `m_CoffA = m_CoffA>>1; m_CoffB = m_CoffB>>1; Ld = m_CoffA - m_CoffB;` 不能合并成 `Ld = (m_CoffA - m_CoffB)>>1` —— 两次截断各丢一次余数。
  - `PG_Zero_isr` 的寄存器写序列与写顺序不变: `(*EQepRegs).QCLR.all = 0x0401;` 与 `PieCtrlRegs.PIEACK.all = PIEACK_GROUP5;` 夹在 `EALLOW` / `EDIS` 之间, 四条语句的先后一字未动。`gIPMZero.zFilterCnt = 8;` 在 `gIPMZero.Flag = (gIPMZero.Flag | 0x01);` 之前, 也一字未动 —— 两者都写 `gIPMZero`, 顺序有语义。
  - `errZFlag` 的计数语义不变: 原代码 `errZFlag` 初值 0, ABZ 支路最多 `++` 一次 (1), UVW 支路最多 `++` 两次 (1 或 2), 判据是 `if(errZFlag)` 即非零。改写后 `ZeroErrCount()` 返回 0/1/2, 判据 `if(ZeroErrCount())` 等价。没有把它简化成 `= 1`, 因为那样就丢掉了第二条判据也失败这一事实的可观测性 (虽然当前只判非零, 但保留计数更贴近原意)。
  - `GetUvwPhase()` 的调用次数与位置不变: `PG_Zero_isr` 里 UVW 支路一次 (在第一条判据之后、第二条之前)、`RunStep == STATUS_GET_PAR` 分支末尾一次、`IPMCheckInitPos` 入口一次。三处都由 `gMockCall[]` 计数比较, 差分测试 113392 例零不一致。
  - `PrepPmsmCsrPrar` 的除法链不变: `temp = 32767L - gMotorInfo.FreqPer;` 然后 `temp = (((long)AbsFrqLpf - FreqPer)<<5) / temp;` —— `temp` 被复用为分母再被覆写为商, 这个同名两用原样保留。改写只是把 `else { ... }` 换成 `if(...) { ...; return; }` 加落空, 以及删掉未被引用的 `int Nf`。
  - `PmFluxWeakDeal` 的两级 IdSet 钳位位置不变: 第一处在 Mode 判断之前 (无条件), 第二处在 `Mode == 2` 分支之内、`IdSet += temp1` 之后。抽成 `FluxWeakIdClamp(maxCur)` 只搬函数体, 不搬调用点。`maxCur` 仍是 `4096L * gFluxWeak.IdMax / 100` 赋给 `int` (截断点不变)。
  - 差分测试: `MotorPmsmMain difftest: cases=113392 mismatches=0`, rc=0, `SELFCHECK: ok`, 27 个可达性计数器全部非零 (`zGuard=9216 zAbz=576 zUvw=576 zOtherPg=1152 zTuneNoLoad=192 zTuneWithLoad=192 zTuneOther=384 cFirst=192 cEarly=576 cLowHigh=32 cLowZero=16 cLowMid=48 cNotLow=96 rEarly=105 rCase1=3 rCase2=3 rCase3=3 rCase4=3 rDefault=9 pAbz=198 pUvw=198 lRan=75575 dRan=625 prepSimple=390 prepGain=450 fMode2=438 fOther=915)。
  - 负向对照: 175 条变异, 全部 CAUGHT, 0 MISSED, 0 BROKEN。首轮 182 条抓出 12 条 MISSED, 逐条分类后没有一条是模块的问题: 5 条是测试台的洞 (已补, 见 cases/MotorPmsmMain/README.md 第 6 节), 7 条可证明等价或不可达 (已证明并从清单删除)。
  - 格式: CRLF、无注释、纯 ASCII (484 行 / 12434 字节), `verify_tree.py` 通过。
  - 条件编译阶梯原样保留: 三处 `#if 0` / `#if 1 ... #else ... #endif` 全部在位 (`grep -n '#if'` 可见), 只把 `#if 0` 块里引用的局部名跟着所在函数一起改名 (`errZFlag` -> `errCnt`)。`#if 0` 不参与编译, 所以保留与否 `.text` 都是 1022 字 —— 实测过两次, 一次带块一次不带, 目标文件尺寸与符号集合完全相同。
  - 告警不增加: 目标端 `cl2000` 编译 ref 3 条告警 / new 2 条, 新增告警 0 条, 消失的 1 条是 `variable Nf was declared but never referenced` (删掉的那个未用局部)。剩下的两条 (`TurnToStopStatus` / `PrepareParForRun` 隐式声明) 是原代码就有的, 来自头文件缺原型, 与本次改写无关。

| 项 | 结果 |
|---|---|
| 差分用例数 | **113,392** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_EQepRegs`, `_GetUvwPhase`, `_GpioDataRegs`, `_IPMCalAcrPIDCoff`, `_IPMCheckInitPos`, `_InitSetAdc`, `_InitSetPWM`, `_PG_Zero_isr`, `_PieCtrlRegs`, `_PmChkInitPosRest`, `_PmDecoupleDeal`, `_PmFluxWeakDeal`, `_PrepPmsmCsrPrar`, `_PrepareParForRun`, `_RunCaseIpmInitPos`, `_SetIPMPos`, `_SetIPMPos_ABZRef`, `_SetInterruptEnable`, `_SynCalLdAndLq`, `_SynCalRealPos`, `_SynInitPosDetSetPwm`, `_TurnToStopStatus`, `_gBasePar`, `_gError`, `_gFVCSpeed`, `_gFluxWeak`, `_gGetParVarable`, `_gIMTQ24`, `_gIMTSet`, `_gIPMInitPos`, `_gIPMPos`, `_gIPMPosCheck`, `_gIPMZero`, `_gImAcrQ24`, `_gItAcrQ24`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtInfo`, `_gMotorExtPer`, `_gMotorExtReg`, `_gMotorInfo`, `_gOutVolt`, `_gPGData`, `_gPMInitPos`, `_gPmDecoup`, `_gPmParEst`, `_gUVWPG`, `_gVCPar`, `_qsin`, `_qsqrt` |
| `.text` 尺寸 | 995 → 1022 (**+27**) |

### `src/motor/MotorPmsmParEst.c`

- 变换: PMSM 参数辨识/初始位置辨识模块。13 个入口: 初始化 `InitSetPosTune`、初始位置脉冲等待 `SynTuneInitPos`、初始位置检测状态机 `SynInitPosDetect`、位置写回 `SetIPMPos` / `SetIPMPos_ABZRef`、Ld/Lq 相位计算 `SynCalLabAndLbc` / `SynInitPosDetCal`、载波/脉宽配置 `SynInitPosDetSetTs` / `SynInitPosDetSetPwm`、零位检测 `DetectZeroPosOnce`、空载/带载零位辨识 `SynTunePGZero_No_Load` / `SynTunePGZero_Load`、反电势辨识 `SynTuneBemf`。模块自有 4 个文件级对象 (`gIPMZero` / `gIPMInitPos` / `gEstBemf` / `gPmParEst`)。改写手法 (整体 1133 -> 1094 行, 30798 -> 30891 字节): (1) 全部 9 处 `switch`/`case` 分派改写为等值 `if/else if` 链, 各 case 体逐字搬入; 基准里 case 之间发生**贯穿**的 (`SynInitPosDetect` 的 `case 3|4`、`SynTunePGZero_No_Load` 的 `case 7|default`、`SynTunePGZero_Load` 的 `case 3|4`、`SynTuneBemf` 的 `case 1|4`) 合成一条 `(A || B)` 判据, 语义仍是"两个取值走同一段代码"; 无体的 `case`/`default` 不生成分支; (2) `SetIPMPos` / `SetIPMPos_ABZRef` 里两处**逐字符相同**的 QEP 计数式抽为 `static Uint QepCountFromPos(Uint)`, 表达式与 `Uint` 截断点一字未改, 调用点仍在 `DINT` 之前 (临界区长度不变); 同时删掉 `m_Pos` 这个只是形参副本、两次读之间无写入的局部量; (3) `SynCalLabAndLbc` 的两处 `? :` 赋值拆成 `if/else` (两臂同左值同类型); (4) `SynInitPosDetCal` 三条 `Cur[0/2/4] += Cur[0/2/4+6]` 合成一个步长 2 的循环 —— 读集合 {6,8,10} 与写集合 {0,2,4} 不相交, 属全域等价; (5) `SynTunePGZero_No_Load` 的 `case 1` 把 `if(...){...; break;}` 加尾随 `if/else` 改为 `if/else` 嵌套 (两条路径在 `DetectCnt==0` 时都跳过尾随 `if`); (6) `SynTunePGZero_Load` 的 `case 1` 把 `if(PGMode==0){ ; }` 的空臂取反为 `if(PGMode != 0)`, 写保留; `case 4` 两条唯一语句逐字符相同的 `IdSubStep++` 臂合并为一条 `||` 判据; `case 5` 把三条并列判据共有的前半条件 `PGType == PG_TYPE_UVW` 提为外层守卫; (7) `DetectZeroPosOnce` 把 `DetectCnt == 2000` 的两支极性反转; (8) `SynInitPosDetSetPwm` 直接使用形参 (形参在本函数内只读), 删除副本; (9) `SynInitPosDetSetTs` **一字未改** —— 6 条 EPwm 寄存器写的顺序是硬件时序的一部分。**D19 硬边界: 所有处于 16 位截断点上的算术表达式 (移位量、截断宽度、运算顺序、显式/隐式转换) 逐字符不动**, 本模块没有把"两行各自截断"合并成"一行只截断一次"。

- 必须守住的不变量:

  - **函数名/签名与文件级对象不变**: 13 个入口名与签名逐字保留 (含 `SynTuneBemf()` 的空括号写法); 4 个文件级对象 `gIPMZero` / `gIPMInitPos` / `gEstBemf` / `gPmParEst` 名称与类型不变; include 集合不变 (`#include "MotorInclude.h"`); 5 个前置声明原样保留。新增的 `QepCountFromPos` 是文件级 `static`, C28x 上是 `C_STAT`, 不占外部符号。
  - 目标端外部符号集合改前改后**逐项相同** (target_check.py 用 nm2000 读 C_EXT, `符号集合相同: True`), 无一条新增/丢失; `warn_added []` / `warn_gone []`, 两侧告警均为 2 条 (`m_Dir1` / `m_Dir3` 设置未使用 —— 基准固有); `.text` 2444 -> 2340 (**-104**), 记录不作为门禁。
  - **宿主差分: 冻结基准 vs 交付源码 20145 例 0 处不一致** (hand=145 + random=20000), 见 docs/_difftest/MotorPmsmParEst.json。driver 逐字段比对 23 个共享全局 + 4 个模块自有全局 + 4 个外设寄存器块 + 8 槽调用台账 + 7 槽 CPU 控制序列。
  - **关键计数逐一相等** (改动前 / 改动后): `EALLOW 4/4`, `EDIS 4/4`, `DINT 2/2`, `EINT 2/2`, `DisableDrive 12/12`, `EnableDrive 5/5`, `abs( 17/17`, `GetQepCnt 2/2`, `atan( 1/1`, `ResetParForVC 2/2`, `SynCalLdAndLq 1/1`, `IPMCalAcrPIDCoff 1/1`, `InitSetPWM 1/1`, `GetUvwPhase 1/1`, `JudgeABDir 2/2`, `JudgeUVWDir 2/2`, `JudgeRTDir 2/2`。
  - **负向对照 219/219 全部 CAUGHT, 0 BROKEN, 0 HUNG**; 全量清单 `docs/_difftest/_negctl_partial_MotorPmsmParEst.json`; 总账 `docs/_difftest/_negctl_summary.json` 4573/4573 `pass True`。`tests/test_difftest_negctl.py` 11 项 0 失败。
  - **阶段标签审计 0 条只被随机相位抓到** (`build/_me_phase_audit.py MotorPmsmParEst`, 219 条记录、0 条缺失、126 个确定性标签中至少一个能逼出每条变异的差异)。审计过程同时暴露并修好了测试台自身的一处缺陷: 随机相位标签原写 `"rand"`, 而审计只把 `"random"` 当非确定性标签, 于是对本用例天然失明; 改为规范标签后重扫。
  - **无注释 / 纯 ASCII / CRLF / 无制表符**: 1094 行, CRLF 1094, 制表符 0, `/*` 0, 非 ASCII 0 (verify_tree.py 复核)。
  - **改写前后归一化代码行多重集差异逐条可归因, 且没有一条是算术表达式**: 全部差异落在 `switch`(9) / `case N:`(54) / `default:`(8) / `break;`(59) 及其配对花括号、QEP 计数式 2->1 去重、三元 4 行拆 8 行、三条 `Cur[]` 语句合成一个循环、两臂合并的 `IdSubStep++` 2->1、极性反转的两处判据、两个空 `;` 臂。

| 项 | 结果 |
|---|---|
| 差分用例数 | **20,145** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_DetectZeroPosOnce`, `_EPwm1Regs`, `_EPwm2Regs`, `_EPwm3Regs`, `_EQepRegs`, `_GetUvwPhase`, `_GpioDataRegs`, `_IPMCalAcrPIDCoff`, `_InitSetPWM`, `_InitSetPosTune`, `_JudgeABDir`, `_JudgeRTDir`, `_JudgeUVWDir`, `_ResetParForVC`, `_SetIPMPos`, `_SetIPMPos_ABZRef`, `_SynCalLabAndLbc`, `_SynCalLdAndLq`, `_SynInitPosDetCal`, `_SynInitPosDetSetPwm`, `_SynInitPosDetSetTs`, `_SynInitPosDetect`, `_SynTuneBemf`, `_SynTuneInitPos`, `_SynTunePGZero_Load`, `_SynTunePGZero_No_Load`, `_atan`, `_gADC`, `_gCtrMotorType`, `_gError`, `_gEstBemf`, `_gGetParVarable`, `_gIMTSet`, `_gIPMInitPos`, `_gIPMPos`, `_gIPMZero`, `_gIUVWQ24`, `_gInvInfo`, `_gLineCur`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtInfo`, `_gMotorExtReg`, `_gMotorInfo`, `_gOutVolt`, `_gPGData`, `_gPGDir`, `_gPmParEst`, `_gRotorTrans`, `_gUDC`, `_gUVWPG` |
| `.text` 尺寸 | 2444 → 2340 (**-104**) |

### `src/motor/MotorParChange.c`

- 变换: 参数变更/派生量计算模块。5 个入口: 逆变器档位与整定 `SystemParChg2Ms`、运行态派生量 `RunStateParChg2Ms`、编码器模式重配 `SystemParChg05Ms`、电机参数归一化 `ChangeMotorPar`、功率/转矩反算 `InvCalcPower`。**本模块不自有任何文件级对象** —— 它读写的 33 个全局量全部属于别的模块, 写的 3 组外设是芯片的, 所以外部符号集合天然稳定。改写手法 (441 -> 526 行, 14855 -> 16719 字节): (1) `SystemParChg2Ms` 的六段电压带 `if/else if` 链改成 6 行常量表 `s_InvBand[]` + 线性扫描, 220T 档"强制 GPType=1"用表里的 `m_GpForce` 表达; (2) 死区档位先算出下标 (`m_DeadIdx`, 400 档特判为 13) 再统一查表, 1140V 档仍走 `DBTIME_1140V*DeadTimeSet/10` 公式; (3) `FreqUint` 的三段 `if/else` 改成 3 行系数表 `s_FreqCoeff[][]` + 下标钳位 (`FreqUint` 是 2 位位域, 取值 0..3, 钳到 2); (4) `RunStateParChg2Ms` 的过压上限五段链改成 5 行常量表 `s_OvBand[]` + 扫描; (5) 三处 `((Ullong)x * FullFreq01 + (1<<14)) >> 15` 抽成 `s_FreqToReal()`, 两处 `*18641>>10` / 三处 `*180>>15` 抽成 `s_PosScale()` / `s_DegScale()`; (6) `ChangeMotorPar` 里三处 `(x*Current)/Votage*18597>>14` 抽成 `s_ResScale()`, 四处 `((L<<11)+BaseL)>>1/BaseL` 抽成 `s_IndScale()`; (7) `SystemParChg05Ms` 的 QEINT 两个分支合成"先算一个值再写一次"; (8) `InvCalcPower` 的 `StartStop` 门控先取一次标志位 `m_Run`; (9) 三个无用局部量 (`SystemParChg2Ms` 的 `m_Long`、`InvCalcPower` 的 `temp`) 与 `#if 0` 死块删除。**D19 硬边界: 所有处在 16 位截断点上的算术表达式逐字符不动** —— 移位量、截断宽度、运算顺序、显式/隐式转换一律保持原样, 例如 `gMotorExtInfo.Rpm * gMotorExtInfo.Poles * 6830L` 里前两个 `Uint` 的乘法在目标端是 16 位、会回绕, 换个写法就改结果; `(gMotorInfo.Votage * 10)` 同理。

- 必须守住的不变量:

  - **函数名/签名不变**: 5 个入口名与签名逐字保留 (含 `void SystemParChg2Ms()` 的空括号写法); include 集合不变 (`#include "MotorInclude.h"`); 没有新增文件级对象 —— 5 个新函数 `s_FreqToReal` / `s_PosScale` / `s_DegScale` / `s_ResScale` / `s_IndScale` 全是 `static`, C28x 上是 `C_STAT`, 不占外部符号; `INV_BAND` / `OV_BAND` 两个 typedef 与 3 张常量表同样是文件级 `static const`。
  - 目标端外部符号集合改前改后**逐项相同** (target_check.py 用 nm2000 读 C_EXT, `符号集合相同: True`, 65 个符号, 含 6 张电压信息表 / 5 张电流表 / 死区两张表 / `_qsin` / `_qsqrt` 与 5 个被调函数); 无一条新增/丢失。`warn_added []`; 基准 2 条告警 (`m_Long` / `temp` declared but never referenced) 在改写后**消失**, 新侧 0 条。`.text` 1471 -> 1378 (**-93**), 记录不作为门禁。
  - **宿主差分: 冻结基准 vs 交付源码 20207 例 0 处不一致** (hand=207 + random=20000), 见 docs/_difftest/MotorParChange.json。driver 逐字段比对 33 个共享全局 + 7 个外设寄存器块 (EPwm1/2/3、EQep1、影子 EQEP、PieVectTable、PieCtrlRegs、SysCtrlRegs) + `EQepRegs` **指针值本身** + 8 槽调用台账 + 7 槽 CPU 控制序列。
  - **CPU 控制序列逐槽相等**: `EALLOW/EDIS` 与 `DINT/EINT` 在宿主 shim 里是 `gHostCpuCtl[]` 的计数器, 该数组整个进比较镜像 —— 所以"少一个 EDIS"(EALLOW 悬空)、"多一对 DINT/EINT"、"临界区套错位置"都会被抓到。改写后计数与基准一致 (4 处 EALLOW/EDIS + 1 处 DINT/EINT, 逐场景相等)。
  - **负向对照 194/194 全部 CAUGHT, 0 BROKEN, 0 HUNG, 0 崩溃**; 退出码直方图 `{1: 194}` (只有 1 = 跑完且有差异才是证据; 0xC0000005 一类崩溃必须单独计数, 见下面的 coverage_gap)。全量清单 `docs/_difftest/_negctl_partial_MotorParChange.json`; 总账 `docs/_difftest/_negctl_summary.json` 由 tests/test_difftest_negctl.py 断言。
  - **交付格式**: CRLF、无注释、纯 ASCII (verify_tree.py 卡这三条); `build/_mpc_write_src.py` 是唯一的生成入口, 它自己断言这三条。
  - **分母全部钉死**: 本模块有 14 处除法, 其中 11 处的分母是别的模块的全局量。driver 的 `pin_divisors()` 在每个场景之后强制它们非零, 另外两个由模块自己算出来的分母 (`m_BaseL` / `ItRated`) 通过约束它们的输入保持 > 0。变异一条都不碰分母置零 —— 分母置零在宿主上是 #DE 崩溃, 会让整条台账作废。
  - **随机相覆盖 5 个入口**: 20000 例随机输入下按 `rnd16() % F_COUNT` 轮转调用, 5 个入口各约 4000 例。
  - **按工程实际优化选项编译时, 改写没有收敛到同一份机器码** —— 这一点必须如实记录, 不能当成失败, 也不能当成通过: `tools/difftest/o2_check.py` 记录 `identical_all = False`, Debug (-o2) 反汇编 new 1351 行 / ref 1431 行、1418 处不同; Release (-o3) new 1359 行 / ref 1420 行、1414 处不同。原因是本模块把六段电压带 `if/else if` 链换成了**常量表 + 循环**, 优化器于是生成'查表循环'而不是'比较链' —— 这是改写的**预期后果**, 不是行为差异。`.text` 反而变小: Debug 0x556 -> 0x4c5 字 (-145), Release 0x54a -> 0x4d2 (-120)。证据: docs/_difftest/MotorParChange.o2.json。
  - **关优化口径下两侧也不是逐字节相同** (这正是改写模块的定义), 行为等价由20207 例差分测试 + 194 条负向对照保证, 而不是由字节相同保证。

| 项 | 结果 |
|---|---|
| 差分用例数 | **20,207** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_ChangeMotorPar`, `_EPwm1Regs`, `_EPwm2Regs`, `_EPwm3Regs`, `_EQep1Regs`, `_EQepRegs`, `_InitSetQEP`, `_InvCalcPower`, `_PG_Zero_isr`, `_PieCtrlRegs`, `_PieVectTable`, `_ReInitForPG`, `_RunStateParChg2Ms`, `_SetIPMPos`, `_SetIPMPos_ABZRef`, `_SysCtrlRegs`, `_SystemParChg05Ms`, `_SystemParChg2Ms`, `_gADC`, `_gBasePar`, `_gCurSamp`, `_gDeadBand`, `_gDeadBandTable`, `_gDeadCompTable`, `_gExtendCmd`, `_gIAmpTheta`, `_gIMTQ12`, `_gIMTSetApply`, `_gIMTSetQ12`, `_gIPMInitPos`, `_gIPMPos`, `_gIPMPosCheck`, `_gIUVWQ12`, `_gIUVWQ24`, `_gInLose`, `_gInvCurrentTable1140T`, `_gInvCurrentTable220S`, `_gInvCurrentTable220T`, `_gInvCurrentTable380T`, `_gInvCurrentTable690T`, `_gInvInfo`, `_gInvVoltageInfo1140T`, `_gInvVoltageInfo220S`, `_gInvVoltageInfo220T`, `_gInvVoltageInfo380T`, `_gInvVoltageInfo480T`, `_gInvVoltageInfo690T`, `_gLineCur`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtInfo`, `_gMotorExtPer`, `_gMotorInfo`, `_gOutVolt`, `_gOvUdc`, `_gPGData`, `_gPmDecoup`, `_gPowerTrq`, `_gRotorTrans`, `_gUDC`, `_gUVCoff`, `_gUVWPG`, `_gVFPar`, `_gVarAvr`, `_qsin`, `_qsqrt` |
| `.text` 尺寸 | 1471 → 1378 (**-93**) |

### `src/function/f_runSrc_accDecFrq.c`

- 变换: 加减速频率规划模块。4 个入口: 主入口 `AccDecFrqCalc(accTime, decTime, mode)` (f_runSrc.c 的 9 处活调用)、两条 S 曲线状态机 `SCurveOridinaryCalc` / `SCurveSpecialCalc`、直线插补 `LineChangeCalc`(只经 `frqLine.calc` 函数指针调用)。`LOCALF`/`LOCALD` 在 `include/function/f_debug.h:27-33` 的 `#if 1` 分支里分别展开成**空**与 `extern`, 所以两条 S 曲线函数在目标端是外部链接的。改写手法 (615 -> 686 行, 15379 -> 15833 字节): (1) 六项 `&&` 守卫链改成 `s_AccDecUseRun()` 提前 return, **短路顺序逐项保持**; (2) ACC/DEC 分叉抽成 `s_AccDecIsAcc(frq0)`; (3) `accDecBenchmark` 的 if/else-if 改成 `s_BenchOverride()` 里的 switch; (4) S 曲线降级判据抽成 `s_CurveDowngrade(mode, accDecTime)`; (5) 三路派发改成 `switch (mode)`, `#if` 结构照原样保留; (6) 三处 `while (tmp >= 1L << 18) { tmp >>= 1; q[x]--; }` 抽成 `s_Fit18(int64, Uint16*)`; (7) 段斜率三分支改 switch, `frqTmpFrac` 的 if/else 抽成 `s_Q15Rescale(int64, Uint16)`; (8) `SCurveSpecialCalc` 的四路拐点选择改成"先取有效源再判符号"; (9) ACC/DEC 计数步进抽成 `s_TickUp()` / `s_TickDown()`; (10) `START_S` 的分子/分母写两遍抽成 `s_SbStep()`, `LINE` 写两遍抽成 `s_SbLine()`; (11) 上冲/下冲钳位抽成 `s_SbClamp(frq0, old)`; (12) `LineChangeCalc` 的嵌套 if 改成提前 return。**逐字节保持的截断点**: `Uint16 lineTime = 2000 - startTime - endTime;` 的截断、`benchFrq` 的三条赋值路径 (`ABS_INT32(frqCurAim)` / `maxFrq` / `100*decNumber[frqPoint]`, 后两条都截到 `Uint16`)、所有 `int64` 中间量 (`tmp3`/`tmp1`/`remainder`/`rem1`)、`(tmp*tmp)` 与 `tickerAll2*B*b` 在 **int32** 宽度上的乘法、两个入口分支里的 `Uint32 u`。

- 必须守住的不变量:

  - **函数名/签名不变**: 4 个入口名与签名逐字保留; include 集合不变 (5 个头); 没有新增文件级对象 —— 11 个新函数全是 `static` (`s_AccDecUseRun` / `s_AccDecIsAcc` / `s_BenchOverride` / `s_CurveDowngrade` / `s_Fit18` / `s_Q15Rescale` / `s_TickUp` / `s_TickDown` / `s_SbStep` / `s_SbLine` / `s_SbClamp`), C28x 上是 `C_STAT`, 不占外部符号。`frqLine` 的初值宏 `LINE_FRQ_CHANGE_STRTUCT_DEFALUTS` 原样保留 (含 `(void (*)(void *))LineChangeCalc` 强转)。
  - 目标端外部符号集合改前改后**逐项相同** (`target_check.py` 用 nm2000 读 C_EXT: `符号集合相同: True`, 24 个 —— 4 个函数 + 15 个数据 + `_qsqrt`); 无一条新增/丢失。两侧告警都是 0 条, `warn_added` / `warn_gone` 皆空。`.text` 1548 -> 1542 (**-6**), 记录不作为门禁 (`--opt_level=0` 会把每个 static 小函数照样展开成调用序列, 收益有限)。
  - **宿主差分: 冻结基准 vs 交付源码 539530 例 0 处不一致** (8 个手写相 + 20000 随机), 见 docs/_difftest/f_runSrc_accDecFrq.json。driver 逐字段比对模块自有 4 个标量 + `frqLine` 的 5 个字段 + 归一化函数指针 `calcKind` + 模块写的 5 个外部量 + 调用返回的 6 个 `o*` 字段。
  - **`frqLine.calc` 用归一化比较, 不比裸地址**: 直接比函数指针地址永远不等 (两侧各有自己的 `LineChangeCalc`)。归一化成 `calcKind`: 0=NULL / 1=指向本侧的 `LineChangeCalc` / 2=别的非空地址。`calcKind==2` 的哨兵必须是**真函数** `other_calc` —— 第一版用假地址 `0x1234UL`, `AccDecFrqCalc` 的 LINE 分支会 `frqLine.calc(&frqLine)`, 直接 0xC0000005。
  - **18 个覆盖计数器全部非零**: `STATUS_ACC` 239203 / `STATUS_DEC` 243147 / `STATUS_CONST` 57180 / `GUARD_FRQ_RUN` 1603 / `GUARD_FRQ_TMP` 85745 / `BENCH_FROM_MAX` 21435 / `BENCH_FROM_DECN` 21376 / `BENCH_OTHER` 44537 / `FRAC_NONZERO` 142315 / `TICKER_ZERO` 493536 / `TICKER_NONZERO` 45994 / `RET_EQ_AIM` 159796 / `RET_NE_AIM` 279829 / `LINE_REM_NONZERO` 9864 / `LINE_OUT_EQ_AIM` 3754 / `CALC_OWN` 527609 / `CALC_NULL` 5650 / `CALC_OTHER` 6271。这组数不是装饰: 第一版跑出 0 不一致时其中 5 个是 0, 追下去全是测试台自己的 bug (见 coverage_gaps)。
  - **`runFlag.bit.accDecStatus` 在 bit 5..6**, 不是 bit 0 (`run:1, common:1, jog:1, tune:1, jogWhenRun:1` 在前面)。driver 用 `ACCDEC_BITS(v) = ((v)&3)<<5` 装、`ACCDEC_STATUS_OF(all) = ((all)>>5)&3` 读。第一版直接 `runFlagAll = ACC_SPEED` 写进 bit 0, 一条 S 曲线分支都没进过。
  - **函数内静态量靠"同一起点 + 同一调用序列"对齐**: `sCurveAStatus`/`ticker`/`t[3]`/`a[3]`/`q[3]`/`remainder` 等静态量 driver 看不到, 两侧从同样的零态开始跑完全相同的序列, 实现等价时同步演化。`a[3]/q[3]/t[3]` 在 `sCurveAStatus == SA_STEP_OK` 时会被**越界读**, 值来自相邻静态量 —— 所以六个 `static` 的声明顺序**逐字保持** (`sCurveAStatus, ticker, t[3], a[3], q[3], remainder`)。
  - **负向对照 198 条全部 CAUGHT, 0 BROKEN, 0 HUNG**: `AD*` 49 (`AccDecFrqCalc`), `SA*` 55 (`SCurveOridinaryCalc`), `SB*` 82 (`SCurveSpecialCalc`), `LC*` 12 (`LineChangeCalc`)。锚点唯一性由 `build/_adf_negctl.py --check` 对每条断言 `base.count(old) == 1` 强制 (`build_variant` 是 `replace(old,new,1)`, `locate_anchor` 是 `find(old)`, 两处都只看第一处命中)。5 条**可证等价**的变异已删除并写明证明: AD16/AD17 (第 84 行 `if (frqCurAim == frq0)` 已把相等分流, `>` 与 `>=` 同值)、SA9 (`frqAimDelta` 只出现在 `ABS_INT32()` 里)、SB80/SB81 (钳位块体是 `frq0 = frqCurAim`, 放宽后多出的唯一情形 `frq0 == frqCurAim` 是恒等赋值)。
  - **SB67 与 SB37/SA45/SA48 换成可抓的同址变异, 不是删掉**: SB67/SB37 原是 `remainder = 0 -> 1`, MISSED 但**不是死存储** —— 同址放大到 `= 1000000` 分别抓到 2045 / 4581 例。读点是 `delta = (X+remainder)/D`、`remainder = (X+remainder)%D`, `D = tickerAll2^2 * Sden` (Sden=30), `+1` 只在 `X % D == D-1` 时改商, 而那些命中点上 `X` 恰好都是 `D` 的整数倍。SA45 (`q >= 15` -> `>= 16`) 是**代数等价**: q==15 时 `>>0` 与 `<<0` 同值; SA48 (`frq0 < frqCurAim` -> `<=`) 在**真机不可达**: 唯一调用方第 84 行 `if (frqCurAim == frq0)` 已把相等分流去 CONST_SPEED。四条都换成同址、同 bug 类、实测 CAUGHT 的变异 (SB67 -> 抄 ACC 分支公式; SB37 -> `remainder = rem1`; SA45 -> `>= 14`; SA48 -> 极性取反)。
  - **`LineChangeCalc` 的钳位是 `if/else` 两支都写一遍**, 改写时保留两支: 上升支 `curValue += delta` 后 `> aimValue` 钳位, 下降支 `curValue -= delta` 后 `< aimValue` 钳位。`(int32)p->maxValue + p->remainder` 的 int32 加法与 `% p->tickerAll` 的取模逐字节保持。

| 项 | 结果 |
|---|---|
| 差分用例数 | **539,530** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AccDecFrqCalc`, `_LineChangeCalc`, `_SCurveOridinaryCalc`, `_SCurveSpecialCalc`, `_accTimeOld`, `_bFrqCurAimChg`, `_benchFrq`, `_decNumber`, `_decTimeOld`, `_frqCurAim`, `_frqCurAimFrac`, `_frqCurAimOld`, `_frqLine`, `_frqRun`, `_frqTmp`, `_frqTmpFrac`, `_frqVFRunRemainder`, `_funcCode`, `_maxFrq`, `_motorFc`, `_qsqrt`, `_runFlag`, `_runStatus`, `_timeBench` |
| `.text` 尺寸 | 1548 → 1542 (**-6**) |

### `src/function/f_osc.c`

- 变换: 上位机示波器模块 (SCI 命令通道 + 波形采集回传)。3 个外部链接入口: `OscSciFunction`(f_main.c:714 主循环每拍一次)、`OscControlRead` / `OscControlWriter`(只被本模块的 FC_READ_FC_DATA / FC_WRITE_FC_DATA 分支调, 因 `f_osc.c:21-22` 先给原型且 FC_CODE_CONTROL_EN 为 1, 所以是外部链接, driver 能直接驱动)。其余 14 个函数全是 static。改写手法 (747 -> 746 行, 15954 -> 14416 字节): (1) `InitOsc` 的 9 条逐字段赋值改成表驱动 —— `static const DSP_OSC_CON_DATA OscCongDefault` + 整体赋值; (2) `InitSciOSC` 里逐个 `PIEIER9.bit.INTxN = 0/1` / `PIEIFR9.bit.INTxN = 0` 改成位掩码 RMW (`PIEIER9.all &= ~OSC_PIE_RX_MASK` 等, 其他位不受影响); `BautRateCfg[baudRate-1]` 只算一次存进 `baud`; (3) `SciDataTX` / `sciaTxFifoIsr` / `sciaRxFifoIsr` 里 4 条展开的 `*buf++` 推/取改成 `for (i = 0; i < OSC_SCI_CHUNK; i++)` 循环; (4) `SciDataRxDeal` 的 FC_CHANNL_SEL: 4 组「赋值 + 条件自增」改成先把 P1..P4 收进 `sel[4]`、循环数出 `sum`、再一次性写 4 个通道 + chSum; (5) `SciDataRxDeal` 的 CRC 校验把内联拼接提进 `crcFrame` 再比 (**拼接表达式逐字保留, 含 `(Uint16)` 转换的位置**); (6) `OscDataQcq` / `OscDataTxDeal` 的 A/B 两段几乎逐字重复的分支改成`buf = (bufSel == 0) ? &OscDataBufA : &OscDataBufB` + 单份主体, 回绕动作 `bufSel = (bufSel == 0) ? 1 : 0` (对 bufSel 的**全部取值**都与原写法同值); (7) `GetOscData` 的 4 段重复通道块改成 `addr[4]` + 循环, OSC_GET_CHECK 宏原样保留; (8) `ClearOscBufEnd` 的 8 条赋值改成 `buf[2]` + 双层循环 (k=1..4 -> 下标 63/62/61/60); (9) `OscSciIoInit` 的两条 GPAPUD 位写改成 `GPAPUD.all &= ~((Uint32)0x3 << 28)` (GPADAT_BITS 里 bit28=GPIO28、bit29=GPIO29); (10) `SciDataRx` 的超时判据 `(count > timeOut) && (timeOut != 0)` 交换成 `(timeOut != 0) && (count > timeOut)` (两侧都无副作用); 采样节拍 `if (count == interval) { ... }` 改成 `if (count != interval) return;` + 尾部 count=0; (11) `OscControlRead` 去掉被立刻覆盖的 `oscReturn = 0` 死初始化。**逐字节保持的截断点**: `CONTROL_FRAME_P3 = err;` **不掩码** (uint8 在 C28x 上 是 16 位, 整个 err 留在 P3), `rwPI += chSum << 1`、`len -= 4`、`frame++`、`chSum` 自增、`SciTxBufStr.len = len - 4` 的全部 uint8 宽度。

- 必须守住的不变量:

  - **函数名/签名/链接性不变**: 3 个外部入口与 14 个 static 函数名逐字保留, include 集合不变 (f_osc.h + f_comm.h), **没有新增函数** —— 第一版曾抽出 `OscBufPick(uint8)` 做 A/B 选择, 为了保持与原工程 1:1 的函数对应关系又内联回三元表达式。7 个文件级 static 对象 (`OscCongData` / `OscDataBufA` / `OscDataBufB` / `OscConFrameBuf` / `BaudCfg` / `SciTxBufStr` / `SciRxBufStr`) 的名字与类型逐字不变 —— 用例的访问器 (`shim/osc_state_body.h`) 按名字读它们。
  - **C28x 的 char 是 16 位, 这是本模块最容易搞错的一条**: `f_osc.h` 里 `typedef unsigned char uint8;`, 所以 `uint8 OscConFrameBuf[10]` 占 10 个字, `CrcValueByteCalc((const Uint16 *)OscConFrameBuf, FC_FRAME_LEN-2)` 读 **6 个字**。宿主上 char 是 8 位, 同一行会读 12 字节、越界 2 字节, CRC 就取决于两个 TU 里数组后面刚好放了什么。用例的 `ref_tu.c` / `new_tu.c` 在 include 源码**之前** `#include <stdint.h>`、`#include <stddef.h>` 再 `#define char short` —— 顺序是硬要求, 否则 glibc 的 `typedef signed char int8_t;` 会被一起替换成 `signed short`。
  - **`OscQuqData = (Uint16 *)((&funcCode.group.u0[0]) -1)` 与 `GET_OSC_DATA(addr)` 宏逐字保留**: addr 1 读到的是 `u0[0]`。这个 -1 偏移是采集通道地址的约定, 改一个字符就整体错位。
  - **目标端外部符号集合改前改后逐项相同** (`target_check.py` 用 nm2000 读 C_EXT, 15 个: `_OscSciFunction` / `_OscControlRead` / `_OscControlWriter` / `_CrcValueByteCalc` / `_CommRead` / `_CommWrite` / `_SciaRegs` / `_PieCtrlRegs` / `_PieVectTable` / `_GpioCtrlRegs` / `_SysCtrlRegs` / `_funcCode` / `_sciFlag` / `_commRcvData` / `_commReadData`), 符号集合相同: True。`_GpioDataRegs` 不在集合里是正确的 —— RS485_ENABLE==0, 那一段不编译。两侧告警都是 0 条, `warn_added` / `warn_gone` 皆空。`.text` 874 -> 801 (**-73**), 记录不作为门禁。
  - **宿主差分: 冻结基准 vs 交付源码 29123 例 0 处不一致** (9 个手写相 + 20000 随机), 见 docs/_difftest/f_osc.json。driver 逐字段比对: 模块自有 7 个状态对象 (配置块 9 字段 / 两个 70 字缓冲 / 10 字帧缓冲 / BaudCfg / 两个 SCI_RT_CON_DATA 的 busy+len+rxflag+归一化 buf)、13 个 SCI 寄存器、4 个 PIE 寄存器、两个中断向量、GPIO 与 SysCtrl 整块字节、IER、7 个 CPU 控制计数器、sciFlag / commRcvData / commReadData, 以及 3 个入口的返回值与 `*result` 出参。
  - **指针比较归一化成 (kind, off), off 以元素为单位**: `SciTxBufStr.buf` / `SciRxBufStr.buf` 直接比地址必然不同; 归一成 0=NULL / 1=`OscDataBufA` 内 / 2=`OscDataBufB` 内 / 3=`OscConFrameBuf` 内 / 4=其它。`off = (v - b) / sizeof(uint8)`, **不是字节数** —— 宿主上 uint8 是 2 字节, 用字节偏移会把「前进 4 个通道」算成 8, 把正确的实现判成 BENCH-BUG。
  - **30 个覆盖计数器全部非零**: MAIN 20811 / RXISR 2183 / TXISR 3116 / CTRLWRITE 1968 / CTRLREAD 1019 / RXISR_ERR 3 / RXISR_OK 2 / RXISR_LAST 1 / FRM_CRC_BAD 3 / FRM_HEAD_BAD 3 / FRM_STOP 1 / FRM_STATUS_BUSY 1 / FRM_COMM_TEST 1 / FRM_CHANNL 6 / FRM_PARA 4 / FRM_START 1 / FRM_READ 3 / FRM_WRITE 3 / FRM_DEFAULT 3 / FRM_PWDPASS 1 / QCGQ_FULL_A 47 / QCGQ_FULL_B 21 / TX_SENT_A 12 / TX_SENT_B 6 / RXDIR_ARM 4 / RXDIR_TIMEOUT 4 / RXDIR_LENDONE 4 / BAUDCFG_REINIT 2 / STATUS_ON 13619 / STATUS_OFF 15504。这组数不是装饰: 其中 5 个曾经是 0 或严重不对称, 追下去全是测试台自己的 bug (见 coverage_gaps 与 README §4)。第 31 个计数器 `ISR_GUARD` **恒为 0**, 它不是覆盖率而是一个断言 —— 见下一条。
  - **中断前置条件门 `isr_safe()`, 基准上必须一次都不关**: 真机上两个 ISR 只在硬件把 4 字节摆进 FIFO 时被调用, 那一刻 `SciRxBufStr.buf` / `SciTxBufStr.buf` 一定指向还有 >=4 个元素余量的缓冲; 测试台是从 `PieVectTable` 取函数指针直接调, 没有这层保证。第一版 201 条变异里 **18 条 rc=0xC0000005**（访问违例）: ISR 的 4 条 `*buf++` 写到 `OscConFrameBuf[10]` 之外, 覆盖紧随其后的 `SciRxBufStr.buf` 指针, 下一次解引用即野指针 —— 进程崩在半路, 后面的用例一条都没跑, 总账却照样记 CAUGHT（rc 直方图 `{1: 183, 3221225477: 18}`）。修法: 驱动 ISR 前按**每一侧自己**的写行为算前置条件（RX 只看 `rxerr==0`; TX 只在 `len>3` 时写, `len<=3` 的收尾支照常驱动; 真要写时 `(kind,off)` 必须落在 三个缓冲之一里且 `off+4 <= 容量`）, 关掉时记 `COV_ISR_GUARD`。修完 rc 直方图 `{1: 201}`, 32 条变异触发过这道门且全部仍有 60 行 MISMATCH, 「门关过 + 一行 MISMATCH 都没有」的条目 0 条。
  - **派生覆盖从快照读, 不从「我打算走哪一支」读**: `QCGQ_FULL_A/B` 不看 `full` 位 (填满当拍就被 OscDataTxDeal 送走了), 而是看 `rwPI` 是否**回绕** (`bufA[68] < 上一拍的值`); `TX_SENT_A/B` 看 `txBusy` 0->1 那一拍 `txKind` 是 1 还是 2。
  - **负向对照 201 条全部 CAUGHT, 0 BROKEN, 0 HUNG**: QU* 3 (数据/常量), OS* 18 (OscSciFunction), IN* 9 (InitOsc), IS* 20 (InitSciOSC), TX* 9 (SciDataTX), TI* 8 (sciaTxFifoIsr), RX* 18 (SciDataRx), RI* 10 (sciaRxFifoIsr), SD* 42 (SciDataRxDeal), QC* 21 (OscDataQcq), TD* 16 (OscDataTxDeal), GD* 7 (GetOscData), CB* 5 (ClearOscBufEnd), IO* 8 (OscSciIoInit), CW*/CR* 7 (Control 两入口)。锚点唯一性由 `build/_osc_negctl.py --check` 对每条断言 `base.count(old) == 1` 强制, 并额外判**借位**（锚点自带缩进、命中位置前面还有缩进）、**撞语句**（锚点不带缩进却从 token 中间开始）、**两条变异产生逐字节相同的变异源码**。本模块缩进 tab 与空格混排、大量语句在别处以更深缩进重复出现, 只写 `\tSciTxBufStr.busy = 0;` 会从 `\t\tSciTxBufStr...` 的第 2 个字符开始匹配 —— 所以绝大多数锚点都以 `\n` 开头把匹配钉在行首。
  - **`EALLOW`/`EDIS` 各一次、位置不变, 且进比较镜像**: `gHostCpuCtl[4..6]` 是 EALLOW/EDIS/DINT/EINT 的计数器, 整个放进 struct HW。这样「少一个 EDIS」(IO8)、「少一个 EALLOW」(IO7) 才抓得到 —— 这类错**只比结果永远看不出来**。代价是改写时不能为了顺眼把两对 EALLOW/EDIS 合成一对。
  - **`status != 0` 那一支什么也不写, 是原工程如此**: 它只给局部 `err` 赋值 0x80, 而 `if (err > 0) { ... }` 与 `SciDataTX(...)` 都在 `else` 块**内部**。driver 的 status-busy 用例能验的只有「什么都没变」, 反过来它恰好能抓「改写把它挪进 if (err>0)」。
  - **`FC_STOP_OSC` 的四条清零只在当拍可观测**: 紧接着的下一次 OscSciFunction 调用里, OscDataQcq 的 `status != 1` 分支会把同样四个字段再清一遍, 所以「删掉 STOP 里那四行」在 12 拍之后看是真等价。driver 用 `run_frame_until_status()` —— status 一变就立刻停。
  - **`build_and_compare` 报的两条段差异是索引错位的假象, 不是真差异**: 新对象多出 `.econst:_OscCongDefault`（9 字, 就是表驱动初值那张表）, 段数 9 -> 10, 而比较器按**段下标**配对, 于是 `.text:retain: size=133/10` 与 `.ebss:_OscConFrameBuf: size=10/9` 都是把 A 的第 9 段和 B 的第 9 段硬凑在一起。真实数字用 `dis2000 -1` 直接读两个 .obj: `.text` 874 -> 801, `.text:retain` 133 -> 122（合计 **-84**）, `_OscConFrameBuf` 两侧都是 10 字。

| 项 | 结果 |
|---|---|
| 差分用例数 | **29,123** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CommRead`, `_CommWrite`, `_CrcValueByteCalc`, `_GpioCtrlRegs`, `_OscControlRead`, `_OscControlWriter`, `_OscSciFunction`, `_PieCtrlRegs`, `_PieVectTable`, `_SciaRegs`, `_SysCtrlRegs`, `_commRcvData`, `_commReadData`, `_funcCode`, `_sciFlag` |
| `.text` 尺寸 | 874 → 801 (**-73**) |

### `src/motor/MotorImParEst.c`

- 变换: 异步电机参数辨识模块 (Rs -> Rr/L0 -> Lm/I0 -> 电流环 PI)。15 个外部链接入口全部无参数: `RsIdentify` / `RrLoIdentify` / `LmIoIdentify` (三台状态机, 靠 `gGetParVarable.IdSubStep` 推进, 段间由 `ParEstMstep` 交接), `ChgParForEst`, `LmIoInPeriodInt` (MotorMain.c:104 周期中断), `RrLoInPeriodAndCmpInt` (**唯一的中断入口**, 14 点采样窗口, 向量表在 MotorMain.c:756), `SetPwmForRrLoId` / `InitRrLoVariable` / `EnableTimer` / `RrWaitAddGetUdc` / `RrGetVoltReBegin` / `RrLoCalVaule` (Rr 段自调), `ImCalKpKi` / `InitLmIoVaribale` / `LmIoCalValue` (Lm 段)。模块**不定义任何全局量** —— 目标端 nm2000 给出 41 个未定义符号 (3 个编译器运行时 / 7 个外设结构体 / 6 个被调者 / qsin / qsqrt / 23 个全局量), 这决定了测试台的形状 (整幅输入映像, 见 invariants)。改写手法 (832 行 23560 字节 -> 827 行 20476 字节, .text 1942 -> 1920): (1) `RsIdentify` case 1 的电压类型三段 if/else-if/else 改成 switch (1140V=5 / 690V=3 / default); (2) case 4 的两道独立门槛 `>=512` / `>=1024` 改成嵌套 (1024 蕴含 512, 两个体都照旧执行); (3) case 3/5 的三次「先算 m_UData1 再覆盖」改成先选**分子因子** (1024/4096/3500) 再算一次, `Comper += if/else` 改成三元; (4) case 6 的逐位右移双变量循环改成「先数移位次数、再一次性移位」(Ulong 右移 k 次 == 一次右移 k 位); (5) `RrLoIdentify` case 2/3 里两处**逐字相同**的 13 行等待块抽成 `static void RrLoWaitTick(void)` (static 在两侧 TU 各有副本, 不会撞名也不会被链接器折叠); (6) case 3 的三处 `WaitCnt=20; RrGetVoltReBegin();` 收成一个 `m_Repulse` 标志; R2 系数的两段 if/else-if 改成系数变量 `m_R2Coff` + 一次条件乘法 (0 表示不改, 逐字对应原文的「两个分支都不进」); (7) `SetPwmForRrLoId` 的 6 组 `#ifdef DSP_CLOCK100` / `#ifdef DSP_CLOCK60` 收成 2 个宏 `IM_PWM_CLK_DIV` / `IM_PWM_HSPCLK_DIV`, 6 处 CLKDIV 写操作引用宏; (8) `InitRrLoVariable` 的 for 清零改成 while; (9) `LmIoIdentify` 的 case1->case2 隐式 fallthrough 改成前置 if + switch 从 case 2 起 (IdSubStep==1 时先 InitLmIoVaribale + 自增, 再落进 case 2 —— 与原 fallthrough 逐拍同值); case 3 的三处 `break`(跳出 switch) 改成 `m_Hold` 标志, `maxFrqErr` 提到函数作用域 (类型仍是 int, 赋值表达式逐字保留); (10) `LmIoInPeriodInt` 的复合 `if((a)||(b)) return;` 拆成两个独立早退; (11) `RrLoCalVaule` 两个**同区间**的 for (一个累 m_IsUdcVoltDiff、一个累 m_IsZeroVoltDiff) 合并成一次遍历; (12) `ChgParForEst` 的 PM / 非 PM 两条几乎逐字的 if/else-if/else 链改成一次算 `m_FcSet` + `m_HighRange` 标志, 两条支共用 (非 PM 支第三档就是 30, PM 支第三档是「保住原有的 >=30」—— 这个不对称正是必须用标志而不能用 `m_FcSet` 直接赋值的理由)。**刻意没动的六个函数**: `EnableTimer` / `RrGetVoltReBegin` (纯寄存器写序列, 没有可重写的控制流, 重排只会在硬件上引入时序风险而不改变任何可观测结果 —— 那是假的改写), `RrLoInPeriodAndCmpInt` (ISR, 14 点窗口 / __IQsat 饱和 / CBCLimitCurPrepare 的调用时机 / DINT 与 PIEACK 的先后每一条都对着硬件, 收益为零风险最高), `ImCalKpKi` / `LmIoCalValue` (稠密定点算术, `L0*LM` 的 16 位截断与 `(m_ComIm*m_ComIm)>>13` 的溢出边界都是改一个字就变语义的地方), `RrWaitAddGetUdc` (超时循环圈数可观测, 改成 for 会多读一次完成标志)。

- 必须守住的不变量:

  - **函数名 / 签名 / 链接性不变**: 15 个外部入口名逐字保留, include 集合不变 (`MotorParaIDinclude.h` 一个)。新增的只有 1 个 `static void RrLoWaitTick(void)` —— static 是内部链接, 不进 nm2000 的 C_EXT, 也不会被 MSVC 发成 linkonce/COMDAT (那是 `void inline` 才会踩的坑)。目标端外部符号集合 54 个改前改后**逐项相同** (`target_check.py` 判 `符号集合相同: True`)。
  - **整幅输入映像, 而不是「模块自有全局量」**: 本模块一个全局量都不定义, ref 跑完 new 看到的起点已被 ref 改过。测试台把 30 组外设/全局对象整体装进 `struct IMG`, `install(&g_in) -> ref() -> capture(&g_a) -> install(&g_in) -> new() -> capture(&g_b) -> compare()`。整结构 memcpy 有两个好处: 不存在「某个字段忘了重置」, 而且**每个字段自动成为输入**(不需要维护输入清单, 漏一项的代价是 new 从被污染的起点出发、报假 MISMATCH)。代价是 install() 必须显式把外设清零(让「没写」与「写了 0」可区分)、把每个除数钉在安全区。
  - **`#define int short` 是硬要求, 且 `interrupt` 空宏必须放在所有 include 之后**: MotorDefine.h 是 `typedef unsigned int Uint;`, C28x 上 int 16 位、宿主 32 位; 不折叠则所有 Uint 字段的截断全没了, 而本模块到处是截断 (gMotorExtReg.R1/R2/L0/LM/I0/RsPm 全是 Uint, m_UData1/m_UData2 全是 Uint)。`interrupt` 是 C28x 关键字, 本模块在**自己的源码里**写 `interrupt void RrLoInPeriodAndCmpInt(void)` (第 4 行原型 + 第 444 行定义), host_patch.json 只能补丁头文件、补不到 .c, 所以 shim 里加空宏 —— 但位置**必须**在所有 #include 之后: 放前面会把 DSP2803x_PieVect.h 的 `typedef interrupt void(*PINT)(void);` 一起改掉, 与 host_patch.json 提供的整份补丁副本互相覆盖。
  - **中断向量归一化**: PieVectTable.EPWM1_INT / EPWM2_INT 被本模块写。ref 写 &ref_RrLoInPeriodAndCmpInt、new 写 &new_..., 天然不等, 所以比较前把两个字段置 0, 另用 vecTag[2] 归一 (0 空 / 1 本模块 ISR 两侧同值 / 2 EPWM1_zero_isr / 3 dummy_isr / 9 其它)。install() 每次把两个向量都写成 dummy_isr, 于是「模块没写向量」与「模块写回了原向量」能区分开。
  - **`ADC_CLEAR_INT_FLAG` / `ADC_END_CONVERSIN` 的「次数」搬进了比较镜像**: 前者是 `AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1` (写只写寄存器, 内存里留下一个恒为 1 的位), 而 RrWaitAddGetUdc 里**两次**清标志 (基准 492 / 508 行) 写的是同一个 1 —— 「少清一次」在终值上完全重合。后者是一次只读, 超时循环的圈数只体现为「读了几次」。于是 RW2/RW4/RW6/RW8 四条变异在纯状态比较下**恒等**, 第一轮全部 MISSED。修法: host_patch.json 把两条宏改成 `adcClearIntFlag()` / `adcEndConversion()` (函数体在 mock 里, 行为与真宏一致), 次数进 IMG 的 adcClr / adcRead 两个字段。这是把观测粒度提到硬件的**副作用序列**粒度, 不是作弊: 少清一次会让下一次等待立刻返回、读到过期的母线电压, 正是这条路径要防的 bug。side=both, 目标端一个字节不变。
  - **除数钉死 (pin_rs / pin_rrlo / pin_calc)**: 11 处变量除数。除零在宿主上不是一行 MISMATCH 而是进程直接死 (0xC0000094), 整份台账作废, 且会被总账记成 CAUGHT —— 比 MISSED 更危险。三个 pin_* 把模块里那几行算式照抄一遍, 算出 0 就调**输入**再算。`g_divPin` 记的是「输入被归一化了几次」, **不是**「发现了几次异常」: 随机相本来就会把字段撒到全空间, 归一化是设计的一部分; 真正要盯的是 (a) 进程没死 (b) pin_rrlo 六次都救不回来时打 BENCH-BUG。
  - **44 个覆盖计数器全部非零, 基准上一条都不许缺**: 计数器不是在调用点记意图, 而是比对 g_in 与 g_a **推出**实际走到了哪一支。缺一条直接判 BENCH-BUG 并让整个用例失败 —— 这条门必须能**让用例失败**, 一个只打印不判定的计数器等于没有 (第一版 RS_C35_LACK 恒为 0, 追下去是相位把 m_UData1 设成 0 导致缺相支永远走 UP 支)。
  - **`phase_bound` 的输入是反解出来的, 不是试出来的**: 随机相把字段撒到 0..65535, 于是 `Number >= 1024` 这类门槛只有输入恰好是 1022/1023/1024 才区分得开两个门槛 —— 命中概率 3/65536, 20000 个随机用例的期望命中数**不到 1**。凡是「门槛差 1」的变异都栽在这里, 表现为 MISSED 而不是「被测代码没问题」。所以 4299 例边界相按算式反解: RS1 解 `InvCurrent/4096` 的差值 + `Current^2 <= 1024*CurrentGet`; RS39 解 `(m_MaxCur*m_UData1) % Current == Current-1`; LM31 解 `DestFreq*100>>10` 要差 1 需 DestFreq >= 1024; LP5/LP6 解 qsin 导数 `|cos|*32767*pi/32768` 只在 |cos|≈1 时差 1~3。**关键一条**: 中间量只活在局部变量里时, 光让它差开没用 —— m_UData1 只喂给 `abs(gIUVWQ12.U) >= m_UData1` 这一次比较, 所以 iuvw.U 必须钉在两个结果**之间** (13737 与 600)。
  - **负向对照 356 条, 锚点三条判据全过**: `build/_mipe_negctl.py --check` 判 ①借位 (锚点自带缩进却匹配到缩进更深的行) ②撞语句 (锚点不带缩进、从行中间开始匹配) ③两条变异产生逐字节相同的变异源码; `build/_audit_anchors.py` 对全库 5522 条锚点复判, 本模块 356 条违规 0。锚点由**整行原文**构造 (可往前接行): case 2 与 case 3 的 `if(WaitCnt != 0){ WaitCnt--; ... }` 共 13 行只有首行缩进不同, case 4 与 case 6 的 `gUVCoff.Number++ / if(>= 512) / {` 三行逐字相同。
  - **rc 直方图只应出现 {1: N}**: 总账把**任何** rc != 0 都记 CAUGHT, 所以 caught == total 的台账里可能混着「进程崩在半路」的条目 —— 它后面的用例根本没跑, 却写着抓到。本模块第一轮就有一条: RC11 的 rc = 3221225620 = 0xC0000094 (整数除零)。审台账的第一件事是 rc 直方图, 不是看 verdict。
  - **EALLOW/EDIS 各次、位置不变, 且进比较镜像**: gHostCpuCtl[0..6] 整个进 IMG, 于是「少一个 EDIS」(EALLOW 悬空)、「多一对 DINT/EINT」、「临界区套错位置」才抓得到 —— 这类错只比结果永远看不出来。本模块有 9 个临界区 (RsIdentify case 1 / case 3/5, RrLoIdentify case 1 / case 4, SetPwmForRrLoId, EnableTimer, RrLoInPeriodAndCmpInt, RrWaitAddGetUdc, RrGetVoltReBegin)。代价: 改写时不能为了顺眼把两对 EALLOW/EDIS 合成一对。
  - **死声明 `m_CurrMultiple` 逐字保留**: 基准第 534 行 `int m_IdVoltage,m_zeroVoltageTime,m_index,m_CurrMultiple;` 里的 m_CurrMultiple 从未被引用, 目标端告警 1 条 (`line 534: variable "m_CurrMultiple" was declared but never referenced`)。删掉它会让告警从 1 条变 0 条 —— 那不是「不增加」而是**减少**, 会掩盖「改写引入另一条新告警但总数持平」的情况。保留后两侧告警都是 1 条, 任何新增都跑不掉。

| 项 | 结果 |
|---|---|
| 差分用例数 | **24,299** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AdcRegs`, `_AdcResult`, `_CBCLimitCurPrepare`, `_ChgParForEst`, `_EPWM1_zero_isr`, `_EPwm1Regs`, `_EPwm2Regs`, `_EPwm3Regs`, `_EnableTimer`, `_GetUDCInfo`, `_GpioDataRegs`, `_ImCalKpKi`, `_InitLmIoVaribale`, `_InitRrLoVariable`, `_InitSetAdc`, `_InitSetPWM`, `_LmIoCalValue`, `_LmIoIdentify`, `_LmIoInPeriodInt`, `_PieCtrlRegs`, `_PieVectTable`, `_RrGetVoltReBegin`, `_RrLoCalVaule`, `_RrLoIdentify`, `_RrLoInPeriodAndCmpInt`, `_RrWaitAddGetUdc`, `_RsIdentify`, `_SetInterruptEnable`, `_SetPwmForRrLoId`, `_gADC`, `_gBasePar`, `_gCtrMotorType`, `_gError`, `_gExcursionInfo`, `_gFVCSpeed`, `_gGetParVarable`, `_gIAmpTheta`, `_gIUVWQ12`, `_gInvInfo`, `_gLmIoIdentify`, `_gMainCmd`, `_gMotorExtReg`, `_gMotorInfo`, `_gOutVolt`, `_gPGData`, `_gPWM`, `_gPhase`, `_gPmParEst`, `_gRrLoIdentify`, `_gUDC`, `_gUVCoff`, `_gVFPar`, `_qsin`, `_qsqrt` |
| `.text` 尺寸 | 1942 → 1920 (**-22**) |

### `src/motor/MotorVCMain.c`

- 变换: 重复片段提为 static 帮助函数 (ResetAsrTotals / ResetVoltOut / PrepareTorqueLimitPid / CalAsrKi / PrepareUdcPid / VcLowFreqHold / SnapshotImTs / LimitVoltAmp / ClearVoltWhenNotRun / SetVoltPhase)、两处 if/else 取反改提前返回、<<7/<<8 与 <<9/<<10 的双支改为先定移位再统一计算、插值系数两处除法合并为 CalAsrKi

- 必须守住的不变量:

  - 函数名 ResetParForVC / ResetAsrPar / ResetCsrPar / CalIMSet / CalTorqueLimitPar / PrepareAsrPar / CalUdcLimitIT / VcAsrControl / CalWsAndSynFreq / CalWsAndSynFreq_380 / PrepImCsrPara / PrepareCsrPara / VCCsrControl / SvcCalOutVolt / SvcCalOutVolt_380 / CalcABVolt / VCCsrControl_380 与全局 16 个不变
  - DINT/EINT 与 EALLOW/EDIS 的**条数与配对**不变 (SnapshotImTs 只是把同一对搬进 static 函数, gHostCpuCtl 计数两侧逐位相同)
  - 所有算术表达式保持原有类型宽度; VCTorqueLim 是 Uint, 钳位处不得包进 int 形参
  - LimitVoltAmp / ClearVoltWhenNotRun 的实参 Limit 与原三元里的常量逐一对应 (4916 / maxVolt2)
  - CalAsrKi 的 Ulong 移位与截断点与原式一致, 返回 int 后由赋值隐式截断

| 项 | 结果 |
|---|---|
| 差分用例数 | **86,945** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_CalIMSet`, `_CalTorqueLimitPar`, `_CalUdcLimitIT`, `_CalWsAndSynFreq`, `_CalWsAndSynFreq_380`, `_CalcABVolt`, `_PID`, `_PID32`, `_PrepImCsrPara`, `_PrepareAsrPar`, `_ResetAsrPar`, `_ResetCsrPar`, `_ResetParForVC`, `_ResetSVC`, `_SVC0Signal`, `_SvcCalOutVolt`, `_SvcCalOutVolt_380`, `_VCCsrControl`, `_VCCsrControl_380`, `_VcAsrControl`, `_atan`, `_gABVoltSet`, `_gAsr`, `_gBasePar`, `_gComPar`, `_gCtrMotorType`, `_gFluxWeak`, `_gIMSetAcr`, `_gIMTQ12`, `_gIMTQ12_obs`, `_gIMTQ24`, `_gIMTQ24_obs`, `_gIMTSet`, `_gIMTSetApply`, `_gIMTSetQ12`, `_gImAcrQ24`, `_gInvInfo`, `_gItAcrQ24`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtPer`, `_gMotorInfo`, `_gOutVolt`, `_gOvUdc`, `_gPWMVAlphBeta`, `_gPhase`, `_gPmDecoup`, `_gSvcVoltAdj`, `_gTestDataReceive`, `_gUAmpTheta`, `_gUDC`, `_gUMTSet`, `_gUdcLimitIt`, `_gVCPar`, `_gVFPar`, `_gVcCsrPara`, `_gWspid`, `_qsin`, `_qsqrt` |
| `.text` 尺寸 | 2279 → 2198 (**-81**) |

### `src/motor/MotorVF.c`

- 变换: VVVF (恒压频比) 控制模块。16 个外部链接入口: VfVarInitiate / VFOVUdcLimit / CalOutVotInVFStatus / CalTorqueUp / VFWsTorqueBoostComm / VFAutoTorqueBoost / VFWSCompControl / VFOverMagneticControl / VFSpeedControl / VfOverCurDeal / VfOverUdcDeal / VfFreqDeal / VfOscIndexCalc / HVfDeadBandComp / HVfOscDampDeal / HVfCurReDecomp, 全部无参数 (CalOutVotInVFStatus 收一个 int freq)。改写手法 (1020 -> 963 行): (1) VfVarInitiate 的正/负相序 if/else 改成三元; (2) VFOVUdcLimit 把 `if(A && B) { 计数 } else { 清零 }` 改成 De Morgan 的提前返回 (`if(!A || !B) { 清零; return; }`), 计数与钳位留在尾部; (3) CalOutVotInVFStatus 的尾段 `if(|f|==0){...} else if(|f|>=FreqPer){...}` 改成两个独立提前返回, 两处钳位各自 `return ((a > b) ? b : a)`; (4) CalTorqueUp 的 `if(skip){...} else { 主体 }` 改成提前返回, 且把`(VFTorqueUp==0)&&(VFOvShock==0)` 按 De Morgan 取反成 `(VFTorqueUp!=0)||(VFOvShock!=0)`, 把斜面支提到 PID 支之前 (两支互补, 逐输入同支), `m_VoltUp = 0` 提到函数头、`detVfVoltUp` 的写入提出 else (skip 支显式写 0); (5) VFWsTorqueBoostComm 的 `Coff > 1400` 支改成提前返回, 角度符号的 if 改成三元; (6) VFAutoTorqueBoost 的三档 m_LowerLimit 改成嵌套三元, 内层 `先赋 EsVolt 再按 <950 覆盖` 改成三元, PID.Deta 的三支 (>>5 无掩码 / >>3 &0xFFFC / >>1 &0xFFF0) 改成`(>>m_Shift) & m_Mask` 并令无掩码支的 m_Mask = -1L (long 上恒等); (7) VFWSCompControl 的 `if(speed_CON && VFWsComp){ if(DelayTime>100){...} else DelayTime++ } else DelayTime=0` 改成三段 if/else-if/else (门取反提前、自增支提中); (8) VFOverMagneticControl 的 m_UDCDesired 改成三元; (9) VfOverCurDeal 的 SubStep/m_AddStep、MaxStepLow/MaxStepHigh、CurLim、m_MaxStep 四处 if/else 改成三元 (CurLim 的 `15 == InvTypeApply ? 170 : ocPoint` 先选操作数再统一 `(long) <<12 / 100`); (10) VfOverUdcDeal 的 m_Add1/m_Add2 改成三元, `if(m_DetaU<0) += m_Add2 else += m_Add1` 改成三元增量, 并删掉一段**死代码** (见 dead_code 第一条); (11) VfFreqDeal 的 m_Dir 与 `m_Deta = -m_Deta; if(FreqApply==0) m_Deta=0` 改成三元; (12) HVfDeadBandComp 的三段 `CompU / CompV / CompW` 逐字重复块改成`int *comp[3]` + 一个三拍循环 (每拍末尾 `phase -= 21845`, 与原写法逐拍一致).

- 必须守住的不变量:

  - **函数名 / 签名 / 链接性不变**: 16 个外部入口名逐字保留, include 集合不变 (MotorVFInclude.h + MotorInclude.h), **没有新增函数**, 也没有把任何一段抽成 static helper —— 16 个入口 1:1 对应原工程。目标端外部符号集合 54 个改前改后**逐项相同** (`target_check.py` 判 `符号集合相同: True`)。
  - **16 个模块自有全局量名与类型逐字不变** (`gVFPar` / `gVFAutoVar` / `gWsComp` / `gVarAvr` / `gVfOsc` / `gOscAmp` / `gVfOverUdc` / `gOvCur2` / `gVfFreq2` / `gOvCur` / `gOvUdc` / `gHVfOscDamp` / `gHVfOscIndex` / `gHVfDeadBandCompOpt` / `gHVfCur`), 用例的 ref_tu.c / new_tu.c 按名字重命名它们。
  - **所有算术表达式保持原有类型宽度**: `int` 在目标端是 16 位 (宿主 shim 用 `#define int short` 折叠), 所以 `m_MaxFreqUp = ((Ulong)VFTorqueUpLim<<15)/FullFreq` 这类赋值仍然**截断到 16 位**; 改写只动控制流, 一个乘除、一个移位、一个 `(long)` 转换的位置都没有动。逐条改写都对着原文抄算式。
  - **`m_Shift` / `m_Mask` 引入后 PID.Deta 仍逐位相同**: 无掩码支用 `m_Mask = -1L` (`x & -1L` 在 32 位 long 上恒等, 负数也成立); 另两支的 `0xFFFCL` / `0xFFF0L` 与原文的 `0xFFFC` / `0xFFF0` 在 long 上同值。
  - **HVfDeadBandComp 的三拍循环与原三段逐拍同值**: 原文在每一段之后都执行 `phase -= 21845` (最后一段之后也执行, 但 phase 是局部量、之后不再被读), 循环同样在每拍末尾减, 且 `CompU/CompV/CompW` 都是 `int` 字段, 取地址合法。
  - **死代码刻意保留**: 见 dead_code 第一条 —— `PhaseFwdFedCoeff` 那条 14 行链**没有删**。删掉它会让 `temp` 变成未使用变量, 目标端告警从 3 条变 4 条 (或改变内容); 而「改写引入一条新告警但总数持平」正是告警集合检查要防的事, 所以宁可留着这段死代码, 保住告警集合逐字不变。
  - **宿主差分: 冻结基准 vs 交付源码 32503 例 0 处不一致** (17 个手写相 + 2000 随机), 见 docs/_difftest/MotorVF.json。比较镜像是 27 个字段: 15 个模块自有对象 + `gOutVolt` / `gPhase` / `gDeadBand` / `gIAmpTheta` / `gMainCmd` + `CalOutVotInVFStatus` 的返回值 + PID / AlphBetaToDQ 的 6 项调用台账。
  - **141 个覆盖计数器全部非零**, 任一为 0 就让用例返回 1。覆盖计数器必须在**被测模块的宽度**上复算 —— 第一轮 `cov_observe` 把 `m_MaxFreqUp` 当 32 位算, 于是它报告「PID 臂走过」而模块实际走的是 `m_DetaFreq <= 0` 那一支, 三条变异因此假 MISSED。
  - **负向对照 179 条全部 CAUGHT, 0 BROKEN, 0 HUNG**, rc 直方图 `{1: 179}`。按函数分组: VF 9 / VU 4 / CF 17 / CU 11 / WQ 14 / AT 20 / WC 16 / OM 8 / SC 2 / OC 17 / UD 17 / FD 13 / OI 6 / DB 14 / OD 4 / CD 7。锚点由 `build/_vf_negctl.py` 生成并对每条断言「片段在该行恰 1 次」「整行锚点在整份基准里恰 1 次」「替换有效」「两条锚点互不相同」; `build/_audit_anchors.py` 对全库 6005 条锚点复判, 本模块179 条违规 0。
  - **锚点唯一性在本模块踩过两次真坑**: `m_Mid = ((Ulong)gOvUdc.Limit * 3932L)>>12;` 在 VFOVUdcLimit(第 83 行) 与 VfOverUdcDeal(第 669 行) 是**同一行文本**, `build_variant` 的 `replace(old, new, 1)` 只打第一处 —— 单行锚点会让 UD4 打在 VFOVUdcLimit 上, 台账却写 CAUGHT。两条都改成跨行锚点 (span=3 / span=2) 才分开。
  - **删除清单里的变异时必须重跑全量, 不能 `--resume`**: `--resume` 只接 CAUGHT 记录, 会把旧锚点/旧 driver 的 CAUGHT 当成新证据留下。本模块两轮全量重跑, 第二轮 179 条、第三轮 (改 driver 之后) 仍 179 条。

| 项 | 结果 |
|---|---|
| 差分用例数 | **32,503** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AlphBetaToDQ`, `_CalOutVotInVFStatus`, `_CalTorqueUp`, `_HVfCurReDecomp`, `_HVfDeadBandComp`, `_HVfOscDampDeal`, `_PID`, `_VFAutoTorqueBoost`, `_VFOVUdcLimit`, `_VFOverMagneticControl`, `_VFSpeedControl`, `_VFWSCompControl`, `_VFWsTorqueBoostComm`, `_VfFreqDeal`, `_VfOscIndexCalc`, `_VfOverCurDeal`, `_VfOverUdcDeal`, `_VfVarInitiate`, `_atan`, `_gBasePar`, `_gCtrMotorType`, `_gDeadBand`, `_gExponentVf`, `_gExtendCmd`, `_gHVfCur`, `_gHVfDeadBandCompOpt`, `_gHVfOscDamp`, `_gHVfOscIndex`, `_gIAlphBeta`, `_gIAmpTheta`, `_gIMTQ12`, `_gIMTQ24`, `_gInvInfo`, `_gLineCur`, `_gMainCmd`, `_gMainStatus`, `_gMotorExtInfo`, `_gMotorExtPer`, `_gMotorInfo`, `_gOscAmp`, `_gOutVolt`, `_gOvCur`, `_gOvCur2`, `_gOvUdc`, `_gPhase`, `_gUDC`, `_gVFAutoVar`, `_gVFPar`, `_gVarAvr`, `_gVfFreq2`, `_gVfOsc`, `_gVfOverUdc`, `_gWsComp`, `_qsin`, `_qsqrt` |
| `.text` 尺寸 | 2505 → 2478 (**-27**) |

### `src/function/f_comm.c`

- 变换: SCI/LIN 串行通讯层。23 个外部入口: `SciDeal`(主循环轮询), 两个 SCI 中断 `SCI_RXD_isr` / `SCI_TXD_isr`, 两个 LIN 中断包装 `Lina_Level0_ISR` / `Lina_Level1_ISR`, 报文分发 `CommRcvDataDeal` / `CommSendDataDeal` / `SciErrCheck` / `commStatusDeal` / `commErrorDeal` / `CommDataReRcv` / `CommDataSend` / `CommStartSend`, funcCode 远程读写 `CommWrite` / `CommRead` / `CommRwFuncCode`, 以及八个真外设编程入口 `InitSciaGpio` / `InitSetScia` / `UpdateSciFormat` / `resetLinSci` / `closeRTX` / `setRxConfig` / `setTxConfig`。改写手法 (1197 -> 1172 行, 27640 -> 27329 字节): (1) `SciDeal` 的错误标志分派取反改提前返回, 30 s 密码超时门槛收成宏 `COMM_PWD_HOLD_TICK`; (2) `SCI_RXD_isr` 的两层嵌套 if 合成一个 `&&` (`StartDeal` 仍先求值, `rcvFlag` 仍只在它为真时读); (3) `Lina_Level0_ISR` 的 if/else-if/else 改 `switch` (11 / 12 / default); (4) `CommRcvDataDeal` 的 `if(!slaveAddr){...}else{...}` 拍平成 `if / else if (CRC) / else`, 命令字三分支改 `switch`, 尾部 `if (sciFlag.bit.write) {...}` 改提前返回; (5) `SciErrCheck` 的八段 if/else-if 优先级链改成顺序提前返回 (`operateErr` 局部量随之消失); (6) `CommSendDataDeal` 把 `sendNumMax - 2` 只算一次存进 `crcAt`, 第二字节写 `crcAt + 1`; (7) `CommWrite` 的八个地址常量 if/else-if 链改 `switch (addr)`, 三处逐字重复的 AO/HDO 钳位块 抽成 `static Uint16 commSetAo(Uint16, Uint16)`, `canOpen` 翻转改三元 + 取反; (8) `CommRead` 的 0x1000 支两臂互换 (先判在界内), 三段等值地址改 `switch`, 状态字正反转两臂与键盘判据各改三元, `(i+low)==DISP_OUT_CURRENT` 与 `commReadCurrentPoint` 合成一个 `&&`; (9) `CommRwFuncCode` 的组号计算改 `switch (highH)`, `gradeAdd` 改三元, 三层 OR 的禁写判据拆成顺序提前返回, 写支的三段 else-if 改独立 if + return, paraInit 的五段 `else if (... == data)` 链改 `switch (data)`, 尾段 EEPROM 判据改提前返回; (10) `commStatusDeal` 拆掉 `SCI_RECEIVE_OK -> SCI_SEND_DATA_PREPARE` 的**隐式 fallthrough**, 把准备发送的体抽成 `static void commSendPrepare(void)` 供两处调用; (11) `CommDataReRcv` 的第二个 `if (rcvNum >= rcvNumMax)` 改成 `if (rcvNum < rcvNumMax) return;`, `rcvNum++` 拆成显式两步; `CommDataSend` 同理改提前返回; (12) `InitSetScia` 与 `UpdateSciFormat` 里**逐字相同**的「CAN_OPEN 选 0, 否则选 commParity」两段收成 `static void setLinParity(void)`; (13) `UpdateSciFormat` 把 `commProtocolSec` 缓存进 `protoSec` (仅限四次 `GetNumberDigit1` 之前 —— 该函数只写 `digit[0..4]`, 对 `funcCode` 是纯的; `UpdateCommFormat` 之后的那次读仍读全局)。**刻意没动的八个入口**: `CommStartSend` (一行寄存器写), `resetLinSci` / `closeRTX` / `setRxConfig` / `setTxConfig` (纯寄存器写序列), `commErrorDeal` (四行赋值 + 一对 EALLOW/EDIS), `Lina_Level1_ISR` (一行 PIEACK), `InitSciaGpio` (两组 EALLOW/EDIS 里的 20 个位域写, 重排只会在硬件上引入时序风险而不改变任何可观测结果 —— 那是假的改写)。

- 必须守住的不变量:

  - **函数名 / 签名 / 链接性不变**: 23 个入口名逐字保留, include 集合不变 (`f_comm.h` / `f_p2p.h` / `f_ui.h`)。注意 `LOCALF` / `LOCALD` 在 `include/function/f_debug.h:27-33` 的 `#if 1` 分支里分别展开成**空**与 `extern`, 所以那些「看着像 static」的入口在目标端是**外部链接** (`nm2000` 里 `_CommRwFuncCode` / `_CommRead` / `_commStatusDeal` 都是 `T`)。因此新增的三个 helper 一律**显式写 `static`** 而不是 `LOCALF` —— 写成 `LOCALF` 会多出 `_commSetAo` / `_setLinParity` / `_commSendPrepare` 三个外部符号。`target_check.py` 判 `符号集合相同: True` (78 个, 一条不多一条不少), 两侧告警都是 0 条, `.text` 1642 -> 1647 (**+5**, 来自 `switch` 派发与 `commSendPrepare` 的调用序列; 记录, 不作门禁)。
  - **`CommStartSend` 的 `inline` 语义没有被新增调用点破坏**: 它是 `void inline`, TI cl2000 对 C11 `inline` 定义**不发外部符号**, 所以它从来不在 C_EXT 里。改写后 `commSendPrepare` 又调了它一次 (原先是 `commStatusDeal` 里唯一一处调用), 目标端仍然全部内联, `_CommStartSend` 依旧不出现 —— 这一条是**实测**的, 不是推的: 若哪天 TI 改成发外部定义, `target_check.py` 的符号集合会立刻不等。
  - **`CommRwFuncCode` 的 `rwMode` 有第三个取值, 判据不能写成 `else`**: driver 有一条 `runRwFc(..., 5, 2)` —— `rwMode = 2`, 既不是 `COMM_READ_FC(0)` 也不是 `COMM_WRITE_FC(1)`。原文两处都写 `if (COMM_READ_FC == rwMode)` / `if (COMM_WRITE_FC == rwMode)`, 于是 `rwMode = 2` 「非读非写」: 禁写表不查、密码支不进、最后**落到写路径** (ModifyFunccodeUpDown)。改写把禁写判据拆成 `if (COMM_WRITE_FC == rwMode) {...} else if (COMM_READ_FC == rwMode) {...}` —— **必须保留这两个显式判据**, 一旦图省事写成 `else` 就会把 `rwMode = 2` 当成读, 而那条用例正是专门盯这个的。
  - **组号 `switch (highH)` 与 FP 覆写的先后可以交换, 但要能证**: 原文顺序是 A0/B0/C0 -> `(addr & 0xFF00) == 0x1F00` -> `0x7000`; 改写把 `0x7000` 并进 `switch`、FP 覆写挪到 switch 之后。等价理由: `(addr & 0xFF00) == 0x1F00` 蕴含 `highH == 0x1000`, 而 `0x1000` 不在 {0xA000, 0x4000, 0xB000, 0x5000, 0xC000, 0x6000, 0x7000} 里, 所以两条路径互斥, FP 覆写永远是在「switch 什么都没加」的前提下发生。
  - **`gradeAdd` 的 `Uint16` 回绕逐字保留**: 原文 `gradeAdd = data - 1;` (`Uint16`), `data = 0` 时得到 0xFFFF; 改写写成 `(COMM_READ_FC == rwMode) ? (Uint16)(data - 1) : 0` —— 强转是**必须**的, 去掉它三元的两臂会提升成 `int` (宿主 32 位 / 目标 16 位), 赋值回 `Uint16` 时才截断, 结果虽然相同, 但 `grade + gradeAdd` 那一步的比较宽度就不再是原文的宽度了。
  - **EALLOW / EDIS 的条数与位置一个都没动**: 全模块 9 对 (`commSendPrepare` 1 / `commErrorDeal` 1 / `commStatusDeal` 的 `SCI_SEND_OK` 支 1 / `CommDataReRcv` 1 / `InitSetScia` 1 / `UpdateSciFormat` 首尾各 1 / `InitSciaGpio` 2)。`gHostCpuCtl[0..6]` 整体进比较镜像, 所以「少一个 EDIS」(EALLOW 悬空)、「多一对 DINT/EINT」、「临界区套错位置」都看得见 —— 这类错只比结果永远看不出来。代价: 改写时不能为了顺眼把两对 EALLOW/EDIS 合成一对 (抽 `commSendPrepare` / `setLinParity` 时特意确认了 helper 体内**不含** EALLOW/EDIS)。
  - **宿主差分: 冻结基准 vs 交付源码 24766 例 0 处不一致** (15 个手写相 4766 例 + 20000 随机), 见 `docs/_difftest/f_comm.json`。23 个入口全部被驱动, 最少 806 例 (`CommStartSend`), 最多 3066 例 (`CommRwFuncCode`)。比较镜像是四类面: 模块状态逐字段 (`commStatus` / `commRcvData` 14 字段 / `commSendData` 6 字段 / `sendFrame` / `rcvFrame` / `commReadData` / `sciFlag` / `aoComm` / `doComm` / `commRunCmd` / 三个密码通过标志 / `commTicker`), 外设整体按字节 (`LinaRegs` / `GpioCtrlRegs` / `GpioDataRegs` / `PieCtrlRegs` / `IER` / `gHostCpuCtl`), `PieVectTable` 折算成 0/1/2 编码 (两侧装的是各自的 ISR 地址, 直接比字节是噪音), 以及 mock 调用日志 (含 `ModifyFunccodeUpDown` 的 `delta` 与 `SaveOneFuncCode` 排进 EEPROM 写队列的 index)。
  - **负向对照 128 条全部 CAUGHT, 0 MISSED, 0 BROKEN, 0 HUNG**, `rc` 直方图 `{1: 128}` (`docs/_difftest/_negctl_partial_f_comm.json` 的 `pass: true`)。锚点由 `build/_fc_negctl.py` 生成并对每条断言「片段在该行恰 1 次」; `build/_audit_anchors.py` 对全库 6133 条锚点复判, 本模块 128 条违规 0。覆盖: 每个私有地址常量、每个错误码、每个状态机转移各至少一条。
  - **测试台的四个 bug 都是先出现成片 MISSED 才定位的, 记在 `cases/f_comm/README.md` §5**: ① `run1()` 里的 `g_curRw = 0` 把 2205 个「写」例全跑成了读, `CommRwFuncCode` 的整个写分支从未执行; ② 随机相的 `sendNumMax = rnd32() % 36` 让 `sendFrame[sendNumMax - 2]` 在 `Uint16` 上回绕, 进程崩在第 705 例, 而总账把任何 `rc != 0` 都记 CAUGHT, 于是 129 条里 128 条「抓到」实际一条差异都没算; ③ `ModifyFunccodeUpDown` 替身照真实现写回 `*data`, 而 paraInit 那串判据用的是**回填之后**的 `data`, 驱动只把值当入参传、没设 `upDownData`, 六条支一条都进不去; ④ 四个输入维度 (`canOpen` 翻转 / `runFlag.bit.run,dir` / `keyBordValue` / `frameSpaceTime` / `GPAPUD` 初值) 从没被驱动过。第 ② 条是本轮最重要的一课: **审台账的第一件事是 `rc` 直方图, 不是看 verdict**。
  - **`RF14` 判定为可达状态空间内等价, 从清单里删除 (不是标记排除)**: `f_funcCode.h:336` 的 `AFNUM` 是 `1+0`, `group.af` 是 1 元素占位数组, 所以 `funcCodeGradeAll[32] == AFNUM - 1 == 0`; 任何解码到 32 组的地址都必然在 `grade + gradeAdd >= funcCodeGradeComm[32]` (`>= 0`) 处提前返回, 于是 `group == FUNCCODE_GROUP_AF` 恒假, 「隐藏组锁判据取反」两臂在整个可达状态空间里同值。`test_difftest_negctl.py` 从 `CASES` 推导期望集合, 留着会永远差一条。
  - **`CommRwFuncCode` 的 `Uint16 funcCodeGradeComm[FUNCCODE_GROUP_NUM]` 是 83 项栈数组**, `group` 的取值范围算得出来: `(addr >> 8) & 0x0F` 得 0..15, 加 A0(17)/B0(33)/C0(49)/U0(67) 最大 82, FP 是 16 —— 恰好落在 83 以内。改写把三处 `group +=` 挪进 `switch` 没有改变这个上界。

| 项 | 结果 |
|---|---|
| 差分用例数 | **24,766** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_COMM_ERR_INDEX`, `_ClearRecordDeal`, `_CommDataReRcv`, `_CommDataSend`, `_CommRcvDataDeal`, `_CommRead`, `_CommRwFuncCode`, `_CommSendDataDeal`, `_CommWrite`, `_CrcValueByteCalc`, `_GetNumberDigit1`, `_GpioCtrlRegs`, `_GpioDataRegs`, `_InitSciaGpio`, `_InitSetScia`, `_LinaRegs`, `_Lina_Level0_ISR`, `_Lina_Level1_ISR`, `_ModbusCommErrCheck`, `_ModbusRcvDataDeal`, `_ModbusSendDataDeal`, `_ModbusStartDeal`, `_ModifyFunccodeEnter`, `_ModifyFunccodeUpDown`, `_PieCtrlRegs`, `_PieVectTable`, `_ProfibusCommErrCheck`, `_ProfibusRcvDataDeal`, `_ProfibusSendDataDeal`, `_ProfibusStartDeal`, `_SCI_RXD_isr`, `_SCI_TXD_isr`, `_SciDeal`, `_SciErrCheck`, `_UpdataFuncCodeGrade`, `_UpdateModbusCommFormat`, `_UpdateProfibusCommFormat`, `_UpdateSciFormat`, `_aoComm`, `_canLinkTicker`, `_closeRTX`, `_commDispIndex`, `_commErrorDeal`, `_commParitys`, `_commProtocol`, `_commRcvData`, `_commReadData`, `_commRunCmd`, `_commSendData`, `_commStatus`, `_commStatusDeal`, `_commTicker`, `_commType`, `_companyPwdPass4Comm`, `_doComm`, `_dspBaudRegData`, `_errorCode`, `_errorInfo`, `_errorOther`, `_funcCode`, `_funcCodeGradeSum`, `_funcCodeOneWriteIndex`, `_funcCodeOneWriteNum`, `_funcCodeRwMode`, `_funcCodeRwModeTmp`, `_groupHidePassComm`, `_keyBordTestFlag`, `_keyBordValue`, `_p2pData`, `_protocolFunc`, `_rcvFrame`, `_resetLinSci`, `_runFlag`, `_sciFlag`, `_sendFrame`, `_setRxConfig`, `_setTxConfig`, `_userPwdPass4Comm` |
| `.text` 尺寸 | 1642 → 1647 (**+5**) |

### `src/function/f_io.c`

- 变换: IO 抽象层: 数字输入 / 数字输出 / 模拟量 / 脉冲 / 折线 / 低通 / 温度。34 个入口: `DiCalc` / `DoutDeal` / `DoCalc` / `AiCalc` / `AoCalc` / `AoCalcChannel` / `PulseInCalc` / `PulseInSample` / `PulseOutCalc` / `FMPDeal` / `UpdateFmpAoValue` / `LineCalc` / `LpfCalc` / `TemperatureDeal` / `SensorDeal` / `AfterDeal` / `GetTemperature` / `GetTemperatureCalc` / `Di2Bin` / `Do2Bin` / `getDiHwStatus` / `setDOStatus` / `InitDIGpio` / `InitDOGpio` 以及 9 个死入口。改写手法 (3188 -> 2516 行, 95688 -> 57668 字节): (1) `DiCalc` 的去抖 / 虚拟 DI / AI-as-DI / 延时四个循环改升序 (每个 `i` 只碰自己那一位), 新增 `static Uint16 ioPulseSourceConflict(void)` 与 `static void ioSetOrClear(Uint32 *, Uint32, Uint16)`, **末级 `fc1[i] -> pDiFunc` 的循环必须保持倒序** (见 invariants); (2) `DoutDeal` 的 41 段分支: `Dout2/38/41` 抽出 `static Uint16 ioErrorOutputActive(void)` (三处判据逐字相同), `Dout3/25` 抽出 `static void ioFrqFdtArrive(Uint16 *, Uint16, Uint16)`, `Dout1/6/7/19/36` 的 `if (x) f = 1;` 改 `|=` / 三元, `Dout4/26/27` 把 `band` / `now` / `aim` 提到条件外各算一次, `Dout8/9/10` 两段 if 合成一个 `||`, `Dout34` 改提前返回 + `limit` 只算一次; (3) `DoCalc` 新增 `static Uint16 ioDoFuncFlag(Uint16)`, `doCommIndex[]` 改 `static const`, 循环改升序; (4) `TemperatureDeal` / `SensorDeal` / `AfterDeal` / `GetTemperature` 改提前返回, `period` 提成局部量, `GetTemperature` 的 if/else-if 改 `switch`, `AfterDeal` 两处 if 改三元; (5) `GetTemperatureCalc` 的 `for + break` 改 `while`; (6) `AiCalc`: `aiMin` 的「第一轮为 `AI_MIN_VOLTAGE_HW*10`、其后为 0」改成局部标志 `aiLowFirst`, 折线搜索 `for` 改 `while`, 嵌套 if/else 拍平; (7) `AoCalcChannel`: `EPwmRegs` / `pECapRegs` 在声明处用三元选边, `paraIndex` 提出来复用; (8) `PulseInCalc`: `pulseSum` / `lengthSum` 各只算一次, `if (0 == captureTicker)` / `if (!p->t)` 统一成 `0 ==` 风格; (9) `PulseInSample`: 四个 `ts` 先各自累加到 `tsSum`, 末尾一次性加进 `capturePeriodSum`; (10) `PulseOutCalc`: 新增 `static Uint16 ioHdoCompareTrack(...)`, `HDOCTR = (HDOCTR > 292) ? 0xFFFF : (Uint16)(HDOCTR * 224);` 之类改三元; (11) `UpdateFmpAoValue`: 用局部 `value` / `limit` 累到最后统一写, `FRQ_SET`/`FRQ_AIM`/`SPEED` 三支合并、`OUT_CURRENT`/`OUT_CURRENT_1` 两支合并; (12) `LineCalc`: 删掉 `#if 0` / `#elif 1` / `#elif 1` 三层只留活的那一种, `if (p->mode == 1)` 提到最前; (13) `LpfCalc`: 删掉 `#if 0` 那层, 删掉**写了不用**的 `int32 out;`, `if (!p->t)` 改提前返回; (14) `InitDIGpio` / `InitDOGpio` / `setDOStatus` 的 `? :` 改 if/else, `getDiHwStatus` 的 `|=` 改 `=` 并加显式 `(Uint16)` 转换, `Di2Bin` / `Do2Bin` 的条件合成一个局部 `changed`; (15) 顶部新增四个宏 `AI_AS_DI_LOW` / `AI_AS_DI_HIGH` / `MOTOR_T_SENSOR_AI_MAX_VOLTAGE_IDEA` / `LINE_SPEED_CALC_PERIOD`。**刻意没动的九个死入口**: `PulseOutDeal`、`InitSetEcap2/3/4`、`InitECap2Gpio/3Gpio/4Gpio` —— 它们不在 `.lkf` 的构建清单里, 改了既无证据也无收益, 逐字保留。

- 必须守住的不变量:

  - **`DiCalc` 末级 `fc1[i] -> pDiFunc` 的循环必须保持倒序**: 多个端子可以映射到**同一个** `fc` 位 (`fc1[i] / 32` 与 `1UL << (fc1[i] % 32)` 会撞), 所以「谁最后写谁生效」是有语义的。改写只把前面四个「每个 `i` 只碰自己那一位」的循环改成升序, 末级这个共享写口的循环**逐字保留倒序**。把它改成升序时差分测试立刻报不一致 —— 这是本模块唯一一处「看着像可重排、其实不可重排」的地方。
  - **函数名 / 签名 / 链接性不变**: 34 个入口名逐字保留, include 集合不变。注意 `LOCALF` / `LOCALD` 在 `include/function/f_debug.h:27-33` 的 `#if 1` 分支里分别展开成**空**与 `extern`, 所以 `Di2Bin` / `Do2Bin` / `getDiHwStatus` / `setDOStatus` / `UpdateFmpAoValue` 在目标端都是**外部链接** (`nm2000` 里是 `T`)。因此新增的 helper 一律**显式写 `static`** 而不是 `LOCALF` —— 写成 `LOCALF` 会多出 `_ioPulseSourceConflict` / `_ioSetOrClear` / `_ioErrorOutputActive` / `_ioFrqFdtArrive` / `_ioDoFuncFlag` / `_ioHdoCompareTrack` 六个外部符号。`target_check.py` 判 `符号集合相同: True`, `.text` 4523 -> 4410 (**-113 字节**, 来自四个 helper 消掉的三处重复体与拍平的 if 链)。
  - **差分测试**: `run_case.py f_io 20000` -> `cases=56213 mismatches=0` (固定相 36213 + 随机相 20000), 34 个入口全部被驱动。比较镜像含 `g_inM` / `g_inE` / `g_outM` / `g_outE` / `g_line` / `g_lpf` 以及 CPU 控制计数器 `gHostCpuCtl[]` (EALLOW/EDIS/DINT/EINT 的计数), 所以「少一个 EDIS」「临界区套错位置」这类只在副作用上可见的错也能被抓到。
  - **交付源码三条硬约束实测通过**: `bytes=57668` / `bare LF: 0` / `non-ascii: 0` / `/* count 0` / `// count 0` / `lines 2516`。CRLF、无注释、纯 ASCII。
  - **锚点审计**: `build/_audit_anchors.py` 对全库 6340 条锚点跑三条精确判据 (借位 / 语句形锚点撞行中 / 两条变异产生逐字节相同的变异源码), 本模块 **206 条、违规 0**。
  - **负向对照**: 206 条变异全 CAUGHT, `rc` 直方图 `{1: 206}` —— 每一条都是**干净的不一致** (rc == 1), 没有一条是靠进程崩溃 (0xC0000094 除零 / 0xC0000005 访问违例) 被总账记成 CAUGHT 的。这一点必须单列: 总账把任何 `rc != 0` 都记 CAUGHT, 所以「caught == total」本身不构成证据。
  - **`g_line` / `g_lpf` 必须在比较镜像里, 且两侧各有一份**: 这两个全局结构是 `LineCalc` / `LpfCalc` 的**唯一**输出通道。搭台时它们既没进 `snapshot()` 也没进 `compare()`, 于是 `K_LineCalc` / `K_LpfCalc` 一直处于「调了但没看结果」的状态, 对应 5 条变异全部假 MISSED。更隐蔽的是两侧共用同一个实例: `LpfCalc` 会读 `p->out`, 第二侧拿到的就成了第一侧的**输出**。修法: 加进 `struct SIDE`, `run1()` 每次 `install()` 前恢复初值。
  - **`doFunc` 是纯输出, 预填必须覆盖全 0 与全 1 两种**: 原测试台把 `g_inM.doFunc.f1/f2` 预填成 `0x0F0F0F0F`, 恰好盖住 `DoutDeal` 要写的 9 位 —— 「置 1」看不见 (本来就是 1), 「写 0」也看不见 (掩码里那几位是 0)。改成 2x2 组合 (`0x00000000` / `0xFFFFFFFF` 各一份), 例数 11670 -> 21504。
  - **门槛类判据要扫「门槛的整除因子」, 不能扫邻近值**: `Dout17/18/37/26/27` 的 `upperFrq` / `frqArriveValue1/2` 原预填是 500/10/300, 而驱动用的 `frqs[]` 里一个都没有 —— 相等点一次都到不了, `>=` 与 `>` 永不分歧。改成**从 `frqs[]` 本身取值**。同理 `DiCalc` 的去抖门槛预填 `10+i` 加一次最多到 13, 离 50/100/500 差得远, 改成 `dts[t] * 50 - 1`; `Dout30` 的保持相从 3 次改成 300 次 (arriveTcnt 这时是 249 / 248, 要一路减到 0 才看得见「少 1」)。
  - **`LpfCalc` 的 `if (!p->t)` 分支不能靠「跳过」来规避**: `p->t == 0` 是模块的合法输入 (除零在目标端 不陷入, 只给饱和值)。驱动必须喂 `t = 0`, 只能改变异本身 —— `LP2` 原来改成 `if (p->t)` 会引入除法, 改成 `if (!p->t) { p->out = p->outOld; }` 才是良定义的。同理 `UF13` 的 `lengthCurrent` 改成 `lengthCurrent + 1` (保底 1) 而不是把 `lengthCurrent == 0` 从定义域里删掉。
  - **`LP3` 的锚点必须带 `p->out`**: 原文里 `int32 out;` 是**声明了从没被读过**的死变量, `out = tmp / (p->t * 1) + p->outOld;` 与活语句 `p->out = ...` 只差三个字符, `src.find` 命中的是前者。`--dry-run` 报「出现 2 次, 命中第 2153 行」, 行号看着对, 但那一行是死的。这是「锚点唯一」纪律的第三个形状: 不只借位与撞语句, 还有**打在死赋值上**。

| 项 | 结果 |
|---|---|
| 差分用例数 | **56,213** |
| 不一致 | **0** |
| 进程退出码 | 0 |
| 目标端编译 (ref / new) | rc=0 / rc=0 |
| 目标端新增告警 | 无 |
| 外部符号集合 | **一致** |
| 外部符号 | `_AiCalc`, `_AoCalcChannel`, `_Di2Bin`, `_DiCalc`, `_Do2Bin`, `_DoCalc`, `_Dout1`, `_Dout10`, `_Dout11`, `_Dout12`, `_Dout13`, `_Dout14`, `_Dout15`, `_Dout16`, `_Dout17`, `_Dout18`, `_Dout19`, `_Dout2`, `_Dout21`, `_Dout22`, `_Dout23`, `_Dout24`, `_Dout25`, `_Dout26`, `_Dout27`, `_Dout28`, `_Dout29`, `_Dout3`, `_Dout30`, `_Dout31`, `_Dout32`, `_Dout33`, `_Dout34`, `_Dout35`, `_Dout36`, `_Dout37`, `_Dout38`, `_Dout39`, `_Dout4`, `_Dout40`, `_Dout41`, `_Dout5`, `_Dout6`, `_Dout7`, `_Dout8`, `_Dout9`, `_DoutRSVD`, `_ECap1Regs`, `_EPwm4Regs`, `_EPwm5Regs`, `_EPwm6Regs`, `_FMPDeal`, `_FcDigit2Bin`, `_GetNumberDigit1`, `_GetTemperature`, `_GetTemperatureCalc`, `_GpioCtrlRegs`, `_GpioDataRegs`, `_InitDIGpio`, `_InitDOGpio`, `_InitECap1Gpio`, `_InitECap2Gpio`, `_InitECap3Gpio`, `_InitECap4Gpio`, `_InitEPwm4Gpio`, `_InitEPwm5Gpio`, `_InitEPwm6Gpio`, `_InitSetEPWM4`, `_InitSetEPWM5`, `_InitSetEPWM6`, `_InitSetEcap1`, `_LineCalc`, `_LpfCalc`, `_PulseInCalc`, `_PulseInSample`, `_PulseOutCalc`, `_TemperatureAfterDeal`, `_TemperatureDeal`, `_TemperatureSensorDeal`, `_TorqueLimitCalc`, `_UpdateFmpAoValue`, `_aiDeal`, `_aiLpf`, `_aoComm`, `_aoFmpMax`, `_aoFmpValue`, `_bOverFlowCounterTicker`, `_bOverFlowLengthCurrent`, `_bPlcEndOneLoop`, `_bUv`, `_capturePeriodSum`, `_captureTicker`, `_curTime`, `_diDelayTicker`, `_diFunc`, `_diHwStatus`, `_diLogic`, `_diSelectFunc`, `_diStatus`, `_doComm`, `_doDelayTicker`, `_doFunc`, `_doFuncCal`, `_doHwStatus`, `_doLogic`, `_doStatus`, `_dspStatus`, `_errAutoRstNum`, `_errorAttribute`, `_errorCode`, `_errorDealStatus`, `_errorOther`, `_frq`, `_frqAim`, `_frqDisp`, `_frqRun`, `_funcCode`, `_generatrixVoltage`, `_getDiHwStatus`, `_itDisp`, `_lineSpeed`, `_lowerFrq`, `_mainLoopTicker`, `_maxFrq`, `_motorFc`, `_motorForeOT`, `_motorOT`, `_noCaptureTicker`, `_oCurrentCheckFlag`, `_oCurrentChkTicker`, `_outCurrentDisp`, `_outPower`, `_outVoltage`, `_pulseInFrq`, `_pulseInFrq1`, `_pulseInLpf`, `_pulseInSet`, `_runFlag`, `_setDOStatus`, `_setRunLostTime`, `_setRunTimeAim`, `_softOcDoFlag`, `_swingFrqLimit`, `_tempSampleSum`, `_temperature`, `_temperatureVoltage`, `_temperatureVoltageOrigin`, `_tickerTempDeal`, `_tuneCmd`, `_upperFrq`, `_vdiFcSet`, `_vdiSrc`, `_voltageTempPT100`, `_voltageTempPT1000` |
| `.text` 尺寸 | 4523 → 4410 (**-113**) |

### 宿主专用补丁 (`host_patch.json`)

差分测试在 x86 上重编源码, 于是会碰到**目标编译器接受、MSVC 不接受**的
写法。已确认的那一类是**静态初始值设定项里的"两个地址常量相减"**:

- MSVC 接受**单个**地址常量 (`offsetof(...)`, `&((T*)0)->m`)
- MSVC **拒绝任何两个地址常量之差**: 实测 6 种写法 (裸指针差、`char*` 差、
  `size_t` 差、`long` 差、经 `extern` 对象、`offsetof` 之差) 全部报 C2099
- 本机没有 gcc/clang (VS 的 LLVM 组件只带 `clang-format` / `clang-tidy`)

这类写法无法用 shim 绕开, 只能用**宿主专用补丁**: 把源文件改写成宿主可编译
的副本, 放进 `build/_difftest/gen_<case>/`, 并让该目录**优先**于原目录被
include。补丁以 `(原串, 替换串, 出现次数)` 描述, **出现次数不符就报错停下** ——
与其拿一份"看着像但已经不对"的副本去跑测试, 不如直接停。

被补丁的用例: `f_motorFc`, `f_invPara`, `f_fcDeal`, `f_error`, `f_menu`, `MotorInfoCollect`, `MotorParEst`, `MotorPublicCal`, `MotorSpeedCheck`, `MotorCarrier`, `f_main`, `f_interface`, `MotorEncoder`, `MotorDebug`, `SubPrg`, `MotorInvProtect`, `MotorMain`, `mainInit_2803x`, `MotorSVC`, `MotorPWM`, `f_comm_DP`, `MotorPmsmMain`, `MotorPmsmParEst`, `MotorParChange`, `MotorImParEst`, `MotorVCMain`, `MotorVF`, `f_io`

> 补丁**只作用于宿主副本**, 交付源码与目标端编译完全不受影响。

### 宿主 vs 目标端 布局对照 (`tools/difftest/target_layout.py <case>`)

差分测试用两套编译器编同一份源码, 所以"宿主上的结构体布局是否等于目标端"
必须**实测**。探针把每个用例关心的尺寸/偏移分别用 MSVC 与 cl2000 编一遍,
再并排比对:

**`f_motorFc`** — 不一致 **7 / 14** 项

| 项 | 单位 | 宿主 (x86) | 目标端 (C28x) | 差 |
|---|---|---:|---:|---:|
| `sizeof(MOTOR_FC)` | 字节 | 206 | 132 | +74 |
| `sizeof(MOTOR_PARA_STRUCT)` | 字节 | 54 | 54 | = |
| `sizeof(PG_PARA_STRUCT)` | 字节 | 20 | 20 | = |
| `sizeof(VC_PARA)` | 字节 | 46 | 46 | = |
| `sizeof(DI_FUNC)` | 字节 | 8 | 8 | = |
| `sizeof(FUNCCODE_ALL)` | 字节 | 2580 | 2232 | +348 |
| `sizeof(FUNCCODE_CODE)` | 字节 | 2580 | 2232 | +348 |
| `FNUM_ALL` | 个 | 1116 | 1116 | = |
| `word off motorFcM2.motorType` | 字 | 568 | 506 | +62 |
| `word off motorParaM1.ratingSpeed` | 字 | 35 | 34 | +1 |
| `word off motorParaM1.ratingFrq` | 字 | 34 | 33 | +1 |
| `word off frqPoint` | 字 | 22 | 22 | = |
| `word off motorSn` | 字 | 24 | 24 | = |
| `GetCodeIndex(motorFcM2.motorType)` | 字 | 568 | 506 | +62 |

**`f_invPara`** — 不一致 **27 / 28** 项

| 项 | 单位 | 宿主 (x86) | 目标端 (C28x) | 差 |
|---|---|---:|---:|---:|
| `sizeof(FUNCCODE_ALL)` | 字节 | 2580 | 2232 | +348 |
| `sizeof(FUNCCODE_CODE)` | 字节 | 2580 | 2232 | +348 |
| `FNUM_ALL` | 个 | 1116 | 1116 | = |
| `motor1ParaIndex[0] ratingPower` | 字 | 31 | 30 | +1 |
| `motor1ParaIndex[1] ratingCurrent` | 字 | 33 | 32 | +1 |
| `motor1ParaIndex[2] statorResistance` | 字 | 36 | 35 | +1 |
| `motor1ParaIndex[3] rotorResistance` | 字 | 37 | 36 | +1 |
| `motor1ParaIndex[4] leakInductance` | 字 | 38 | 37 | +1 |
| `motor1ParaIndex[5] mutualInductance` | 字 | 39 | 38 | +1 |
| `motor1ParaIndex[6] zeroLoadCurrent` | 字 | 40 | 39 | +1 |
| `motor1ParaIndex[7] pmsmRs` | 字 | 46 | 45 | +1 |
| `motor1ParaIndex[8] pmsmLd` | 字 | 47 | 46 | +1 |
| `motor1ParaIndex[9] pmsmLq` | 字 | 48 | 47 | +1 |
| `motor1ParaIndex[10] pmsmRsLdUnit` | 字 | 49 | 48 | +1 |
| `motor1ParaIndex[11] pmsmCoeff` | 字 | 50 | 49 | +1 |
| `motor1ParaIndex[12] mAcrKp` | 字 | 118 | 80 | +38 |
| `motor1ParaIndex[13] mAcrKi` | 字 | 119 | 81 | +38 |
| `motor1ParaIndex[14] tAcrKp` | 字 | 120 | 82 | +38 |
| `motor1ParaIndex[15] tAcrKi` | 字 | 121 | 83 | +38 |
| `motor1ParaIndex[16] pmsmCheckTime` | 字 | 51 | 50 | +1 |
| `motor1ParaIndex[17] enCoderDir` | 字 | 87 | 59 | +28 |
| `motor1ParaIndex[18] enCoderAngle` | 字 | 88 | 60 | +28 |
| `motor1ParaIndex[19] uvGainWarp` | 字 | 523 | 461 | +62 |
| `motor1ParaIndex[20] uvwSignDir` | 字 | 89 | 61 | +28 |
| `motor1ParaIndex[21] uvwSignAngle` | 字 | 90 | 62 | +28 |
| `motorSnIndexIncToMotor1[1] (M2-M1)` | 字 | 538 | 477 | +61 |
| `motorSnIndexIncToMotor1[2] (M3-M1)` | 字 | 641 | 543 | +98 |
| `motorSnIndexIncToMotor1[3] (M4-M1)` | 字 | 744 | 609 | +135 |

根因两条, 都已确认:

- `f_funcCode.h` 里 3 处 `Uint16 all[sizeof(struct X)]` (ERROR_SCENE /
  MOTOR_PARA / PG_PARA)。C28x 上 `sizeof` 返回**16 位单元数**, 正好等于
  `Uint16` 元素个数; x86 上返回**8 位字节数**, 数组因此大一倍, 把 union
  撑大, 进而撑大 `MOTOR_FC` 与 `FUNCCODE_CODE`。这是被测头文件的惯用法在
  x86 上的固有失真。
- `enum MOTOR_SN` 在 C28x 上是 1 个 16 位单元, MSVC 上是 4 字节, 使
  `motorSn` 之后的成员整体 +1 字。

**这对等价性结论的影响, 必须说清楚**: 两侧 (ref/new) 共用同一套头文件,
布局差异对两侧完全对称, "相同输入 → 相同输出" 的判据本身不受影响 —— 因此这
是**已量化的保真度缺口, 不是等价性缺口**。但它确实限制了下述推理: 宿主上
的等价性只覆盖"两份实现用同一套下标表达式算出同一个值", **不覆盖**"目标端
会写到同一个地址"。后者由第 5 节的**目标端外部符号集合检查**兜底: 改写后的
目标文件仍然引用同一组全局量 (`motor1ParaIndex` / `motorSnIndexIncToMotor1`
/ `funcCode` ...), 说明它仍然通过这几张表在算下标, 而不是把下标值写死。

**`text` 尺寸变化必须知情**: 改写会改变生成的指令序列, 因此**周期数会变**。
落在中断服务程序里的函数需要单独评估最坏执行时间, 本证据链不覆盖时序。

## 6. 静态验收 (与编译无关的补充证据)

| 检查 | 结果 |
|---|---|
| token 流等价 | **94 / 145 文件一致**, 0 处非预期不一致, 51 个改写模块按预期不同 |
| 注释残留 | **0 处** |
| TODO/FIXME/NOTE | **0 处** |
| 非 ASCII 字节 | **0 处** |
| 二次去注释可删字符 | **0 处** |
| 符号抽取不一致 | **0 处** |
| 隐式函数声明 | 28 处 (保留未修) |
| 中断函数定义 | 89 个 |

负向对照 (注入错误验证检查有效性): **4 / 4 类被抓到**, 总体判定 **通过**

| 注入的错误类型 | 是否被抓到 |
|---|---|
| token_mismatch | **是** |
| comment_residual | **是** |
| non_ascii | **是** |
| implicit | **是** |

基线(注入前)应全部为空: 是

## 7. 明确**未**证明的部分

| 项 | 原因 |
|---|---|
| 硬件上的实际行为 (PWM 输出、ADC 采样、CAN/SCI 收发、电机闭环) | 需目标板与台架 |
| 中断响应时序 / 最坏执行时间 | 需示波器或周期计数实测; 改写模块的 `.text` 尺寸已变化 |
| 目标端有符号/无符号混合比较的宽度语义 | 宿主与 C28x 提升规则不同, 靠"不改变表达式类型"的纪律保证, 非测试保证 |
| Flash 烧写与上电启动 | 需仿真器 (XDS100/510) |
| 运行期数据竞争 | 需硬件在环 |
| 原工程 `39 Warnings` 的逐条对照 | 原构建日志只记条数, 未记正文 |

本工程提供的是**编译输入等价 + 生成产物等价 + 改写模块行为等价**的证据链;
上述硬件相关项必须在目标板上复测, 不能由本证据链替代。

## 8. 复现命令

```bash
# 逐模块两层比对 (反汇编 + 目标文件内容级)
python tools/build_and_compare.py

# 构建新工程 (按 jt380.pjt 的真实选项), 产出 .out/.map/.hex
python tools/build_official.py

# 构建原工程副本 (bpqym 只读复制), 产出 .out/.map/.hex
python tools/build_baseline.py

# 改写模块: 先跑负向对照确认测试台有效, 再跑差分测试与目标端检查
python tools/difftest/negative_control.py
python tools/difftest/run_case.py zeroPosCtl 1000
python tools/difftest/run_case.py f_p2p 1000
python tools/difftest/run_case.py f_common 100
python tools/difftest/run_case.py f_motorFc 20000
python tools/difftest/run_case.py f_invPara 200
python tools/difftest/run_case.py f_plc 400000
python tools/difftest/run_case.py f_modbus 300000
python tools/difftest/run_case.py f_torqueCtrl 20000
python tools/difftest/run_case.py f_vf 20000
python tools/difftest/run_case.py f_frqSrc 20000
python tools/difftest/run_case.py f_runSrc 20000
python tools/difftest/run_case.py f_frqSrc_pid 20000
python tools/difftest/run_case.py f_canlink 20000
python tools/difftest/run_case.py f_profibus 20000
python tools/difftest/run_case.py MotorDataExchange 20000
python tools/difftest/run_case.py f_fcDeal 20000
python tools/difftest/run_case.py f_error 20000
python tools/difftest/run_case.py f_menu 20000
python tools/difftest/run_case.py DSP2803x_CpuTimers 20000
python tools/difftest/run_case.py MotorInfoCollect 20000
python tools/difftest/run_case.py MotorParEst 20000
python tools/difftest/run_case.py f_dspcan 20000
python tools/difftest/run_case.py MotorPublicCal 200
python tools/difftest/run_case.py MotorSpeedCheck 20000
python tools/difftest/run_case.py MotorCarrier 20000
python tools/difftest/run_case.py f_ui 20000
python tools/difftest/run_case.py f_main 20000
python tools/difftest/run_case.py f_interface 20000
python tools/difftest/run_case.py f_eeprom 20000
python tools/difftest/run_case.py MotorEncoder 20000
python tools/difftest/run_case.py MotorCurrentTransform 5
python tools/difftest/run_case.py MotorDebug 200000
python tools/difftest/run_case.py SubPrg 200000
python tools/difftest/run_case.py MotorInvProtect 20000
python tools/difftest/run_case.py MotorMain 20000
python tools/difftest/run_case.py main 200000
python tools/difftest/run_case.py mainInit_2803x 20000
python tools/difftest/run_case.py MotorSVC 50
python tools/difftest/run_case.py MotorPWM 200000
python tools/difftest/run_case.py f_debug 20000
python tools/difftest/run_case.py f_comm_DP 20000
python tools/difftest/run_case.py MotorPmsmMain 20000
python tools/difftest/run_case.py MotorPmsmParEst 20000
python tools/difftest/run_case.py MotorParChange 20000
python tools/difftest/run_case.py f_runSrc_accDecFrq 20000
python tools/difftest/run_case.py f_osc 20000
python tools/difftest/run_case.py MotorImParEst 20000
python tools/difftest/run_case.py MotorVCMain 200000
python tools/difftest/run_case.py MotorVF 20000
python tools/difftest/run_case.py f_comm 20000
python tools/difftest/run_case.py f_io 20000
python tools/difftest/target_check.py src/motor/ZeroPosCtl.c
python tools/difftest/target_check.py src/function/f_p2p.c
python tools/difftest/target_check.py src/function/f_common.c
python tools/difftest/target_check.py src/function/f_motorFc.c
python tools/difftest/target_check.py src/function/f_invPara.c
python tools/difftest/target_check.py src/function/f_plc.c
python tools/difftest/target_check.py src/function/f_modbus.c
python tools/difftest/target_check.py src/function/f_torqueCtrl.c
python tools/difftest/target_check.py src/function/f_vf.c
python tools/difftest/target_check.py src/function/f_frqSrc.c
python tools/difftest/target_check.py src/function/f_runSrc.c
python tools/difftest/target_check.py src/function/f_frqSrc_pid.c
python tools/difftest/target_check.py src/function/f_canlink.c
python tools/difftest/target_check.py src/function/f_profibus.c
python tools/difftest/target_check.py src/motor/MotorDataExchange.c
python tools/difftest/target_check.py src/function/f_fcDeal.c
python tools/difftest/target_check.py src/function/f_error.c
python tools/difftest/target_check.py src/function/f_menu.c
python tools/difftest/target_check.py src/system/DSP2803x_CpuTimers.c
python tools/difftest/target_check.py src/motor/MotorInfoCollect.c
python tools/difftest/target_check.py src/motor/MotorParEst.c
python tools/difftest/target_check.py src/function/f_dspcan.c
python tools/difftest/target_check.py src/motor/MotorPublicCal.c
python tools/difftest/target_check.py src/motor/MotorSpeedCheck.c
python tools/difftest/target_check.py src/motor/MotorCarrier.c
python tools/difftest/target_check.py src/function/f_ui.c
python tools/difftest/target_check.py src/function/f_main.c
python tools/difftest/target_check.py src/function/f_interface.c
python tools/difftest/target_check.py src/function/f_eeprom.c
python tools/difftest/target_check.py src/motor/MotorEncoder.c
python tools/difftest/target_check.py src/motor/MotorCurrentTransform.c
python tools/difftest/target_check.py src/motor/MotorDebug.c
python tools/difftest/target_check.py src/subprogram/SubPrg.c
python tools/difftest/target_check.py src/motor/MotorInvProtect.c
python tools/difftest/target_check.py src/motor/MotorMain.c
python tools/difftest/target_check.py src/system/main.c
python tools/difftest/target_check.py src/system/mainInit_2803x.c
python tools/difftest/target_check.py src/motor/MotorSVC.c
python tools/difftest/target_check.py src/motor/MotorPWM.c
python tools/difftest/target_check.py src/function/f_debug.c
python tools/difftest/target_check.py src/function/f_comm_DP.c
python tools/difftest/target_check.py src/motor/MotorPmsmMain.c
python tools/difftest/target_check.py src/motor/MotorPmsmParEst.c
python tools/difftest/target_check.py src/motor/MotorParChange.c
python tools/difftest/target_check.py src/function/f_runSrc_accDecFrq.c
python tools/difftest/target_check.py src/function/f_osc.c
python tools/difftest/target_check.py src/motor/MotorImParEst.c
python tools/difftest/target_check.py src/motor/MotorVCMain.c
python tools/difftest/target_check.py src/motor/MotorVF.c
python tools/difftest/target_check.py src/function/f_comm.c
python tools/difftest/target_check.py src/function/f_io.c
python tools/difftest/target_layout.py

# 静态验收 + 全量测试
python tools/verify_tree.py
python tests/run_all.py

# 汇总本报告
python tools/make_build_report.py
```

