#!/bin/bash
# 纸飞机 PaperPlane 0.5.0 · 安装器
#
# 目标：用户只需要双击一次，剩下的全部自动完成。
#   · 不装 Python、不装 Xcode、不装后台服务、不联网；
#   · 驱动落 /Library/Printers/PaperPlane/paperplane（macOS 26 唯一允许 cupsd 执行的位置）；
#   · 自动枚举本机打印机 → 判定指令集 → 建好队列 → 打印时直接选名字。
#
# 强制规则：不允许猜。
#   判定顺序：设备自报 device-id 的 CMD 字段 → 有出处的机型库 → 未知则停止自动绑定。
set -u

VERSION="0.5.1"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_SRC="$HERE/bin/paperplane"
PPD_SRC="$HERE/drivers"
SYS_DIR="/Library/Printers/PaperPlane"
SYS_PPD_DIR="/Library/Printers/PPDs/Contents/Resources/PaperPlane"
STATE_DIR="$HOME/Library/Application Support/PaperPlane"
RULES="$HERE/data/models.tsv"
DRY=0
ASSUME_YES=0
WANT_PAPERS=0
BIND_ONLY=0
SKIP_INSTALL=0
FORCE_FAMILY=""
FORCE_URI=""

# ── 输出 ──────────────────────────────────────────────────────────────
if [ -t 1 ]; then
  B=$'\033[1m'; DIM=$'\033[2m'; R=$'\033[31m'; G=$'\033[32m'; Y=$'\033[33m'; N=$'\033[0m'
else
  B=""; DIM=""; R=""; G=""; Y=""; N=""
fi
say()  { printf "\n%s== %s%s\n" "$B" "$*" "$N"; }
ok()   { printf "  %s✓%s %s\n" "$G" "$N" "$*"; }
warn() { printf "  %s!%s %s\n" "$Y" "$N" "$*"; }
bad()  { printf "  %s✗%s %s\n" "$R" "$N" "$*"; }
dim()  { printf "  %s%s%s\n" "$DIM" "$*" "$N"; }

run() { if [ "$DRY" = 1 ]; then printf "  %s[dry-run] %s%s\n" "$DIM" "$*" "$N"; return 0; fi; "$@"; }

# ── 参数 ──────────────────────────────────────────────────────────────
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY=1 ;;
    --yes|-y)  ASSUME_YES=1 ;;
    --with-papers) WANT_PAPERS=1 ;;
    --bind)    SKIP_INSTALL=1 ;;
    --bind-family)
      FORCE_FAMILY="${2:-}"; shift
      FORCE_URI="${2:-}"; shift ;;
    --version) echo "$VERSION"; exit 0 ;;
    --help|-h) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
  esac
  shift
done

# ── 0. 环境自检 ───────────────────────────────────────────────────────
say "纸飞机 PaperPlane $VERSION"
[ "$(uname -s)" = "Darwin" ] || { bad "只支持 macOS"; exit 1; }
OSVER="$(sw_vers -productVersion 2>/dev/null || echo "?")"
dim "macOS $OSVER · $(uname -m)"

miss=0
for f in "$BIN_SRC" "$PPD_SRC/tspl_label.ppd" \
         "$PPD_SRC/escpos_receipt.ppd" "$RULES"; do
  [ -e "$f" ] || { bad "安装包不完整，缺少：$f"; miss=1; }
done
[ "$miss" = 0 ] || exit 1
ok "安装包完整（驱动 + 4 族 PPD + 机型库）"

if command -v xattr >/dev/null; then
  xattr -dr com.apple.quarantine "$HERE" 2>/dev/null || true
fi

# ── 1. 落地驱动（需要一次管理员授权） ─────────────────────────────────
need_root=0
if [ "$SKIP_INSTALL" = 1 ]; then
  if [ -x "$SYS_DIR/paperplane" ]; then
    ok "驱动已在系统里，跳过安装（--bind）"
  else
    bad "驱动尚未安装，--bind 需要先完整安装一次（去掉 --bind 再运行）"
    exit 1
  fi
elif [ -x "$SYS_DIR/paperplane" ] && [ -d "$SYS_PPD_DIR" ]; then
  if cmp -s "$BIN_SRC" "$SYS_DIR/paperplane"; then
    ok "驱动已是最新，跳过写入（不需要密码）"
  else
    need_root=1
  fi
else
  need_root=1
fi

SUDO=""
if [ "$need_root" = 1 ] && [ "$DRY" = 0 ]; then
  say "安装驱动到系统目录"
  dim "macOS 26 规定：只有 root 拥有的 filter 才允许被 cupsd 执行。"
  dim "所以这一步必须输一次管理员密码，之后永不再问。"
  if [ "$(id -u)" = 0 ]; then
    SUDO=""
  elif sudo -n true 2>/dev/null; then
    SUDO="sudo"
  else
    # 双击 .command 时用系统密码框，比终端里敲密码更省事
    if osascript -e 'do shell script "true" with administrator privileges' >/dev/null 2>&1; then
      SUDO="SUDO_ASKPASS"
    else
      bad "未获得管理员授权，无法安装驱动"
      dim "可以右键本文件 → 打开，或在终端执行：sudo bash \"$0\""
      exit 1
    fi
  fi
fi

elev() {
  # 用一次授权执行一条命令；优先 sudo，其次 osascript（双击场景）
  if [ "$SUDO" = "SUDO_ASKPASS" ]; then
    /usr/bin/osascript -e "do shell script \"$1\" with administrator privileges" >/dev/null 2>&1
  else
    $SUDO /bin/sh -c "$1"
  fi
}

if [ "$need_root" = 1 ] && [ "$DRY" = 0 ]; then
  TMP="$(mktemp -d)"
  mkdir -p "$TMP/root/Library/Printers/PaperPlane" \
           "$TMP/root/Library/Printers/PPDs/Contents/Resources/PaperPlane"
  cp "$BIN_SRC" "$TMP/root/Library/Printers/PaperPlane/paperplane"
  cp "$PPD_SRC"/*.ppd "$TMP/root/Library/Printers/PPDs/Contents/Resources/PaperPlane/"
  chmod 755 "$TMP/root/Library/Printers/PaperPlane/paperplane"
  chmod 644 "$TMP/root/Library/Printers/PPDs/Contents/Resources/PaperPlane"/*.ppd

  installed=0
  # 首选：直接以 root 复制（用户自己的终端里通常可行）
  if elev "mkdir -p '$SYS_DIR' '$SYS_PPD_DIR' && cp '$TMP/root/Library/Printers/PaperPlane/paperplane' '$SYS_DIR/paperplane' && cp '$TMP/root/Library/Printers/PPDs/Contents/Resources/PaperPlane/'*.ppd '$SYS_PPD_DIR/' && chown -R root:wheel '$SYS_DIR' '$SYS_PPD_DIR' && chmod 755 '$SYS_DIR/paperplane' && chmod 644 '$SYS_PPD_DIR/'*.ppd" 2>/dev/null; then
    installed=1
  fi
  # 兜底：走安装包（某些 macOS 版本对 /Library/Printers 有额外保护）
  if [ "$installed" = 0 ]; then
    dim "直接写入被系统拒绝，改用安装包（同样只输一次密码）"
    if pkgbuild --root "$TMP/root" --identifier cn.paperplane.driver \
                --version "$VERSION" --install-location / "$TMP/pb.pkg" >/dev/null 2>&1; then
      if elev "installer -pkg '$TMP/pb.pkg' -target /" 2>/dev/null; then installed=1; fi
    fi
  fi
  rm -rf "$TMP"

  if [ "$installed" = 1 ] && [ -x "$SYS_DIR/paperplane" ]; then
    ok "驱动已安装：$SYS_DIR/paperplane"
    ok "PPD 已登记：$(ls "$SYS_PPD_DIR" | wc -l | tr -d ' ') 个指令集族"
  else
    bad "驱动安装失败"
    dim "请手动执行：sudo bash \"$0\""
    exit 1
  fi
fi

# ── 2. 判定阶梯：设备自报 > 有出处的型号库 > 未知停止 ─────────────────
# 用法：classify "<厂商 型号>" "<CMD 字段>"
# 输出：<family> <依据>，family 为 unknown 时调用方必须停止，不得默认
# 判定逻辑只有一份实现：lib/resolve.sh（安装器与测试台共用，杜绝两边漂移）
# shellcheck source=lib/resolve.sh
. "$HERE/lib/resolve.sh"
classify() { pb_classify "${1:-}" "${2:-}"; }
PP_RULES="$RULES"

# 队列级配置：一队列一事实，写进 /Users/Shared（免密码，cupsd 可读）
write_queue_config() {
  local q="$1" fam="$2" dpi="$3" w="$4"
  local dir="/Users/Shared/PaperPlane/queues"
  mkdir -p "$dir" 2>/dev/null || return 0
  case "$fam" in
    tspl_label)     printf '{"pdl":"tspl","dpi":%s,"max_width_mm":%s,"media_type":"gap","gap_mm":2,"density":8,"speed":4,"direction":1,"rotate180":1,"invert":1,"tear_off":1,"cut":0}\n' "$dpi" "$w" > "$dir/$q.json" ;;
    zpl_label)      printf '{"pdl":"zpl","dpi":%s,"max_width_mm":%s,"media_type":"gap","gap_mm":2,"density":8,"speed":4,"direction":0,"rotate180":0,"invert":0,"tear_off":1,"cut":0}\n' "$dpi" "$w" > "$dir/$q.json" ;;
    escpos_receipt) printf '{"pdl":"escpos","dpi":%s,"max_width_mm":%s,"media_type":"continuous","gap_mm":0,"density":8,"speed":4,"direction":0,"rotate180":0,"invert":0,"tear_off":1,"cut":1}\n' "$dpi" "$w" > "$dir/$q.json" ;;
    escpos_label)   printf '{"pdl":"escpos","dpi":%s,"max_width_mm":%s,"media_type":"gap","gap_mm":2,"density":8,"speed":4,"direction":0,"rotate180":0,"invert":0,"tear_off":1,"cut":0}\n' "$dpi" "$w" > "$dir/$q.json" ;;
  esac
  chmod 644 "$dir/$q.json" 2>/dev/null || true
}

# ── 3. 枚举本机打印机 ─────────────────────────────────────────────────
say "扫描本机打印机"
DEVFILE="$(mktemp)"
trap 'rm -f "$DEVFILE"' EXIT

# lpinfo -l -v 是 CUPS 的权威设备清单：usb:// 与 ipp:// 都带 device-id
lpinfo -l -v 2>/dev/null | awk '
  function flush() {
    if (uri ~ /^(usb|ipp|ipps|socket|lpd):\/\// && uri !~ /127\.0\.0\.1/)
      printf "%s\t%s\t%s\n", uri, mm, id
    uri = ""; mm = ""; id = ""
  }
  /uri = /           { flush(); uri = $0; sub(/.*uri = /, "", uri); gsub(/^[ \t]+|[ \t]+$/, "", uri) }
  /make-and-model = /{ mm = $0; sub(/.*make-and-model = /, "", mm); gsub(/^[ \t]+|[ \t]+$/, "", mm) }
  /device-id = /     { id = $0; sub(/.*device-id = /, "", id); gsub(/^[ \t]+|[ \t]+$/, "", id) }
  END                { flush() }
' > "$DEVFILE" 2>/dev/null || true

# 纯网络机型：lpinfo 的 dnssd 条目没有 device-id，用 ippfind 补设备自报属性
if command -v ippfind >/dev/null 2>&1; then
  ippfind --timeout 1 2>/dev/null | grep -E '^ipps?://' | while read -r u; do
    u2="$(printf '%s' "$u" | sed 's|/$||')"
    attrs="$(ipptool -tv "$u2" get-printer-attributes.test 2>/dev/null \
             | grep -E 'printer-device-id|printer-make-and-model' | head -2)"
    did="$(printf '%s' "$attrs" | sed -n 's/.*printer-device-id[^=]*= *//p' | head -1)"
    mm2="$(printf '%s' "$attrs" | sed -n 's/.*printer-make-and-model[^=]*= *//p' | head -1)"
    [ -n "$did$mm2" ] && printf '%s\t%s\t%s\n' "$u2" "$mm2" "$did"
  done >> "$DEVFILE" 2>/dev/null || true
fi

# 已有队列也要认：用户可能早在「系统设置 → 打印机」里加过，不必重加。
# usb://Deli/DL-888D%20PRO?serial=xxx 里的厂商/型号来自 USB 描述符，
# 是设备自报信息，不是我们推测出来的。
lpstat -v 2>/dev/null | sed -n 's/.*设备：\(.*\)$/\1/p; s/^device for .*: \(.*\)$/\1/p' \
  | grep -E '^(usb|ipp|ipps)://' | grep -v '127\.0\.0\.1' | while read -r u; do
      mm="$(printf '%s' "$u" | sed 's|^[a-z]*://||; s|?.*||' | tr '/' ' ' | sed 's/%20/ /g')"
      printf '%s\t%s\t\n' "$u" "$mm"
    done >> "$DEVFILE" 2>/dev/null || true

# 去重、去空
grep -v '^[[:space:]]*$' "$DEVFILE" 2>/dev/null | sort -u > "$DEVFILE.2" 2>/dev/null || true
if [ -s "$DEVFILE.2" ]; then
  awk -F'\t' '!seen[$1]++' "$DEVFILE.2" > "$DEVFILE.3" 2>/dev/null && mv "$DEVFILE.3" "$DEVFILE"
fi

DEVS="$(grep -c . "$DEVFILE" 2>/dev/null)"
DEVS="${DEVS:-0}"
if [ "$DEVS" = 0 ]; then
  warn "没检测到已连接的打印机"
  dim "这很正常：打印机没开机、没插线、或是纯网络机型。"
  dim "驱动已经装好；下次开机后运行：bash \"$0\" --bind"
else
  ok "发现 $DEVS 个设备条目"
fi

# ── 4. 绑定队列 ───────────────────────────────────────────────────────
bind_one() {
  local uri="$1" mm="$2" did="$3"
  local cmd=""
  case "$did" in
    *CMD:*) cmd="${did#*CMD:}"; cmd="${cmd%%;*}" ;;
  esac
  [ -n "$mm" ] || mm="$(printf '%s' "$uri" | sed 's|.*/||; s|?.*||' | sed 's|%20| |g')"

  set -- $(classify "$mm" "$cmd")
  local fam="$1" how="$2" r_dpi="${3:-203}" r_w="${4:-104}"

  if [ "$fam" = "unknown" ]; then
    bad "未知机型：$mm"
    dim "依据：设备没有自报指令集（CMD 为空），型号库也没有匹配到带出处的条目。"
    dim "按规则不做猜测，不建队列。处理方式二选一："
    dim "  a) 把型号发到 GitHub issue，补一条带规格书出处的规则；"
    dim "  b) 确认规格书后手动指定：bash install.sh --bind-family tspl_label \"$uri\""
    return 2
  fi
  if [ "$fam" = "unsupported" ]; then
    warn "不支持：${mm}（私有协议，无法生成设备能读懂的指令）"
    dim "原因：$(_note_for "$mm")"
    return 3
  fi
  if [ "$fam" = "sheet_native" ]; then
    ok "跳过：$mm 是普通纸机，直接用 macOS 原生驱动（纸飞机不接管）"
    return 4
  fi

  local qname
  qname="PaperPlane_$(printf '%s' "$mm" | tr -c 'A-Za-z0-9._-' '_' | cut -c1-40)"
  [ "$qname" = "PaperPlane_" ] && qname="PaperPlane_$RANDOM"

  local ppd="$SYS_PPD_DIR/$fam.ppd"
  [ -f "$ppd" ] || ppd="$PPD_SRC/$fam.ppd"

  if [ "$DRY" = 1 ]; then
    dim "[dry-run] lpadmin -p $qname -v $uri -P $ppd"
    return 0
  fi

  lpadmin -x "$qname" >/dev/null 2>&1
  if lpadmin -p "$qname" -E -v "$uri" -P "$ppd" \
        -D "纸飞机 · $mm" -o printer-is-shared=false >/dev/null 2>&1; then
    write_queue_config "$qname" "$fam" "$r_dpi" "$r_w"
    ok "$mm → ${qname}（${fam}，依据：${how}，${r_dpi}dpi/${r_w}mm）"
    printf '{"queue":"%s","uri":"%s","make_model":"%s","device_id":"%s","family":"%s","via":"%s","at":"%s"}\n' \
      "$qname" "$uri" "$mm" "$did" "$fam" "$how" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
      >> "$STATE_DIR/provenance.jsonl" 2>/dev/null || true
    return 0
  else
    bad "建队列失败：$mm"
    dim "常见原因：设备被别的驱动独占、USB 权限、或 URI 已失效。"
    return 1
  fi
}

_note_for() {
  local low
  low="$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')"
  while IFS=$'\t' read -r f_re _re _d _w _c _e _s note; do
    case "$f_re" in unsupported) ;; *) continue ;; esac
    if printf '%s' "$low" | grep -qE "$_re" 2>/dev/null; then echo "$note"; return; fi
  done < "$RULES"
  echo "私有协议"
}

mkdir -p "$STATE_DIR" 2>/dev/null || true

# 手动指定族（仅当用户查过规格书、明确知道该机型指令集时使用）
if [ -n "$FORCE_FAMILY" ] && [ -n "$FORCE_URI" ]; then
  say "手动绑定"
  case "$FORCE_FAMILY" in
    tspl_label|zpl_label|escpos_receipt|escpos_label) ;;
    *) bad "未知指令集族：${FORCE_FAMILY}（可选 tspl_label / zpl_label / escpos_receipt / escpos_label）"; exit 2 ;;
  esac
  qname="PaperPlane_$(printf '%s' "$FORCE_URI" | sed 's|.*/||; s|?.*||' | tr -c 'A-Za-z0-9._-' '_')"
  ppd="$SYS_PPD_DIR/$FORCE_FAMILY.ppd"; [ -f "$ppd" ] || ppd="$PPD_SRC/$FORCE_FAMILY.ppd"
  if run lpadmin -p "$qname" -E -v "$FORCE_URI" -P "$ppd" -D "纸飞机 · 手动指定 $FORCE_FAMILY"; then
    write_queue_config "$qname" "$FORCE_FAMILY" 203 104
    ok "已建队列 ${qname}（${FORCE_FAMILY}，由用户指定；dpi 请用 --bind-family 后手动确认）"
  fi
fi

if [ -z "$FORCE_FAMILY" ]; then
  say "为每台打印机搭桥"
  n_ok=0; n_skip=0; n_bad=0
  while IFS=$'\t' read -r uri mm did; do
    [ -n "${uri:-}" ] || continue
    bind_one "$uri" "${mm:-}" "${did:-}"
    case $? in
      0) n_ok=$((n_ok+1)) ;;
      2|3|4) n_skip=$((n_skip+1)) ;;
      *) n_bad=$((n_bad+1)) ;;
    esac
  done < "$DEVFILE"
  dim "结果：可用 $n_ok · 跳过 $n_skip · 失败 $n_bad"
fi

# ── 5. 可选：把 100 种纸型写进系统 ────────────────────────────────────
if [ "$WANT_PAPERS" = 1 ] && [ "$DRY" = 0 ]; then
  say "写入系统自定义纸张"
  if command -v python3 >/dev/null 2>&1; then
    "$HERE/scripts/install_papers.py" --apply 2>&1 | tail -5
  else
    warn "本机没有 python3，跳过（不影响打印：纸型也可以直接在打印对话框里输尺寸）"
  fi
fi

# ── 6. 收尾自检 ───────────────────────────────────────────────────────
say "自检"
if [ "$DRY" = 1 ]; then
  dim "[dry-run] 跳过"
else
  if [ -x "$SYS_DIR/paperplane" ] || [ -x "$BIN_SRC" ]; then
    E="$SYS_DIR/paperplane"; [ -x "$E" ] || E="$BIN_SRC"
    V="$("$E" version 2>/dev/null)"
    [ "$V" = "$VERSION" ] && ok "驱动版本 $V" || warn "驱动版本异常：$V"
  fi

  warnq=0
  for q in $(lpstat -a 2>/dev/null | awk '{print $1}'); do
    case "$q" in PaperPlane_*) ;; *) continue ;; esac
    if lpstat -p "$q" 2>&1 | grep -q "不安全权限\|insecure-filter"; then
      warnq=$((warnq+1))
    fi
  done
  [ "$warnq" = 0 ] && ok "队列权限正常（cupsd 可执行 filter）" \
                   || warn "$warnq 个队列仍有权限告警，请重新运行安装器"
fi

say "完成"
cat <<EOF

  以后打印，只要在打印对话框的「打印机」里选：

EOF
if [ "$DRY" = 0 ]; then
  lpstat -a 2>/dev/null | awk '/^PaperPlane_/ {printf "      %s\n", $1}'
fi
cat <<EOF

  · 标签 / 面单 / 小票 → 选 PaperPlane_ 开头的那台
  · A4 / 照片 / 信封   → 照旧选你自己的 Epson / HP / Canon

  换一台新打印机：插上 USB，重新运行本安装器即可，它会自动认出型号。
  查看诊断：$SYS_DIR/paperplane version
  卸载：bash "$HERE/uninstall.sh"

  本次安装没有打印任何纸张。
EOF
