#!/bin/bash
# 构建：生成 PPD + 编译 arm64/x86_64 通用二进制。只依赖系统 libcups。
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="$(grep -oE '"[0-9]+\.[0-9]+\.[0-9]+"' src/paperplane.c | head -1 | tr -d '"')"
[ -n "$VERSION" ] || { echo "读不到版本号"; exit 1; }
echo "版本：$VERSION"

python3 scripts/gen_ppd.py \
  --filter /Library/Printers/PaperPlane/paperplane \
  --version "$VERSION" \
  --out drivers

mkdir -p bin
clang -O2 -Wall -Wextra \
  -arch arm64 -arch x86_64 \
  -o bin/paperplane src/paperplane.c -lcups

echo "架构：$(lipo -archs bin/paperplane)"
echo "体积：$(du -h bin/paperplane | cut -f1)"
./bin/paperplane version
