#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 data/models.tsv 生成用户可读的「支持的打印机名单」（docs/supported-printers.md）。"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TITLES = {
    "tspl_label": "纸飞机接管 · TSPL 标签机（核心支持，macOS 没有这类驱动）",
    "escpos_receipt": "纸飞机接管 · ESC/POS 小票机（macOS 没有热敏小票驱动）",
    "escpos_label": "纸飞机接管 · ESC/POS 标签模式（无已确认机型，仅手动指定）",
    "system_native_label": "让路 macOS · ZPL/EPL/CPCL 标签机（系统自带 rastertolabel，不装两套）",
    "sheet_native": "让路 macOS · 普通纸机（系统/AirPrint/厂商驱动，纸飞机只识别不接管）",
    "unsupported": "明确不支持 · 私有协议（会直接说明原因，不假装支持）",
}
ORDER = ["tspl_label", "escpos_receipt", "escpos_label",
         "system_native_label", "sheet_native", "unsupported"]


def main():
    rules = []
    for ln, line in enumerate((ROOT / "data" / "models.tsv")
                              .read_text(encoding="utf-8-sig").splitlines(), 1):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        p = s.split("\t")
        if len(p) != 9:
            print("models.tsv 第 %d 行字段数 %d，跳过" % (ln, len(p)), file=sys.stderr)
            continue
        rules.append(p)

    out = ["# 纸飞机 PaperPlane 支持的打印机名单",
           "",
           "> 自动从 `data/models.tsv` 生成，机器库名单与此文档永远一致。",
           "> 判定优先级：设备自报指令集（device-id 的 CMD 字段）> 下表 > 未知则拒绝。",
           ""]
    total = len(rules)
    out.append("共 **%d 条规则**。同品牌同系列的不同尺寸/dpi 变体共用一条规则。\n" % total)

    for fam in ORDER:
        rows = [r for r in rules if r[0] == fam]
        if not rows:
            continue
        out.append("## %s\n" % TITLES[fam])
        out.append("| 品牌 / 系列 | 典型型号 | dpi | 幅宽 | 说明 |")
        out.append("|---|---|---|---|---|")
        for fam_, rx, dpi, w, cut, ev, src, note, sample in rows:
            brand = sample.split()[0] if sample.split() else note
            out.append("| %s | `%s` | %s | %s mm | %s |" % (brand, sample, dpi, w, note))
        out.append("")

    out.append("## 日本品牌覆盖一览\n")
    out.append("| 品牌 | 状态 | 备注 |")
    out.append("|---|---|---|")
    jp = [
        ("理光 Ricoh", "✅ 识别并让路系统", "IM C / IM / MP / SP / P / SG / GX / Aficio 全系列（PCL/PS），另有 GR / IP 旧款"),
        ("佳能 Canon", "✅ 识别并让路系统", "imageRUNNER / i-SENSYS / PIXUS / LBP / TS / MG；SELPHY 热升华不在此列"),
        ("爱普生 Epson", "✅ 识别并让路系统 / ❌ LW 标签机不支持", "PX / LP / EW / WF / L / XP / ET 走系统；LabelWorks LW 为私有协议"),
        ("兄弟 Brother", "✅ 识别并让路系统 / ❌ PT、QL 不支持", "HL / DCP / MFC / PJ 走系统；P-touch、QL 为专有光栅协议"),
        ("京瓷 Kyocera", "✅ 识别并让路系统", "ECOSYS / TASKalfa / FS（PCL XL / KPDL）"),
        ("夏普 Sharp", "✅ 识别并让路系统", "MX / BP / AR；网络发现只报「MX-xxxx」型号名时也能认出"),
        ("东芝 Toshiba", "✅ e-STUDIO 识别并让路系统", "B-SX / B-EV 标签机为 TEC 私有协议，遇到会如实报「未知」，不猜"),
        ("富士胶片 Fujifilm", "✅ 识别并让路系统", "Apeos / DocuPrint / DocuCentre（原富士施乐）"),
        ("柯尼卡美能达", "✅ 识别并让路系统", "bizhub / magicolor / PagePro"),
        ("OKI 冲电气", "✅ 识别并让路系统", "MC / ES / C / B 系列"),
        ("NEC", "✅ 识别并让路系统", "MultiWriter / Picty"),
        ("松下 Panasonic", "✅ 识别并让路系统", "KX / DP / Workio（多为理光 OEM）"),
        ("SATO 佐藤", "✅ 让路系统 ZPL 驱动", "CL4NX / CT408i 等（SZPL/ZPL）"),
        ("Citizen 西铁城", "✅ 让路系统 ZPL 驱动", "CL-S / CL-E 系列"),
        ("Star 星精密", "✅ 纸飞机接管", "TSP / MCP 小票机（ESC/POS）"),
        ("卡西欧 Casio", "❌ 不支持", "KL / DT 标签机为私有协议"),
        ("三菱电机", "❌ 不支持", "CP 系列照片打印机为私有热升华协议"),
        ("索尼 Sony", "—", "已退出打印机市场，无在售机型"),
    ]
    for b, st, note in jp:
        out.append("| %s | %s | %s |" % (b, st, note))
    out.append("")
    out.append("### 为什么「识别并让路」也算覆盖\n")
    out.append("理光、佳能、京瓷这类办公复合机是 PCL/PostScript 设备，macOS 自带驱动体系"
               "（AirPrint / 通用 PCL / 厂商驱动）已经完整支持。纸飞机对它们的职责是：\n")
    out.append("1. **认得出** —— 安装时不会把它们误当成标签机建错队列；")
    out.append("2. **让得开** —— 明确告诉你「这台用系统驱动就好」，不装重复的东西；")
    out.append("3. **不越权** —— A4/照片/信封打印质量由专业驱动负责，纸飞机不做二次处理。\n")
    out.append("如果某台理光/佳能在 macOS 上打不了，那是厂商驱动的事，请装厂商官方 macOS 驱动，"
               "不要用纸飞机强行接管 —— 那只会更糟。")
    out.append("")

    (ROOT / "docs" / "supported-printers.md").write_text("\n".join(out) + "\n", encoding="utf-8")
    print("已生成 docs/supported-printers.md（%d 条规则）" % total)


if __name__ == "__main__":
    sys.exit(main())
