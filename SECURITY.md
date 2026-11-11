# 安全说明

## 攻击面

纸飞机由两部分组成，风险面很小但必须说清楚：

1. **CUPS filter（`/Library/Printers/PaperPlane/paperplane`）**
   由 cupsd 以 root 身份执行。为了满足 macOS 26 的要求，它必须是 `root:wheel`、`0755`。
   它**不联网、不下载、不执行子进程、不加载第三方动态库**，只读 stdin 的 raster 流，
   向 stdout 写打印机指令，并往 `/tmp/paperplane-filter.log` 追加一行日志。

2. **安装器（`install.sh`）**
   纯 shell，只调用系统自带命令（`lpinfo` / `lpadmin` / `lpstat` / `pkgbuild` / `installer`）。
   需要一次管理员授权，用于把 filter 写进 `/Library/Printers`。

## 已知设计取舍

- **`/tmp/paperplane-filter.log` 是全局可写的**（`chmod 0666`），因为 filter 以 root 运行、
  而用户需要能读它排障。这意味着本机其他用户可以伪造日志内容。
  该日志只用于排障，不参与任何安全决策。
- **filter 参数全部经过白名单解析**：`options` 字符串按 `key=value` 逐段解析，
  只接受已知 key，数值经 `strtod` 校验；档名不参与 shell 拼接。
- **不安装任何 LaunchAgent / LaunchDaemon**，没有常驻监听端口。

## 报告漏洞

请通过 GitHub Security Advisory 私下报告，不要开公开 issue。
请在报告中包含：macOS 版本、复现步骤、影响范围（是否能提权 / 是否能读出他人文件）。

承诺：3 个工作日内首次回复；确认后在修复版本发布时致谢（除非你要求匿名）。

## 不接受的报告

- 需要先物理接触设备并替换固件的攻击场景；
- 「PPD 机制已被 Apple 标记弃用」—— 这是已知的上游变更，见 CHANGELOG 的「已知边界」；
- 需要用户手动执行 `sudo` 并输入恶意路径的场景。
