# 更新记录

## 0.5.1（2026-11-11 同日）

### 与系统自带驱动去重（用户实测发现）

- 用户在「打印机软件」列表里发现 Zebra ZPL / EPL / CPCL 条目 —— macOS 自带 `rastertolabel`
  能生成真正的 ZPL（`^PW`/`^LL`/`^XGR`）、EPL（`GW`）、CPCL 指令。经核实，纸飞机的
  `zpl_label` 族与它完全重复，**整族移除**：Zebra / Honeywell / SATO / Godex / Citizen
  现在判定为 `system_native_label`，安装时明确让路给系统驱动，不再创建重复队列。
- 修复 `convert` 的收尾提示被写进指令流文件尾部（`freopen` 后 stdout 指向目标文件，
  状态信息一律改走 stderr）。
- 清理 0.4.x 遗留的重复驱动（DeliLabel.ppd、PrintBridge、DeliLabel 旧 filter 与旧队列），
  系统「打印机软件」列表不再出现同名条目。
- 驱动内保留 `--pdl zpl` 生成能力与离线方言测试兜底，供高级用户与测试使用。

## 0.5.0（2026-11-11）

面向开源发布的重构。核心变化是「零依赖安装」和「判定不允许猜」。

### 安装体验
- **不再需要 Python、clang、Xcode 命令行工具**。之前安装脚本要先 `py_compile` 再 `clang` 编译，
  用户装一个驱动得先装半个开发环境。现在驱动是预编译的 arm64+x86_64 通用二进制，
  安装器是纯 shell。
- **去掉全部后台组件**：不再需要 LaunchAgent、不再有常驻 IPP 服务。
  打印走的是 CUPS 原生队列，开机自启、断电恢复都由 macOS 自己负责。
- 装完自动枚举打印机、判定指令集、建好队列并命名成 `PaperPlane_<机型>`。
- 双击 `安装.command` 自动弹密码框；已安装且版本一致时**不再要密码**。

### 判定逻辑（强制不猜）
- 抽出 `lib/resolve.sh` 作为唯一实现，安装器与测试台共用，杜绝两边逻辑漂移。
- 设备自报 `CMD` 字段按**逗号分隔 token 精确匹配**。修复：Zebra 的 `CMD:ZPL,EPL,CPCL`
  曾被 `*PCL*` 子串匹配误判成普通纸机。
- 修复机型库正则缺括号导致的品牌裸匹配：`芯烨|xprinter.*58` 会把任意芯烨标签机
  判成小票机，现在必须写成 `(芯烨|xprinter).*(58|80)`。
- 未知机型一律拒绝自动绑定并说明原因，不再回落到「保守默认 TSPL」。
- 删除没有出处的推测性规则（ESC/POS 标签族），该族只保留手动指定。

### 驱动
- 单二进制双模式：CUPS filter + CLI（`version` / `preview` / `convert`）。
- 新增 `preview`：PDF 渲染成 PNG，不出纸先看效果。
- 新增队列级配置 `/Users/Shared/PaperPlane/queues/<队列>.json`。
  修复：全局 `device-settings.json` 的 `pdl` 会跨队列串味，导致 ZPL / ESC-POS 队列
  被打成 TSPL。
- PPD 增加 `*ImageableArea`（满幅）。修复：缺失时 CUPS 算出 `cupsWidth=0`，
  驱动拿不到任何像素，整页空白。
- ESC/POS 位图改为 255 行分带输出，兼容单条指令有行数上限的廉价固件；切刀前补走纸。
- TSPL 增加 `SET CUTTER ON` / `CUT` 支持；默认路径与 0.4.x 逐字节一致。

### 测试
- 新增真实链路测试台 `tests/harness.py`：每条机型规则建真实 CUPS 队列，
  作业走 cupsd → cgpdftoraster → 驱动 → socket 后端，抓回字节按指令集语法校验。
- 新增判定阶梯用例，包含「未知机型必须被拒绝」的负向断言。

### 已知边界
- ZPL / ESC-POS 仅离线语法验证，无对应真机。
- 纸张端为回环记录器，不验证物理出纸效果。
- macOS 26 起 PPD 机制被标记为弃用，长期需迁移到 IPP Everywhere / PAE。

## 0.4.1
- 驱动扩成 4 条输出路径（TSPL / ZPL / ESC-POS / 透传），5 个指令集族 + 18 条规则。
- 100 种格式逐条离线验证并分级记录。

## 0.4.0
- 全量重构：格式库、全格式 PPD、双证据链路探测、主机侧配置持久化。

## 0.3.0
- 打通「端点 → 转发层 → CUPS 真机队列」链路，修复取消作业被误报为完成。
