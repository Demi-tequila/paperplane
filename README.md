# 纸飞机 PaperPlane

> **把打标签这件事，折成一次点击。**
> 一个约 120 KB 的驱动，五类指令集，双击一次装完；之后在打印对话框里选个名字，
> 剩下的纸型、间隙、浓度、方向、极性全部自动跟上。

[![CI](https://github.com/Demi-tequila/paperplane/actions/workflows/ci.yml/badge.svg)](https://github.com/Demi-tequila/paperplane/actions)
[![Release](https://img.shields.io/badge/release-0.5.1-blue)](https://github.com/Demi-tequila/paperplane/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![macOS 12+](https://img.shields.io/badge/macOS-12%2B-lightgrey)]()

---

## 为什么值得装

**装一次，以后不用再管。**
标签尺寸、间隙、浓度、走纸方向、位图极性全部存在 Mac 上，随每次作业下发给驱动。
打印机拔电、重启、换卷纸、换 USB 口，都不用再长按 FEED 重新测纸。

**不用装厂商那套东西。**
不需要 Python、不需要 Xcode、不需要后台常驻服务，也不联网。
解压、双击、输一次密码，结束。

**打不成就直说，不硬猜。**
遇到不认识的机型，它会告诉你「我不确定这是哪种指令集」，而不是随便按 TSPL 打出一张废纸。
作业超过机器幅宽时，它按原尺寸告警，而不是偷偷缩放到看不出问题。

> 适合：电商面单、快递单、条码标签、货架标签、58/80mm 小票。
> 不适合：精臣、喵喵机、DYMO、Brother QL 这类走私有协议的机器 —— 装的时候会直接说明。

## 它解决什么问题

macOS 对国产热敏标签机（得力、汉印、佳博、芯烨、启锐…）支持很差：官方驱动常年不更新、
装完还要在打印对话框里手动选纸型、每次换纸都要长按 FEED 重新测纸、打出来要么倒置要么
黑底白字。而 WPS / Excel / 电商后台导出的 PDF 本身没有错，错在中间那层驱动。

纸飞机做的事只有一件：**把「纸型 + 间隙 + 浓度 + 方向 + 极性」这些参数从打印机里搬到 Mac 上**，
在每次打印时由 PPD 默认项随作业下发给驱动，驱动再把页面渲染成设备真正能读懂的指令流。

打印机掉电、重启、换卷纸，配置都不会丢。

## 三个硬规则

1. **不猜。** 指令集只认设备自报（USB `device-id` 的 `CMD` 字段 / IPP `printer-device-id`）；
   设备不报才查机型库，且库里每一条都必须写出处；两者都没有就**停止自动绑定**，
   报「未知机型」让你决定，绝不静默默认成 TSPL。
2. **不静默改尺寸。** 作业超出设备幅宽时按原尺寸输出并告警，不做缩放。
3. **不擅自出纸。** 安装、自检、测试全程零出纸；要出纸只能你自己按打印。

## 装什么、不装什么

| | 纸飞机 0.5.0 |
|---|---|
| Python / Node / 编译器 | **不需要** |
| 后台常驻进程 / LaunchAgent | **不需要** |
| 联网 / 遥测 | **不联网**，无遥测 |
| 安装体积 | 1 个 116 KB 通用二进制 + 4 个 PPD（arm64 + x86_64） |
| 系统改动 | `/Library/Printers/PaperPlane/`（驱动）、`/Library/Printers/PPDs/.../PaperPlane/`（PPD） |
| 需要密码 | 一次（macOS 26 只允许 root 拥有的 filter 被 cupsd 执行） |

## 安装

```bash
git clone https://github.com/Demi-tequila/paperplane.git
cd paperplane
bash install.sh
```

或者直接双击 **`安装.command`**（会自己弹密码框，不用开终端）。

装完在打印对话框里会看到 `PaperPlane_<你的机型>`：

- 标签 / 面单 / 小票 → 选 `PaperPlane_` 开头那台
- A4 / 照片 / 信封 → 照旧选你自己的 Epson / HP / Canon

换一台新打印机：插上 USB，重新跑一次 `bash install.sh --bind`，会自动认出型号。

## 支持的指令集

| 族 | 指令集 | 典型机型 | 处理方式 |
|---|---|---|---|
| `tspl_label` | TSPL / TSPL2 | 得力、汉印、佳博、芯烨、启锐、快麦、立象、博思得、TSC、北洋 | **纸飞机接管**（macOS 没有这类驱动，这是本项目的核心价值；得力 DL-888D PRO 为真机实测） |
| `system_native_label` | ZPL / EPL / CPCL | Zebra、Honeywell、SATO、Godex、Citizen | **让路给 macOS**：系统自带 `rastertolabel`（添加打印机里的「Zebra ZPL Label Printer」），装两套只会互相干扰 |
| `escpos_receipt` | ESC/POS | Epson TM、Star、芯烨 58/80、佳博、商米、映美 | **纸飞机接管**（系统没有热敏小票驱动） |
| `escpos_label` | ESC/POS 位图 | 无已确认机型，仅供 `--bind-family` 手动指定 | 手动 |
| `sheet_native` | 系统原生 | HP / Canon / Epson / Brother 普通纸机 | 交给 macOS 驱动，纸飞机不接管 |

明确的**不支持**清单（私有协议，无法生成设备能读懂的指令）：精臣/niimbot、喵喵机、
Phomemo 便携机、Brother QL、DYMO LabelWriter。装的时候会直接告诉你原因，不会假装支持。
（DYMO 与 Zebra 一样属于系统自带驱动覆盖的范围，纸飞机不重复提供。）

机型库在 [`data/models.tsv`](data/models.tsv)，加一行就能支持新机型，欢迎 PR。

## 验证方式（诚实版）

`tests/harness.py` 会为**每一条机型规则**在本机建一个真实 CUPS 队列，后端指向 127.0.0.1
的字节记录器，用 `lp` 提交真实作业，走完整链路：

```
lp → cupsd → cgpdftoraster → paperplane → socket 后端 → 抓回打印机真正会收到的字节
```

然后按该指令集的语法逐条校验（SIZE 尺寸误差、BITMAP 行长×行高、ZPL 的 ^XA/^GFA/^XZ
闭合与 HEX 长度、ESC/POS 的 GS v 0 分带是否覆盖整页等）。

**它证明什么**：指令能生成、语法正确、尺寸 1:1、参数随队列走、未知机型会被拒绝。
**它不证明什么**：墨粉落到纸上。纸张端是回环记录器，不是实体打印机；
机型覆盖按指令集族收敛，不等于逐台真机认证。完整分级见
[`Diagnostics/coverage.md`](Diagnostics/coverage.md)。

跑测试：

```bash
python3 tests/harness.py          # 需要先 bash install.sh 装好驱动
```

## 常用命令

```bash
# 不出纸预演：把 PDF 渲染成 PNG，先看效果再决定要不要打
/Library/Printers/PaperPlane/paperplane preview --in 面单.pdf --out 预览.png

# 转成指令流存文件（排查用，不发送到打印机）
/Library/Printers/PaperPlane/paperplane convert --in 面单.pdf --out out.tspl --pdl tspl

# 只看某台机器会被判定成什么族
bash lib/resolve.sh "Deli DL-888D PRO" ""

# 卸载（只删纸飞机自己建的东西）
bash uninstall.sh
```

## 自己编译

```bash
bash scripts/make.sh          # 生成 PPD + 编译 arm64/x86_64 通用二进制
```

只依赖系统 `libcups`，没有第三方库。

## 许可证

MIT。见 [LICENSE](LICENSE)。
