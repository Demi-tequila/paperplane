#!/bin/bash
# 纸飞机 PaperPlane · 卸载
# 只删纸飞机自己建的东西，不动你原有的打印机队列与系统驱动。
set -u

if [ -t 1 ]; then B=$'\033[1m'; G=$'\033[32m'; Y=$'\033[33m'; N=$'\033[0m'; else B=""; G=""; Y=""; N=""; fi
say() { printf "\n%s== %s%s\n" "$B" "$*" "$N"; }
ok()  { printf "  %s✓%s %s\n" "$G" "$N" "$*"; }
warn(){ printf "  %s!%s %s\n" "$Y" "$N" "$*"; }

say "卸载纸飞机 PaperPlane"

n=0
for q in $(lpstat -a 2>/dev/null | awk '{print $1}'); do
  case "$q" in
    PaperPlane_*)
      cancel -a "$q" >/dev/null 2>&1
      lpadmin -x "$q" >/dev/null 2>&1 && ok "已删除队列 $q" && n=$((n+1))
      ;;
  esac
done
[ "$n" = 0 ] && warn "没有找到纸飞机队列"

say "删除驱动文件（需要一次管理员密码）"
if [ "$(id -u)" = 0 ]; then
  rm -rf "/Library/Printers/PaperPlane" \
         "/Library/Printers/PPDs/Contents/Resources/PaperPlane"
  ok "已删除"
else
  if osascript -e 'do shell script "rm -rf /Library/Printers/PaperPlane /Library/Printers/PPDs/Contents/Resources/PaperPlane" with administrator privileges' >/dev/null 2>&1; then
    ok "已删除"
  else
    warn "未授权或删除失败；可手动执行：sudo rm -rf /Library/Printers/PaperPlane"
  fi
fi

rm -rf "$HOME/Library/Application Support/PaperPlane" 2>/dev/null
rm -rf "/Users/Shared/PaperPlane" 2>/dev/null
rm -f /tmp/paperplane-filter.log 2>/dev/null
ok "已清理本机配置与日志"

say "完成"
echo "  你原有的打印机（Epson / HP 等）不受影响，照常可用。"
