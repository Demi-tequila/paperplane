# 参与开发

## 最需要的贡献：机型库

纸飞机支持一台新打印机的方式不是写代码，而是往 [`data/models.tsv`](../data/models.tsv) 加一行。
但**加行是有门槛的**：必须有出处。

```
family	regex	dpi	max_width	cut	evidence	source	note	sample
```

| 列 | 说明 |
|---|---|
| `family` | `tspl_label` / `zpl_label` / `escpos_receipt` / `escpos_label` / `sheet_native` / `unsupported` |
| `regex` | 对「厂商 型号」小写串做 ERE 匹配。**多品牌必须加括号**：`(得力\|deli).*(dl-?888)` |
| `dpi` | 原生点密度。这一列决定 1:1 尺寸，写错就打出放大/缩小的图 |
| `max_width` | 最大打印宽度 mm |
| `cut` | 1 = 带自动切刀 |
| `evidence` | `measured`（你手上有这台机器并验证过）或 `vendor-spec`（规格书明示） |
| `source` | 出处：规格书名称 + 章节 / 手册页码 / 实测记录 |
| `note` | 给用户看的中文说明 |
| `sample` | 一个具体型号串，必须能被本行 regex 命中（CI 会校验） |

CI 会拒绝这些情况：字段数不对、`evidence` 或 `source` 为空、`sample` 不被自己的正则命中、
非 `unsupported` 的行 dpi/幅宽为 0。

### 为什么这么严

因为「猜」会直接打出废纸。0.5.0 开发中实测抓到两个真实的猜错案例：

- `芯烨|xprinter.*58` 这种写法等于「品牌名裸匹配」，任何芯烨标签机都会被判成小票机；
- 设备自报 `CMD:ZPL,EPL,CPCL` 用 `*PCL*` 子串匹配会命中 `CPCL`，把标签机判成普通纸机。

所以：**没出处就不加，宁可返回 unknown 让用户自己选。**

## 报告不支持的机型

用「机型报告」模板提 issue，附上：

```bash
lpinfo -l -v | grep -A3 "usb://"
```

输出里的 `device-id`（含 `MFG:` / `MDL:` / `CMD:`）是最有价值的信息 —— 有 `CMD` 就能直接支持。

## 改代码

```bash
bash scripts/make.sh        # 生成 PPD + 编译通用二进制
bash install.sh --dry-run   # 只看会做什么，不写系统
python3 tests/harness.py    # 真实链路矩阵（需先完整安装一次）
```

### 硬规则（PR 会被按这个评审）

1. **不许猜。** 新增任何「设备不报就默认成 X」的分支都会被拒。
2. **不许静默改尺寸。** 超幅宽只能告警，不能缩放。
3. **不许在 filter 里联网或起子进程。** CLI 模式可以有子进程（`cupsfilter` / `sips`），
   filter 模式绝对不行 —— 它由 cupsd 以 root 身份运行。
4. **改判定逻辑只能改 `lib/resolve.sh`。** 安装器和测试台都调它，写第二份实现会被拒。
5. **默认输出必须逐字节稳定。** 改 TSPL 默认路径要说明为什么，并附前后字节对比。

## 提交

- 一个 PR 一件事；机型库可以批量。
- 提交信息用中文或英文都行，说清「改了什么、为什么、怎么验证的」。
- 涉及驱动输出的改动，请附 `pbbridge convert --out` 生成的指令流头尾各 200 字节。
