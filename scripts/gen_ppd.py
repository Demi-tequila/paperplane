#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成各指令集族的 PPD（构建期工具，终端用户运行期不需要 Python）。

用法：python3 scripts/gen_ppd.py [--filter /Library/Printers/PaperPlane/paperplane]
      [--out drivers]

设计要点：
  · 一个族一个 PPD，纸型全部来自 data/formats.json（单一事实来源）；
  · 每族把 dpi / 幅宽 / 介质 / 极性 / 旋转 写进 PPD 默认项，
    这样 CUPS 每次作业都会把它们作为 options 传给 filter，
    打印机掉电、重启、换卷纸都不会丢配置；
  · PPD 不写死任何具体型号——型号差异由 data/models.tsv 的族归属决定。
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FORMATS = ROOT / "data" / "formats.json"

# 族定义：纸型筛选 + 默认参数
FAMILIES = [
    {
        "leaf": "tspl_label.ppd",
        "pcfile": "PBTSPL.PPD",
        "manufacturer": "PaperPlane",
        "model": "PaperPlane TSPL Label",
        "nick": "纸飞机 TSPL 标签机（国产主流，300 dpi）",
        "short": "PaperPlane TSPL",
        "targets": ("label", "roll"),
        "max_width": 100.0,
        "dpi": 300,
        "pdl": "tspl",
        "media": "gap",
        "gap": 2.0,
        "density": 8,
        "speed": 4,
        "direction": 1,
        "rotate180": 1,
        "invert": 1,
        "tear_off": 1,
        "cut": 0,
        "default_paper": "w283h283",
    },
    {
        "leaf": "escpos_receipt.ppd",
        "pcfile": "PBESP.PPD",
        "manufacturer": "PaperPlane",
        "model": "PaperPlane ESC/POS Receipt",
        "nick": "纸飞机 ESC/POS 小票机（58/80 mm，203 dpi）",
        "short": "PaperPlane ESC/POS",
        "targets": ("roll",),
        "max_width": 80.0,
        "dpi": 203,
        "pdl": "escpos",
        "media": "continuous",
        "gap": 0.0,
        "density": 8,
        "speed": 4,
        "direction": 0,
        "rotate180": 0,
        "invert": 0,
        "tear_off": 1,
        "cut": 1,
        "default_paper": "w227h567",
    },
    {
        "leaf": "escpos_label.ppd",
        "pcfile": "PBESPL.PPD",
        "manufacturer": "PaperPlane",
        "model": "PaperPlane ESC/POS Label",
        "nick": "纸飞机 ESC/POS 标签机（标签模式，203 dpi）",
        "short": "PaperPlane ESC/POS Label",
        "targets": ("label",),
        "max_width": 80.0,
        "dpi": 203,
        "pdl": "escpos",
        "media": "gap",
        "gap": 2.0,
        "density": 8,
        "speed": 4,
        "direction": 0,
        "rotate180": 0,
        "invert": 0,
        "tear_off": 1,
        "cut": 0,
        "default_paper": "w227h142",
    },
]


def ppd_page_name(fmt):
    """与 formats.json 的 ppd_name 对齐，重名时加序号（CUPS 要求 PageSize 唯一）。"""
    return fmt["ppd_name"]


def build(fam, formats, filter_path, version):
    papers = [f for f in formats["formats"]
              if f["target"] in fam["targets"] and f["width_mm"] <= fam["max_width"] + 0.01]
    # 按宽度、高度排序，面板里更好找
    papers.sort(key=lambda f: (f["width_mm"], f["height_mm"]))

    seen = {}
    lines = []
    w = lines.append
    w('*PPD-Adobe: "4.3"')
    w('*FormatVersion: "4.3"')
    w('*FileVersion: "%s"' % version)
    w('*PCFileName: "%s"' % fam["pcfile"])
    w('*LanguageEncoding: ISOLatin1')
    w('*LanguageVersion: English')
    w('*Manufacturer: "%s"' % fam["manufacturer"])
    w('*ModelName: "%s"' % fam["model"])
    w('*NickName: "%s"' % fam["nick"])
    w('*ShortNickName: "%s"' % fam["short"])
    w('*Product: "(PaperPlane %s)"' % fam["pdl"])
    w('*PSVersion: "(3010.000) 0"')
    w('*LanguageLevel: "3"')
    w('*ColorDevice: False')
    w('*DefaultColorSpace: Gray')
    w('*FileSystem: False')
    w('*LandscapeOrientation: Plus90')
    w('*VariablePaperSize: True')
    w('*cupsVersion: 1.4')
    w('*cupsFilter: "application/vnd.cups-raster 0 %s"' % filter_path)
    w('*cupsManualCopies: False')
    w('')
    w('*OpenGroup: General/General')
    # 先统一算出「唯一纸型名」，PageSize / PageRegion / PaperDimension / ImageableArea
    # 四处必须用同一套名字，否则 CUPS 会认为纸型缺失
    entries = []
    counter = {}
    for f in papers:
        base = ppd_page_name(f)
        n = counter.get(base, 0)
        counter[base] = n + 1
        name = base if n == 0 else "%s_%d" % (base, n)
        label = "%s %s" % (f["category"], f["short_name"].split(" ", 1)[-1])
        entries.append((name, label, f))

    w('*OpenUI *PageSize/Media Size: PickOne')
    w('*OrderDependency: 10 AnySetup *PageSize')
    w('*DefaultPageSize: %s' % fam["default_paper"])
    for name, label, f in entries:
        w('*PageSize %s/%s: "<</PageSize[%.4f %.4f]/ImagingBBox null>>setpagedevice"'
          % (name, label, f["width_pt"], f["height_pt"]))
    w('*CloseUI: *PageSize')
    w('')
    w('*OpenUI *PageRegion/Page Region: PickOne')
    w('*OrderDependency: 10 AnySetup *PageRegion')
    w('*DefaultPageRegion: %s' % fam["default_paper"])
    for name, label, f in entries:
        w('*PageRegion %s/%s: "<</PageSize[%.4f %.4f]/ImagingBBox null>>setpagedevice"'
          % (name, label, f["width_pt"], f["height_pt"]))
    w('*CloseUI: *PageRegion')
    w('')
    for name, label, f in entries:
        w('*PaperDimension %s/%s: "%.4f %.4f"'
          % (name, label, f["width_pt"], f["height_pt"]))
        # 标签机是满幅打印，ImageableArea 必须显式写满，
        # 否则 CUPS 会算出 cupsWidth=0、驱动拿不到任何像素（整页空白）
        w('*ImageableArea %s/%s: "0.0000 0.0000 %.4f %.4f"'
          % (name, label, f["width_pt"], f["height_pt"]))
    w('*HWMargins: 0 0 0 0')
    w('*CloseGroup: General')
    w('')
    w('*OpenUI *Resolution/Output Resolution: PickOne')
    w('*OrderDependency: 20 AnySetup *Resolution')
    w('*DefaultResolution: %ddpi' % fam["dpi"])
    for d in (203, 300, 600):
        w('*Resolution %ddpi/%d dpi: "<</HWResolution[%d %d]>>setpagedevice"' % (d, d, d, d))
    w('*CloseUI: *Resolution')
    w('')
    w('*% ── 以下配置存在 Mac 侧 PPD 里，CUPS 每次作业都会传给驱动 ──')
    w('*% 打印机掉电、重启、换纸都不会丢；改这里等于改这台机器以后每次打印的默认值')
    for key, default, label in [
        ("DeliDpi", fam["dpi"], "dots per inch"),
        ("DeliSizeW", fam["max_width"], "物理宽度 mm"),
        ("DeliSizeH", 100, "物理高度 mm"),
        ("DeliGap", fam["gap"], "标签间隙 mm"),
        ("DeliDensity", fam["density"], "浓度 0-15"),
        ("DeliSpeed", fam["speed"], "速度"),
        ("DeliDirection", fam["direction"], "走纸方向"),
        ("DeliRotate180", fam["rotate180"], "旋转 180 度"),
        ("DeliInvert", fam["invert"], "位图极性"),
        ("DeliTearOff", fam["tear_off"], "走纸到撕纸位"),
        ("DeliOffsetY", 0, "纵向微调 dot"),
    ]:
        w('*OpenUI *%s/%s: PickOne' % (key, key))
        w('*OrderDependency: 30 AnySetup *%s' % key)
        w('*Default%s: %s' % (key, default))
        w('*%s %s/%s: ""' % (key, default, label))
        w('*CloseUI: *%s' % key)
    w('')
    w('*OpenUI *PBPdl/指令集: PickOne')
    w('*OrderDependency: 30 AnySetup *PBPdl')
    w('*DefaultPBPdl: %s' % fam["pdl"])
    for p in ("tspl", "zpl", "escpos", "passthrough"):
        w('*PBPdl %s/%s: ""' % (p, p))
    w('*CloseUI: *PBPdl')
    w('')
    w('*OpenUI *PBMediaType/介质类型: PickOne')
    w('*OrderDependency: 30 AnySetup *PBMediaType')
    w('*DefaultPBMediaType: %s' % fam["media"])
    for m in ("gap", "blackmark", "continuous"):
        w('*PBMediaType %s/%s: ""' % (m, m))
    w('*CloseUI: *PBMediaType')
    w('')
    w('*DefaultFont: Courier')
    w('*Font Courier: Standard "(001.001)" Standard ROM')
    w('')
    w('*%% 纸型总数：%d' % len(papers))
    return "\n".join(lines) + "\n", len(papers)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--filter", default="/Library/Printers/PaperPlane/paperplane")
    ap.add_argument("--out", default=str(ROOT / "drivers"))
    ap.add_argument("--version", default="0.5.0")
    a = ap.parse_args()

    formats = json.loads(FORMATS.read_text(encoding="utf-8"))
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    total = 0
    for fam in FAMILIES:
        text, n = build(fam, formats, a.filter, a.version)
        (out / fam["leaf"]).write_text(text, encoding="utf-8")
        total += n
        print("%-22s %3d 种纸型  %6d 字节" % (fam["leaf"], n, len(text.encode())))
    print("合计纸型条目：%d（去重前）" % total)


if __name__ == "__main__":
    sys.exit(main())
