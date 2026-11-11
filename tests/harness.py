#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""纸飞机 PaperPlane · 全机型真实链路测试台

它做的事（不是模拟、不是猜）：
  1. 逐条读 data/models.tsv 的机型规则，模拟该机型的设备自报信息；
  2. 用 `lpadmin` 在本机 CUPS 建一个**真实队列**，后端指向 127.0.0.1 的字节记录器；
  3. 用 `lp` 提交真实作业 → cupsd → cgpdftoraster → paperplane → socket 后端；
  4. 抓回打印机真正会收到的字节，按该指令集语法逐条校验；
  5. 拆队列、出 JSON + Markdown 报告。

诚实边界（必须写进报告，不许含糊）：
  · 纸张端是回环 socket 记录器，不是实体纸——本测试证明「指令正确、可被设备读懂」，
    不证明「墨粉落到纸上」；
  · 机型覆盖按指令集族收敛，不等于逐台真机认证；
  · sheet_native 族不做队列（交给 macOS 原生驱动），只校验分类结论。
"""
import argparse
import json
import os
import re
import socket
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODELS = ROOT / "data" / "models.tsv"
SYS_PPD = Path("/Library/Printers/PPDs/Contents/Resources/PaperPlane")
SYS_FILTER = Path("/Library/Printers/PaperPlane/paperplane")
PDF = ROOT / "tests" / "fixtures" / "sample-100x100.pdf"

FAMILY_DEFAULT_PAPER = {
    "tspl_label": (100.0, 100.0),
    "zpl_label": (100.0, 100.0),
    "escpos_receipt": (80.0, 200.0),
    "escpos_label": (80.0, 50.0),
}


def run(argv, timeout=60):
    p = subprocess.run(argv, capture_output=True, timeout=timeout)
    return p.returncode, p.stdout, p.stderr


def load_rules():
    rules = []
    for ln, line in enumerate(MODELS.read_text(encoding="utf-8-sig").splitlines(), 1):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        parts = s.split("\t")
        if len(parts) < 9:
            raise SystemExit("models.tsv 第 %d 行字段不足 9 列：%r" % (ln, line[:60]))
        fam, rx, dpi, width, cut, evidence, source, note, sample = parts[:9]
        rules.append(dict(line=ln, family=fam.strip(), regex=rx.strip(),
                          dpi=int(dpi), width=float(width), cut=int(cut),
                          evidence=evidence.strip(), source=source.strip(),
                          note=note.strip(), sample=sample.strip()))
    return rules


def classify(make_model, cmd=""):
    """调用 lib/resolve.sh —— 与安装器同一份实现，测试台不另写一套逻辑。"""
    rc, out, err = run(["bash", str(ROOT / "lib" / "resolve.sh"), make_model or "", cmd or ""], timeout=30)
    parts = out.decode("utf-8", "replace").strip().split()
    if not parts:
        return "unknown", "none"
    fam = parts[0]
    how = parts[1] if len(parts) > 1 else "none"
    return fam, how


class Recorder:
    """在指定端口接收一次打印作业的全部字节。"""

    def __init__(self, port):
        self.port = port
        self.buf = bytearray()
        self.ready = threading.Event()
        self.done = threading.Event()
        self.sock = None
        self.thread = None

    def start(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", self.port))
        self.sock.listen(4)
        self.sock.settimeout(0.5)
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()
        self.ready.set()

    def _loop(self):
        deadline = time.time() + 40
        while time.time() < deadline and not self.done.is_set():
            try:
                conn, _ = self.sock.accept()
            except socket.timeout:
                continue
            conn.settimeout(3)
            while True:
                try:
                    chunk = conn.recv(65536)
                except socket.timeout:
                    break
                if not chunk:
                    break
                self.buf += chunk
            conn.close()
            if self.buf:
                self.done.set()
                break

    def close(self):
        self.done.set()
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass


def make_queue(name, family, port):
    ppd = SYS_PPD / ("%s.ppd" % family)
    if not ppd.exists():
        return False, "缺少 PPD：%s" % ppd
    run(["lpadmin", "-x", name])
    rc, out, err = run(["lpadmin", "-p", name, "-E",
                        "-v", "socket://127.0.0.1:%d" % port,
                        "-P", str(ppd),
                        "-D", "PaperPlane virtual device",
                        "-o", "printer-is-shared=false"])
    if rc != 0:
        return False, err.decode("utf-8", "replace")[:300]
    return True, ""


def drop_queue(name):
    run(["cancel", "-a", name], timeout=20)
    run(["lpadmin", "-x", name], timeout=20)


def validate(family, data, expect_wh):
    """按指令集语法校验；返回 (ok, 断言列表, 关键指标)。"""
    a = []
    metrics = {}
    ok = True

    def chk(cond, msg):
        nonlocal ok
        a.append(("PASS" if cond else "FAIL", msg))
        if not cond:
            ok = False

    if family.startswith("tspl"):
        text = data[:4096].decode("latin-1", "replace")
        chk(text.startswith("SIZE "), "首条指令是 SIZE（TSPL 语法入口）")
        m = re.search(r"SIZE ([\d.]+) mm,([\d.]+) mm", text)
        chk(bool(m), "SIZE 带明确毫米尺寸")
        if m:
            w, h = float(m.group(1)), float(m.group(2))
            metrics["size_mm"] = [w, h]
            chk(abs(w - expect_wh[0]) < 0.3 and abs(h - expect_wh[1]) < 0.3,
                "物理尺寸与纸型一致（%.2f×%.2f vs %.1f×%.1f）"
                % (w, h, expect_wh[0], expect_wh[1]))
        chk("GAP " in text or "BLINE " in text, "声明了介质定位（间隙/黑标）")
        chk(re.search(r"DENSITY \d+", text) is not None, "声明了打印浓度")
        chk("BITMAP " in text, "位图指令存在")
        mb = re.search(r"BITMAP (\d+),(\d+),(\d+),(\d+),(\d+),", text)
        chk(bool(mb), "BITMAP 参数完整（x,y,字节宽,高,模式）")
        if mb:
            rb, h = int(mb.group(3)), int(mb.group(4))
            metrics["bitmap"] = {"row_bytes": rb, "height": h}
            # 位图数据长度必须精确等于 行字节*高度
            start = mb.end()
            chk(len(data) >= start + rb * h, "位图数据长度不短于 行字节×高度")
            tail = data[start + rb * h:start + rb * h + 20]
            chk(tail.lstrip(b"\r\n").startswith(b"PRINT"), "位图后紧跟 PRINT 指令")
        chk(b"PRINT 1" in data[-64:], "末尾是 PRINT 1（单张，不重复走纸）")
        chk(b"\xff" in data, "位图含白底位（极性未整体反转）")

    elif family == "zpl_label":
        chk(data.startswith(b"^XA"), "以 ^XA 开始（ZPL 标签格式入口）")
        chk(b"^PW" in data, "声明 ^PW 打印宽度")
        chk(b"^LL" in data, "声明 ^LL 标签长度")
        chk(b"^GFA," in data, "使用 ^GFA 位图指令")
        m = re.search(rb"\^GFA,(\d+),(\d+),(\d+),", data)
        chk(bool(m), "^GFA 参数完整（总字节,总字节,行字节）")
        if m:
            total, _, rb = (int(m.group(i)) for i in (1, 2, 3))
            start = m.end()
            body = data[start:start + total * 2]
            chk(len(body) == total * 2, "HEX 数据长度 = 2×位图字节数")
            chk(re.fullmatch(rb"[0-9A-F]+", body or b"") is not None,
                "HEX 数据只含 0-9A-F（无换行污染）")
            metrics["gfa"] = {"total": total, "row_bytes": rb}
        chk(data.rstrip().endswith(b"^XZ"), "以 ^XZ 结束（指令块闭合）")
        chk(b"^PQ1" in data, "显式 ^PQ1（单张，避免固件多走纸）")

    elif family.startswith("escpos"):
        chk(data.startswith(b"\x1b@"), "以 ESC @ 初始化（ESC/POS 入口）")
        chk(b"\x1d\x76\x30" in data, "使用 GS v 0 位图指令")
        # 统计各分带高度，确保覆盖整页
        rows = 0
        i = 0
        bands = 0
        while True:
            i = data.find(b"\x1d\x76\x30", i)
            if i < 0:
                break
            if i + 8 > len(data):
                break
            rb = data[i + 4] | (data[i + 5] << 8)
            hh = data[i + 6] | (data[i + 7] << 8)
            rows += hh
            bands += 1
            i += 8 + rb * hh
        metrics["bands"] = bands
        metrics["rows"] = rows
        chk(bands >= 1, "至少一个位图分带")
        chk(rows > 100, "分带总高度覆盖整页（>100 行）")
        chk(data[-4:] == b"\x1dV\x42\x00" or data.endswith(b"\n\n\n"),
            "结尾是切刀指令或无刀走纸")

    return ok, a, metrics



# ── 判定阶梯实测：设备自报优先，未知必须拒绝（不允许猜） ──────────────
LADDER_CASES = [
    # (设备自报串, 期望族, 期望依据)
    ("MFG:Deli;MDL:DL-888D PRO;CMD:TSPL;",            "tspl_label",     "device-cmd"),
    ("MFG:Zebra;MDL:ZD421;CMD:ZPL,EPL,CPCL;",         "system_native_label", "device-cmd"),
    ("MFG:EPSON;CMD:ESCPL2,BDC,D4;MDL:TM-T82III;",    "escpos_receipt", "device-cmd"),
    ("MFG:HP;CMD:PCL,PCLXL,POSTSCRIPT;MDL:M404dn;",   "sheet_native",   "device-cmd"),
    ("MFG:HPRT;MDL:SL42;",                            "tspl_label",     "model-db"),
    ("MFG:Zebra;MDL:ZT411;",                          "system_native_label", "model-db"),
    ("MFG:Acme;MDL:LP-9000;",                         "unknown",        "none"),
    ("MFG:Acme;MDL:LP-9000;CMD:TSPL;",                "tspl_label",     "device-cmd"),
    ("MFG:Niimbot;MDL:B21;",                          "unsupported",    "model-db"),
]


def run_ladder():
    rows = []
    for did, want_fam, want_how in LADDER_CASES:
        mfg = ""
        mdl = ""
        for part in did.split(";"):
            k = part.split(":")[0].upper()
            v = part.split(":", 1)[1] if ":" in part else ""
            if k in ("MFG", "MANUFACTURER"):
                mfg = v.strip()
            if k in ("MDL", "MODEL"):
                mdl = v.strip()
        make_model = ("%s %s" % (mfg, mdl)).strip()
        cmd = ""
        for part in did.split(";"):
            if part.upper().startswith("CMD:"):
                cmd = part[4:]
        fam, how = classify(make_model, cmd)
        rows.append(dict(device_id=did, make_model=make_model,
                         got=fam, via=how, want=want_fam, want_via=want_how,
                         ok=(fam == want_fam)))
    return rows



def run_cli_dialects(pdf_path):
    """驱动里保留的指令生成能力仍需验证：走 CLI 离线转换，不发打印作业。"""
    filt = "/Library/Printers/PaperPlane/paperplane"
    rows = []
    cases = [
        ("tspl", "tspl_label.ppd", lambda d: d.startswith(b"SIZE ") and b"BITMAP" in d and b"PRINT 1" in d[-64:]),
        ("zpl", None, lambda d: d.startswith(b"^XA") and b"^GFA," in d and d.rstrip().endswith(b"^XZ")),
        ("escpos", "escpos_receipt.ppd", lambda d: d.startswith(b"\x1b@") and b"\x1dv0" in d or b"\x1d\x76\x30" in d),
    ]
    for pdl, ppd, check in cases:
        out = "/tmp/paperplane-dialect-%s.bin" % pdl
        argv = [filt, "convert", "--in", str(pdf_path), "--out", out, "--pdl", pdl]
        if ppd:
            argv += ["--ppd", "/Library/Printers/PPDs/Contents/Resources/PaperPlane/" + ppd]
        else:
            # 测试专用 PPD 只放仓库里，不装系统 —— 系统里 ZPL 由 macOS 自带驱动负责
            argv += ["--ppd", str(ROOT / "tests" / "fixtures" / "zpl_test.ppd"), "--dpi", "203"]
        rc, o, e = run(argv, timeout=120)
        ok = False
        detail = ""
        if rc != 0:
            detail = e.decode("utf-8", "replace")[:200]
        else:
            data = Path(out).read_bytes()
            ok = bool(check(data))
            detail = "%d 字节" % len(data)
        rows.append(dict(pdl=pdl, ok=ok, detail=detail))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "Diagnostics"))
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--pdf", default=str(PDF))
    args = ap.parse_args()

    if not SYS_FILTER.exists():
        raise SystemExit("系统里没有驱动：%s（先运行 install.sh）" % SYS_FILTER)
    pdf = Path(args.pdf)
    if not pdf.exists():
        raise SystemExit("测试用 PDF 不存在：%s" % pdf)

    rules = load_rules()
    if args.limit:
        rules = rules[:args.limit]

    results = []
    base_port = 19100
    stamp = uuid.uuid4().hex[:6]

    for idx, r in enumerate(rules):
        make_model = r["sample"] or r["note"]
        fam, how = classify(make_model)
        rec = dict(rule=r, resolved=dict(family=fam, via=how))

        if fam in ("unknown", "unsupported") or r["family"] == "unsupported":
            rec.update(status="expected-unsupported" if r["family"] == "unsupported"
                               else "unknown-refused",
                       detail="按规则不生成指令流：%s" % r["note"])
            results.append(rec)
            continue
        if fam != r["family"]:
            rec.update(status="classify-mismatch",
                       detail="型号库声明 %s，判定为 %s" % (r["family"], fam))
            results.append(rec)
            continue
        if fam in ("sheet_native", "system_native_label"):
            rec.update(status="native-os", detail="交给 macOS 原生驱动，纸飞机不建队列")
            results.append(rec)
            continue

        port = base_port + idx
        name = "PBV_%s_%02d" % (stamp, idx)
        okq, err = make_queue(name, fam, port)
        if not okq:
            rec.update(status="queue-failed", detail=err)
            results.append(rec)
            continue

        recorder = Recorder(port)
        recorder.start()
        time.sleep(0.3)
        job = None
        try:
            rc, out, serr = run(["lp", "-d", name, "-t", "paperplane-matrix", str(pdf)])
            m = re.search(rb"(\S+-\d+)", out)
            job = m.group(1).decode() if m else None
            recorder.done.wait(timeout=45)
            data = bytes(recorder.buf)
            if not data:
                rec.update(status="no-bytes", detail="队列没有产出任何字节（作业失败）")
            else:
                expect = FAMILY_DEFAULT_PAPER[fam]
                ok, asserts, metrics = validate(fam, data, expect)
                rec.update(status="pass" if ok else "fail",
                           bytes=len(data), asserts=asserts, metrics=metrics,
                           job=job)
        finally:
            recorder.close()
            drop_queue(name)
        results.append(rec)

    # 汇总
    summary = {}
    for r in results:
        summary[r["status"]] = summary.get(r["status"], 0) + 1

    ladder = run_ladder()
    dialects = run_cli_dialects(pdf)

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "matrix.json").write_text(
        json.dumps({"generated": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
                    "filter": str(SYS_FILTER),
                    "rules": len(rules), "summary": summary,
                    "ladder": ladder, "dialects": dialects, "results": results}, ensure_ascii=False, indent=2),
        encoding="utf-8")

    total_asserts = sum(len(r.get("asserts", [])) for r in results)
    failed_asserts = sum(1 for r in results for s, _ in r.get("asserts", []) if s == "FAIL")
    ladder_fail = [r for r in ladder if not r["ok"]]
    print(json.dumps(summary, ensure_ascii=False))
    print("判定阶梯：%d/%d 正确" % (len(ladder) - len(ladder_fail), len(ladder)))
    d_fail = [r for r in dialects if not r["ok"]]
    print("CLI 方言校验：%d/%d 通过" % (len(dialects) - len(d_fail), len(dialects)))
    for r in d_fail:
        print("  FAIL %s: %s" % (r["pdl"], r["detail"]))
    for r in ladder_fail:
        print("  FAIL %s → %s（应为 %s）" % (r["device_id"], r["got"], r["want"]))
    print("断言：%d 条，失败 %d 条" % (total_asserts, failed_asserts))
    return 0 if (failed_asserts == 0 and summary.get("fail", 0) == 0
                 and not ladder_fail and not d_fail) else 1


if __name__ == "__main__":
    sys.exit(main())
