#!/usr/bin/env bash
# 一键：配置 + 编译 + 跑测试（Linux / macOS）
# 用法： ./scripts/build.sh            # 全部
#        ./scripts/build.sh A1         # 只跑名字含 A1 的测试
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

if [ ! -d build ]; then
    cmake -S . -B build
fi
cmake --build build -j

if [ "$#" -ge 1 ]; then
    ctest --test-dir build -R "$1" --output-on-failure
else
    ctest --test-dir build --output-on-failure
fi
