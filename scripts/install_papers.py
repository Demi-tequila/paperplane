#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 data/formats.json 里的纸型写进 macOS 全局「自定义纸张」。

这是可选项：不做也能打印（打印对话框里可以直接输尺寸），
做了的好处是任何软件的纸型下拉里都能直接选到这些名字。

用法：python3 scripts/install_papers.py [--apply] [--list]
      默认 dry-run，只报将新增几条。
"""
import argparse
import json
import plistlib
import shutil
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FORMATS = ROOT / "data" / "formats.json"
PLIST = Path.home() / "Library/Preferences/com.apple.print.custompapers.plist"
BACKUP_DIR = Path.home() / "Library/Application Support/PaperPlane/backups"


def fmt_to_paper(f):
    """CUPS/AppKit 用 pt 表示尺寸；边距按 formats.json 的 margin_mm 换算。"""
    margin = float(f.get("margin_mm") or 0) * 72.0 / 25.4
    return {
        "name": f["name"],
        "id": f["name"],
        "width": float(f["width_pt"]),
        "height": float(f["height_pt"]),
        "left": margin,
        "right": margin,
        "top": margin,
        "bottom": margin,
        "custom": True,
        "printer": " ",
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()

    formats = json.loads(FORMATS.read_text(encoding="utf-8"))["formats"]
    existing = {}
    if PLIST.exists():
        try:
            existing = plistlib.loads(PLIST.read_bytes())
        except Exception as e:  # noqa: BLE001
            print("读取现有纸张失败：%s" % e)
            return 1

    new = {}
    for f in formats:
        if f["name"] in existing:
            continue
        new[f["name"]] = fmt_to_paper(f)

    if a.list:
        for f in formats:
            mark = "已有" if f["name"] in existing else "新增"
            print("%-4s %-30s %7.1f × %-7.1f mm" %
                  (mark, f["short_name"], f["width_mm"], f["height_mm"]))
        return 0

    print("现有 %d 条，本次新增 %d 条，共 %d 条。"
          % (len(existing), len(new), len(existing) + len(new)))
    if not a.apply:
        print("这是 dry-run；加 --apply 才真正写入。")
        return 0
    if not new:
        print("无需变更。")
        return 0

    BACKUP_DIR.mkdir(parents=True, exist_ok=True)
    if PLIST.exists():
        bak = BACKUP_DIR / ("custompapers-%s.plist" % datetime.now().strftime("%Y%m%d-%H%M%S"))
        shutil.copy2(PLIST, bak)
        print("已备份：%s" % bak)

    existing.update(new)
    PLIST.write_bytes(plistlib.dumps(existing, fmt=plistlib.FMT_XML))
    print("已写入 %s（%d 条）" % (PLIST, len(existing)))
    print("提示：正在运行的 App 需要重开才能在纸型列表里看到新尺寸。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
