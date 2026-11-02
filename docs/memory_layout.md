# 内存布局 (memory_layout)

## 1. 链接配置来源

- 链接命令: `CCs/Debug.lkf` 第 1 行 (Debug)
- 存储区定义: `public/common/F28035.cmd` (MEMORY + SECTIONS)
- 外设寄存器区: `public/common/DSP28035Peripheral.cmd`
- 参考 map: `CCs/Debug/md380_C.map` (TMS320C2000 Linker PC v5.2.1)

- 输出文件: `./Debug/md380_C.out`
- 入口符号: `code_start` @ `0x003f7ff6`
- 链接器选项含 `-ecode_start`, 即入口为 `code_start` (汇编启动段), 不是默认的 `_c_int00`。

## 2. 存储区 (MEMORY)

| 页 | 名称 | origin | length | 已用 | 空闲 | 属性 |
|---|---|---|---:|---:|---:|---|
| PAGE 0 | `RAMPRG` | 0x00008000 | 0x0080 | 0x001D | 0x0063 | RWIX |
| PAGE 0 | `OTP` | 0x003D7800 | 0x0400 | 0x0000 | 0x0400 | RWIX |
| PAGE 0 | `FLASHA` | 0x003E8000 | 0xFF80 | 0xED8B | 0x11F5 | RWIX |
| PAGE 0 | `CSM_RSVD` | 0x003F7F80 | 0x0076 | 0x0076 | 0x0000 | RWIX |
| PAGE 0 | `BEGIN` | 0x003F7FF6 | 0x0002 | 0x0002 | 0x0000 | RWIX |
| PAGE 0 | `CSM_PWL` | 0x003F7FF8 | 0x0008 | 0x0008 | 0x0000 | RWIX |
| PAGE 0 | `IQTABLES` | 0x003FE000 | 0x0B50 | 0x0000 | 0x0B50 | RWIX |
| PAGE 0 | `IQTABLES2` | 0x003FEB50 | 0x008C | 0x0000 | 0x008C | RWIX |
| PAGE 0 | `IQTABLES3` | 0x003FEBDC | 0x00AA | 0x0000 | 0x00AA | RWIX |
| PAGE 0 | `ROM` | 0x003FF27C | 0x0D44 | 0x0000 | 0x0D44 | RWIX |
| PAGE 0 | `RESET` | 0x003FFFC0 | 0x0002 | 0x0000 | 0x0002 | RWIX |
| PAGE 0 | `VECTORS` | 0x003FFFC2 | 0x003E | 0x0000 | 0x003E | RWIX |
| PAGE 1 | `RAMM0` | 0x00000100 | 0x0300 | 0x0000 | 0x0300 | RWIX |
| PAGE 1 | `RAMM1` | 0x00000400 | 0x0400 | 0x0400 | 0x0000 | RWIX |
| PAGE 1 | `DEV_EMU` | 0x00000880 | 0x0105 | 0x0004 | 0x0101 | RWIX |
| PAGE 1 | `SYS_PWR_CTL` | 0x00000985 | 0x0003 | 0x0003 | 0x0000 | RWIX |
| PAGE 1 | `FLASH_REGS` | 0x00000A80 | 0x0060 | 0x0008 | 0x0058 | RWIX |
| PAGE 1 | `CSM` | 0x00000AE0 | 0x0010 | 0x0010 | 0x0000 | RWIX |
| PAGE 1 | `ADC_RESULT` | 0x00000B00 | 0x0020 | 0x0020 | 0x0000 | RWIX |
| PAGE 1 | `CPU_TIMER0` | 0x00000C00 | 0x0008 | 0x0008 | 0x0000 | RWIX |
| PAGE 1 | `CPU_TIMER1` | 0x00000C08 | 0x0008 | 0x0008 | 0x0000 | RWIX |
| PAGE 1 | `CPU_TIMER2` | 0x00000C10 | 0x0008 | 0x0008 | 0x0000 | RWIX |
| PAGE 1 | `PIE_CTRL` | 0x00000CE0 | 0x0020 | 0x001A | 0x0006 | RWIX |
| PAGE 1 | `PIE_VECT` | 0x00000D00 | 0x0100 | 0x0100 | 0x0000 | RWIX |
| PAGE 1 | `CLA1` | 0x00001400 | 0x0080 | 0x0040 | 0x0040 | RWIX |
| PAGE 1 | `ECANA` | 0x00006000 | 0x0040 | 0x0034 | 0x000C | RWIX |
| PAGE 1 | `ECANA_LAM` | 0x00006040 | 0x0040 | 0x0040 | 0x0000 | RWIX |
| PAGE 1 | `ECANA_MOTS` | 0x00006080 | 0x0040 | 0x0040 | 0x0000 | RWIX |
| PAGE 1 | `ECANA_MOTO` | 0x000060C0 | 0x0040 | 0x0040 | 0x0000 | RWIX |
| PAGE 1 | `ECANA_MBOX` | 0x00006100 | 0x0100 | 0x0100 | 0x0000 | RWIX |
| PAGE 1 | `COMP1` | 0x00006400 | 0x0020 | 0x0011 | 0x000F | RWIX |
| PAGE 1 | `COMP2` | 0x00006420 | 0x0020 | 0x0011 | 0x000F | RWIX |
| PAGE 1 | `COMP3` | 0x00006440 | 0x0020 | 0x0011 | 0x000F | RWIX |
| PAGE 1 | `EPWM1` | 0x00006800 | 0x0040 | 0x003A | 0x0006 | RWIX |
| PAGE 1 | `EPWM2` | 0x00006840 | 0x0040 | 0x003A | 0x0006 | RWIX |
| PAGE 1 | `EPWM3` | 0x00006880 | 0x0040 | 0x003A | 0x0006 | RWIX |
| PAGE 1 | `EPWM4` | 0x000068C0 | 0x0040 | 0x003A | 0x0006 | RWIX |
| PAGE 1 | `EPWM5` | 0x00006900 | 0x0040 | 0x003A | 0x0006 | RWIX |
| PAGE 1 | `EPWM6` | 0x00006940 | 0x0040 | 0x003A | 0x0006 | RWIX |
| PAGE 1 | `EPWM7` | 0x00006980 | 0x0040 | 0x003A | 0x0006 | RWIX |
| PAGE 1 | `ECAP1` | 0x00006A00 | 0x0020 | 0x0020 | 0x0000 | RWIX |
| PAGE 1 | `EQEP1` | 0x00006B00 | 0x0040 | 0x0040 | 0x0000 | RWIX |
| PAGE 1 | `LINA` | 0x00006C00 | 0x0080 | 0x004A | 0x0036 | RWIX |
| PAGE 1 | `GPIOCTRL` | 0x00006F80 | 0x0040 | 0x0040 | 0x0000 | RWIX |
| PAGE 1 | `GPIODAT` | 0x00006FC0 | 0x0020 | 0x0020 | 0x0000 | RWIX |
| PAGE 1 | `GPIOINT` | 0x00006FE0 | 0x0020 | 0x000C | 0x0014 | RWIX |
| PAGE 1 | `SYSTEM` | 0x00007010 | 0x0020 | 0x0020 | 0x0000 | RWIX |
| PAGE 1 | `SPIA` | 0x00007040 | 0x0010 | 0x0010 | 0x0000 | RWIX |
| PAGE 1 | `SCIA` | 0x00007050 | 0x0010 | 0x0010 | 0x0000 | RWIX |
| PAGE 1 | `NMIINTRUPT` | 0x00007060 | 0x0010 | 0x0010 | 0x0000 | RWIX |
| PAGE 1 | `XINTRUPT` | 0x00007070 | 0x0010 | 0x0010 | 0x0000 | RWIX |
| PAGE 1 | `ADC` | 0x00007100 | 0x0080 | 0x0050 | 0x0030 | RWIX |
| PAGE 1 | `SPIB` | 0x00007740 | 0x0010 | 0x0010 | 0x0000 | RWIX |
| PAGE 1 | `I2CA` | 0x00007900 | 0x0040 | 0x0022 | 0x001E | RWIX |
| PAGE 1 | `RAML0` | 0x00008080 | 0x1F80 | 0x1B34 | 0x044C | RWIX |
| PAGE 1 | `PARTID` | 0x003D7E80 | 0x0001 | 0x0001 | 0x0000 | RWIX |
| PAGE 1 | `CSM_PWL` | 0x003F7FF8 | 0x0008 | 0x0008 | 0x0000 | RWIX |

## 3. 段分配 (SECTION ALLOCATION, 顶层)

| 段 | 页 | origin | length | 说明 |
|---|---|---|---:|---|
| `.pinit` | 0 | 0x003e8000 | 0x00000000 | UNINITIALIZED |
| `.text` | 0 | 0x003e8000 | 0x0000c8c4 |  |
| `.econst` | 0 | 0x003f48c4 | 0x000022cc |  |
| `ramfuncs` | 0 | 0x003f6b90 | 0x0000001d | RUN ADDR = 00008000 |
| `.cinit` | 0 | 0x003f6bad | 0x000001d4 |  |
| `.ovly` | 0 | 0x003f6d84 | 0x0000000a |  |
| `csm_rsvd` | 0 | 0x003f7f80 | 0x00000076 |  |
| `.reset` | 0 | 0x003fffc0 | 0x00000002 | DSECT |
| `vectors` | 0 | 0x003fffc2 | 0x00000000 | DSECT |
| `.stack` | 1 | 0x00000400 | 0x00000400 | UNINITIALIZED |
| `.ebss` | 1 | 0x00008080 | 0x0000174c | UNINITIALIZED |

## 4. 关键约束 (重写必须保持)

- 入口 `codestart` 段固定在 `BEGIN = 0x3F7FF6`, 长度 2 个字。
- `ramfuncs` 段 `LOAD = FLASHA`, `RUN = RAMPRG (0x8000)`, 由 `_prginRAM` 搬运表复制。
- `.text` / `.cinit` / `.pinit` / `.binit` / `.ovly` 均在 `FLASHA (0x3E8000)`。
- 外设寄存器区由 `DSP28035Peripheral.cmd` 固定映射 (PAGE 1)。
- `DSP2803x_GlobalVariableDefs.c` 里的 `#pragma DATA_SECTION` 决定了每个外设结构的落点。
- 新工程链接脚本逐字沿用原 `.cmd` (仅去除注释), 存储区与段映射未变。
