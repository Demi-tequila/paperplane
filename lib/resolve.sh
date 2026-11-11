#!/bin/bash
# 纸飞机 PaperPlane · 指令集判定（唯一实现，安装器与测试台都调用它）
#
# 用法：bash lib/resolve.sh "<厂商 型号>" "<CMD 字段>"
# 输出：<family> <依据> <dpi> <max_width_mm>
#
# 强制规则：不允许猜。
#   ① 设备自报 CMD 优先（权威）——按逗号分隔的 token 精确匹配，
#      不能用 *PCL* 这类子串匹配：Zebra 的 "ZPL,EPL,CPCL" 会被 CPCL 里的
#      PCL 误判成普通纸机（0.5.0 实测抓到过）。
#   ② 设备不报才查机型库，且必须命中有出处的行。
#   ③ 都没有 → unknown，由调用方停下，绝不默认成 TSPL。
set -u

PP_RULES="${PP_RULES:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/data/models.tsv}"

pb_cmd_tokens() {
  # 把 "ZPL,EPL,CPCL" 变成 ",ZPL,EPL,CPCL," 便于整词匹配
  printf ',%s,' "$(printf '%s' "$1" | tr -d ' ' | tr '[:lower:]' '[:upper:]')"
}

pb_classify() {
  local mm="${1:-}" cmd="${2:-}"
  local toks
  toks="$(pb_cmd_tokens "$cmd")"

  if [ -n "$cmd" ]; then
    # 标签指令集优先：一个设备同时报 ZPL 和 CPCL 时，标签机语义优先
    case "$toks" in
      *,TSPL,*|*,TSPL2,*)                    echo "tspl_label device-cmd 203 104"; return ;;
      # ZPL/EPL/CPCL：macOS 自带 rastertolabel（GUI 列表里的
      # "Zebra ZPL Label Printer"），纸飞机不重复提供，让路
      *,ZPL,*|*,ZPL2,*|*,EPL,*|*,CPCL,*)     echo "system_native_label device-cmd 203 104"; return ;;
      *,ESCPL2,*|*,ESCPOS,*|*,ESC/POS,*|*,STAR,*) echo "escpos_receipt device-cmd 203 80"; return ;;
      *,PCL,*|*,PCLXL,*|*,POSTSCRIPT,*|*,PS,*)     echo "sheet_native device-cmd 300 216"; return ;;
    esac
  fi

  local low
  low="$(printf '%s' "$mm" | tr '[:upper:]' '[:lower:]')"
  if [ -f "$PP_RULES" ]; then
    # 容忍带 BOM 的编辑结果：BOM 会让首行注释不生效，进而空正则匹配一切
    while IFS=$'\t' read -r f_re re r_dpi r_w _cut _ev _src _note _sample; do
      case "$f_re" in ''|\#*) continue ;; esac
      if printf '%s' "$low" | grep -qE "$re" 2>/dev/null; then
        echo "$f_re model-db ${r_dpi:-203} ${r_w:-104}"
        return
      fi
    done < <(sed '1s/^\xEF\xBB\xBF//' "$PP_RULES")
  fi

  echo "unknown none 0 0"
}

# 直接执行时当作 CLI 用
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  pb_classify "${1:-}" "${2:-}"
fi
