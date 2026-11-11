# 纸飞机 PaperPlane 支持的打印机名单

> 自动从 `data/models.tsv` 生成，机器库名单与此文档永远一致。
> 判定优先级：设备自报指令集（device-id 的 CMD 字段）> 下表 > 未知则拒绝。

共 **65 条规则**。同品牌同系列的不同尺寸/dpi 变体共用一条规则。

## 纸飞机接管 · TSPL 标签机（核心支持，macOS 没有这类驱动）

| 品牌 / 系列 | 典型型号 | dpi | 幅宽 | 说明 |
|---|---|---|---|---|
| Deli | `Deli DL-888D PRO` | 300 | 100 mm | 得力 DL-888D PRO（本机唯一真机实测机型） |
| Deli | `Deli DL-770D` | 203 | 104 mm | 得力 DL-770D/770E 系列 |
| Deli | `Deli DL-730C` | 203 | 104 mm | 得力 DL-730C |
| Deli | `Deli DL-886A` | 203 | 104 mm | 得力 DL-886 系列 |
| Deli | `Deli DL-820T` | 203 | 108 mm | 得力 DL-820T |
| HPRT | `HPRT SL42` | 203 | 108 mm | 汉印 N41/N31/D45/SL42/CP4000 |
| HPRT | `HPRT MT800` | 203 | 108 mm | 汉印便携 MT800/A300 系列 |
| Gprinter | `Gprinter GP-1324D` | 203 | 108 mm | 佳博 GP-1324D/1924D/1524T |
| Gprinter | `Gprinter GP-3120TU` | 203 | 108 mm | 佳博 GP-3120TU/1324D |
| Xprinter | `Xprinter XP-365B` | 203 | 108 mm | 芯烨 XP-365B/370B/460B/470B |
| Xprinter | `Xprinter XP-TT426B` | 203 | 108 mm | 芯烨 XP-TT426B/TT437B |
| Qirui | `Qirui QR-386A` | 203 | 104 mm | 启锐 QR-386A/488/668 |
| Kuaimai | `Kuaimai KM-118` | 203 | 104 mm | 快麦 KM-118/202/241 |
| Argox | `Argox OS-214plus` | 203 | 104 mm | 立象 OS-214plus/CP-2140 |
| Postek | `Postek C168` | 203 | 108 mm | 博思得 C168/G-2108/Q8 |
| TSC | `TSC TTP-244 Pro` | 203 | 108 mm | TSC 台产的 TTP/TE/DA 系列（TSPL-EZ 原生） |
| BTP-2100E | `BTP-2100E` | 203 | 104 mm | 北洋 BTP-2100E/2300E |
| Zijiang | `Zijiang ZJ-9200` | 203 | 104 mm | 资江 ZJ-9200/9210 |
| Zonerich | `Zonerich AB-320M` | 203 | 104 mm | 中崎 AB-320M/340M |
| HPRT | `HPRT TP585` | 203 | 80 mm | 汉印 TP585 标签模式 |

## 纸飞机接管 · ESC/POS 小票机（macOS 没有热敏小票驱动）

| 品牌 / 系列 | 典型型号 | dpi | 幅宽 | 说明 |
|---|---|---|---|---|
| EPSON | `EPSON TM-T82III` | 203 | 80 mm | Epson TM-T82III/TM-T100/TM-m30 |
| Star | `Star TSP800II` | 203 | 80 mm | Star TSP100/TSP800II |
| Xprinter | `Xprinter XP-58IIH` | 203 | 80 mm | 芯烨 XP-58IIH/XP-80C/N160II |
| Gprinter | `Gprinter GP-58MBIII` | 203 | 80 mm | 佳博 GP-58MBIII/U80300I |
| Zonerich | `Zonerich AB-58MK` | 203 | 80 mm | 中崎 AB-58MK/AB-80MK |
| Sunmi | `Sunmi TP585` | 203 | 80 mm | 商米内置打印 |
| Deli | `Deli DL-801P` | 203 | 80 mm | 得力 DL-801P/802P |
| Jolimark | `Jolimark RP80` | 203 | 80 mm | 映美 58/80mm 小票机 |

## 让路 macOS · ZPL/EPL/CPCL 标签机（系统自带 rastertolabel，不装两套）

| 品牌 / 系列 | 典型型号 | dpi | 幅宽 | 说明 |
|---|---|---|---|---|
| Zebra | `Zebra ZD421` | 203 | 104 mm | Zebra ZD230/ZD421/ZT411/GK420d 等 |
| Zebra | `Zebra ZD621` | 300 | 104 mm | Zebra 300dpi 机型 |
| Honeywell | `Honeywell PC42d` | 203 | 104 mm | Honeywell PC42d/PD43/PM42 |
| SATO | `SATO CL4NX` | 203 | 104 mm | SATO CL4NX/CT408i |
| Godex | `Godex G500` | 203 | 104 mm | Godex G500/G530/RT200 |
| Citizen | `Citizen CL-S700` | 203 | 104 mm | Citizen CL-S700/CL-E720 |
| SEUIC | `SEUIC AUTOID Q9` | 203 | 104 mm | 东集 AUTOID Q9 打印模块 |

## 让路 macOS · 普通纸机（系统/AirPrint/厂商驱动，纸飞机只识别不接管）

| 品牌 / 系列 | 典型型号 | dpi | 幅宽 | 说明 |
|---|---|---|---|---|
| HP | `HP LaserJet M404dn` | 300 | 216 mm | HP 激光/喷墨全系 |
| EPSON | `EPSON L3110 Series` | 300 | 216 mm | Epson L 系列/XP/WF |
| Canon | `Canon LBP2900` | 300 | 216 mm | Canon 激光/喷墨全系 |
| Brother | `Brother HL-L2350DW` | 300 | 216 mm | Brother HL/DCP/MFC |
| RICOH | `RICOH IM C3000` | 300 | 216 mm | 企业复合机 |
| Xiaomi | `Xiaomi Mi Printer` | 300 | 216 mm | 小米喷墨打印机 |
| Ricoh | `Ricoh IM C3000` | 600 | 216 mm | 理光 IM C / MP / SP / Aficio 系列复合机与激光机 |
| Ricoh | `Ricoh P 502` | 600 | 216 mm | 理光 P / SG / GX / NX / AP 系列 |
| Ricoh | `Ricoh Aficio MP 4001` | 600 | 216 mm | 理光 GR / IP / 旧 Aficio |
| Canon | `Canon iR-ADV C5560` | 600 | 216 mm | 佳能 imageRUNNER / i-SENSYS / PIXUS 等 |
| EPSON | `EPSON PX-049A` | 600 | 216 mm | 爱普生 PX / LP / EW / WF / L 系列 |
| Brother | `Brother MFC-J6983CDW` | 600 | 216 mm | Brother HL / DCP / MFC / PJ 系列 |
| KYOCERA | `KYOCERA ECOSYS M3655idn` | 600 | 216 mm | 京瓷 ECOSYS / TASKalfa / FS 系列 |
| SHARP | `SHARP MX-3118NC` | 600 | 216 mm | 夏普 MX / BP / AR 系列复合机 |
| Toshiba | `Toshiba e-STUDIO5508A` | 600 | 216 mm | 东芝 e-STUDIO 系列 |
| FUJIFILM | `FUJIFILM Apeos C5570` | 600 | 216 mm | 富士胶片 Apeos / DocuPrint 系列 |
| KONICA | `KONICA MINOLTA bizhub C368` | 600 | 216 mm | 柯尼卡美能达 bizhub / magicolor 系列 |
| OKI | `OKI MC363dn` | 600 | 216 mm | OKI MC / ES / C / B 系列 |
| NEC | `NEC MultiWriter 5700C` | 600 | 216 mm | NEC MultiWriter / Picty 系列 |
| Panasonic | `Panasonic KX-MB2061` | 600 | 216 mm | 松下 KX / DP / Workio 系列 |
| MX-3118NC | `MX-3118NC` | 600 | 216 mm | 夏普 MX 系列（网络发现只有型号名时） |

## 明确不支持 · 私有协议（会直接说明原因，不假装支持）

| 品牌 / 系列 | 典型型号 | dpi | 幅宽 | 说明 |
|---|---|---|---|---|
| Niimbot | `Niimbot B21` | 0 | 0 mm | 精臣标签机：需厂商 App，纸飞机不支持 |
| Paperang | `Paperang P2` | 0 | 0 mm | 喵喵机：需厂商 App，纸飞机不支持 |
| Phomemo | `Phomemo M02` | 0 | 0 mm | Phomemo 便携机：默认不支持 |
| Brother | `Brother QL-800` | 0 | 0 mm | Brother QL 标签机：需专有驱动，纸飞机不支持 |
| DYMO | `DYMO LabelWriter 450` | 0 | 0 mm | DYMO 标签机：需专有驱动，纸飞机不支持 |
| EPSON | `EPSON LW-900` | 0 | 0 mm | 爱普生 LW 标签机：需官方 Label Editor，纸飞机不支持 |
| Brother | `Brother PT-P910BT` | 0 | 0 mm | Brother PT / QL 标签机：需专有驱动，纸飞机不支持 |
| CASIO | `CASIO KL-8800` | 0 | 0 mm | 卡西欧 KL / DT 标签机：需专有工具，纸飞机不支持 |
| Mitsubishi | `Mitsubishi CP-D70DW` | 0 | 0 mm | 三菱 CP 系列照片打印机：需专有驱动，纸飞机不支持 |

## 日本品牌覆盖一览

| 品牌 | 状态 | 备注 |
|---|---|---|
| 理光 Ricoh | ✅ 识别并让路系统 | IM C / IM / MP / SP / P / SG / GX / Aficio 全系列（PCL/PS），另有 GR / IP 旧款 |
| 佳能 Canon | ✅ 识别并让路系统 | imageRUNNER / i-SENSYS / PIXUS / LBP / TS / MG；SELPHY 热升华不在此列 |
| 爱普生 Epson | ✅ 识别并让路系统 / ❌ LW 标签机不支持 | PX / LP / EW / WF / L / XP / ET 走系统；LabelWorks LW 为私有协议 |
| 兄弟 Brother | ✅ 识别并让路系统 / ❌ PT、QL 不支持 | HL / DCP / MFC / PJ 走系统；P-touch、QL 为专有光栅协议 |
| 京瓷 Kyocera | ✅ 识别并让路系统 | ECOSYS / TASKalfa / FS（PCL XL / KPDL） |
| 夏普 Sharp | ✅ 识别并让路系统 | MX / BP / AR；网络发现只报「MX-xxxx」型号名时也能认出 |
| 东芝 Toshiba | ✅ e-STUDIO 识别并让路系统 | B-SX / B-EV 标签机为 TEC 私有协议，遇到会如实报「未知」，不猜 |
| 富士胶片 Fujifilm | ✅ 识别并让路系统 | Apeos / DocuPrint / DocuCentre（原富士施乐） |
| 柯尼卡美能达 | ✅ 识别并让路系统 | bizhub / magicolor / PagePro |
| OKI 冲电气 | ✅ 识别并让路系统 | MC / ES / C / B 系列 |
| NEC | ✅ 识别并让路系统 | MultiWriter / Picty |
| 松下 Panasonic | ✅ 识别并让路系统 | KX / DP / Workio（多为理光 OEM） |
| SATO 佐藤 | ✅ 让路系统 ZPL 驱动 | CL4NX / CT408i 等（SZPL/ZPL） |
| Citizen 西铁城 | ✅ 让路系统 ZPL 驱动 | CL-S / CL-E 系列 |
| Star 星精密 | ✅ 纸飞机接管 | TSP / MCP 小票机（ESC/POS） |
| 卡西欧 Casio | ❌ 不支持 | KL / DT 标签机为私有协议 |
| 三菱电机 | ❌ 不支持 | CP 系列照片打印机为私有热升华协议 |
| 索尼 Sony | — | 已退出打印机市场，无在售机型 |

### 为什么「识别并让路」也算覆盖

理光、佳能、京瓷这类办公复合机是 PCL/PostScript 设备，macOS 自带驱动体系（AirPrint / 通用 PCL / 厂商驱动）已经完整支持。纸飞机对它们的职责是：

1. **认得出** —— 安装时不会把它们误当成标签机建错队列；
2. **让得开** —— 明确告诉你「这台用系统驱动就好」，不装重复的东西；
3. **不越权** —— A4/照片/信封打印质量由专业驱动负责，纸飞机不做二次处理。

如果某台理光/佳能在 macOS 上打不了，那是厂商驱动的事，请装厂商官方 macOS 驱动，不要用纸飞机强行接管 —— 那只会更糟。

