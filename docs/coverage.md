# 覆盖与验证矩阵（自动生成）

生成时间：2026-11-11T10:21:32+0800  
驱动：`/Library/Printers/PaperPlane/paperplane`（0.5.1）

## 一句话结论

本次共验证 **2 条机型规则**、**22 条语法断言**，失败 **0** 条；判定阶梯 **9/9**；CLI 指令方言 **3/3**（TSPL / ZPL / ESC-POS 离线生成校验）。

## 状态汇总
| 状态 | 数量 | 含义 |
|---|---|---|
| `pass` | 2 | 建真实 CUPS 队列，作业走 cupsd→cgpdftoraster→驱动→socket 全链路，字节级语法校验全过 |

## 各族实测数据
| 指令集族 | 建队列 | 通过 | 产出字节 | 页面尺寸 | 位图规格 |
|---|---|---|---|---|---|
| `tspl_label` | 2 | 2 | 174945–174945 B | 99.99×99.99 mm | 1181 行 × 148 字节/行（BITMAP） |

## 判定阶梯（设备自报优先，未知必须拒绝）
| 设备自报串 | 判定 | 依据 | 结果 |
|---|---|---|---|
| `MFG:Deli;MDL:DL-888D PRO;CMD:TSPL;` | `tspl_label` | device-cmd | ✓ |
| `MFG:Zebra;MDL:ZD421;CMD:ZPL,EPL,CPCL;` | `system_native_label` | device-cmd | ✓ |
| `MFG:EPSON;CMD:ESCPL2,BDC,D4;MDL:TM-T82III;` | `escpos_receipt` | device-cmd | ✓ |
| `MFG:HP;CMD:PCL,PCLXL,POSTSCRIPT;MDL:M404dn;` | `sheet_native` | device-cmd | ✓ |
| `MFG:HPRT;MDL:SL42;` | `tspl_label` | model-db | ✓ |
| `MFG:Zebra;MDL:ZT411;` | `system_native_label` | model-db | ✓ |
| `MFG:Acme;MDL:LP-9000;` | `unknown` | none | ✓ |
| `MFG:Acme;MDL:LP-9000;CMD:TSPL;` | `tspl_label` | device-cmd | ✓ |
| `MFG:Niimbot;MDL:B21;` | `unsupported` | model-db | ✓ |

## CLI 指令方言（离线，不出纸）
| 指令集 | 结果 | 说明 |
|---|---|---|
| tspl | ✓ | 174945 字节 |
| zpl | ✓ | 159867 字节 |
| escpos | ✓ | 281163 字节 |

## 每条规则明细
| # | 机型样例 | 族 | 依据 | dpi | 状态 | 断言 |
|---|---|---|---|---|---|---|
| 1 | Deli DL-888D PRO | `tspl_label` | model-db | 300 | pass | 11/11 |
| 2 | Deli DL-770D | `tspl_label` | model-db | 203 | pass | 11/11 |

## 这份报告不证明什么（必读）

1. **不证明墨粉落到纸上。** 纸张端是 127.0.0.1 的回环字节记录器。本报告证明「指令能生成、语法正确、尺寸 1:1、参数随队列下发」，不证明物理走纸、定位、浓度。
2. **不等于逐台真机认证。** 覆盖按指令集族收敛。真机实测只有得力 DL-888D PRO 一台。
3. **ZPL 只做离线生成校验。** 系统设备端由 macOS 自带 rastertolabel 负责（纸飞机 0.5.1 起 ZPL 族让路，不建队列）。
4. **普通纸机 / 日本品牌复合机未纳入队列测试。** 理光、佳能、京瓷等由 macOS 或厂商驱动负责，纸飞机只识别并让路。
5. **macOS 26 起 PPD 机制被标记弃用。** 长期需迁移到 IPP Everywhere / PAE。
