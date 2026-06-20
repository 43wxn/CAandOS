#!/bin/bash
# ics2025 一键初始化 — 设置编译所需的环境变量
#
# 用法:
#   source init.sh          # 在当前 shell 中生效（推荐）
#   bash init.sh            # 仅打印，不生效

PROJ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 检查各子目录是否存在
check() {
  if [ ! -d "$PROJ_ROOT/$1" ]; then
    echo "⚠  $PROJ_ROOT/$1 不存在，请确认目录完整。" >&2
    return 1
  fi
}

check nemu             || exit 1
check abstract-machine || exit 1
check navy-apps        || exit 1

export NEMU_HOME="$PROJ_ROOT/nemu"
export AM_HOME="$PROJ_ROOT/abstract-machine"
export NAVY_HOME="$PROJ_ROOT/navy-apps"

echo "✔  环境变量已设置："
echo "    NEMU_HOME=$NEMU_HOME"
echo "    AM_HOME=$AM_HOME"
echo "    NAVY_HOME=$NAVY_HOME"
echo ""
echo "现在可以去子目录编译运行了，比如："
echo "    cd nanos-lite && make ARCH=riscv32-nemu run"
