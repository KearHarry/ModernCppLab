#!/usr/bin/env bash
# =============================================================================
#  各模块测试一键跑 —— 直接用 g++ 编译+运行，免 CMake 配置，自动发现模块
# -----------------------------------------------------------------------------
#  用法：
#    ./scripts/test.sh                  # 跑所有「有测试」的模块（测你写的 include 代码）
#    ./scripts/test.sh A5               # 只跑名字含 A5 的模块
#    ./scripts/test.sh A                # 跑所有 A 系列
#    ./scripts/test.sh A5 C6 E1         # 跑多个
#    ./scripts/test.sh --sol            # 改测参考答案（.tmp_sol/），确认答案全绿
#    ./scripts/test.sh --sol A5         # 只确认 A5 的参考答案
#
#  退出码：全部通过 = 0，有失败 = 1。
# =============================================================================
set -uo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

CXX="${CXX:-g++}"
bin="$root/.testbin"; mkdir -p "$bin"
solDir="$root/.tmp_sol"

use_sol=0
filters=()
for a in "$@"; do
    if [ "$a" = "--sol" ] || [ "$a" = "-s" ]; then use_sol=1; else filters+=("$a"); fi
done

# ---- 发现含测试的模块 ----
mods=()
for d in modules/*/; do
    if compgen -G "${d}tests/*.cpp" > /dev/null 2>&1; then mods+=("$(basename "$d")"); fi
done
IFS=$'\n' mods=($(sort <<<"${mods[*]}")); unset IFS

# ---- 过滤 ----
if [ "${#filters[@]}" -gt 0 ]; then
    sel=()
    for m in "${mods[@]}"; do
        for f in "${filters[@]}"; do
            if [[ "$m" == *"$f"* ]]; then sel+=("$m"); break; fi
        done
    done
    mods=("${sel[@]}")
fi

if [ "${#mods[@]}" -eq 0 ]; then echo "没有匹配的模块。"; exit 0; fi

names=(); oks=()
for m in "${mods[@]}"; do
    echo
    if [ "$use_sol" -eq 1 ]; then echo "==== $m  (参考答案) ===="; else echo "==== $m ===="; fi
    srcs=(modules/"$m"/tests/*.cpp)
    # 实现源：有些模块（如 D5 pimpl）把实现放在 src/*.cpp；--sol 时用 .tmp_sol/ 下的同名答案替换。
    if [ -d "modules/$m/src" ]; then
        for f in modules/"$m"/src/*.cpp; do
            [ -e "$f" ] || continue
            base="$(basename "$f")"
            if [ "$use_sol" -eq 1 ] && [ -e "$solDir/$base" ]; then srcs+=("$solDir/$base"); else srcs+=("$f"); fi
        done
    fi
    incs=()
    [ "$use_sol" -eq 1 ] && incs+=(-I "$solDir")
    incs+=(-I common -I "modules/$m" -I "modules/$m/include")
    if "$CXX" -std=c++20 -O0 -g "${incs[@]}" -Wall -Wextra -pthread "${srcs[@]}" -o "$bin/$m.exe"; then
        if "$bin/$m.exe"; then names+=("$m"); oks+=(1); else names+=("$m"); oks+=(0); fi
    else
        echo "[$m] 编译失败"; names+=("$m"); oks+=(0)
    fi
done

# ---- 汇总 ----
echo; echo "================== 汇总 =================="
pass=0
for i in "${!names[@]}"; do
    if [ "${oks[$i]}" -eq 1 ]; then printf "%-28s 通过 \xe2\x9c\x93\n" "${names[$i]}"; pass=$((pass+1));
    else printf "%-28s 失败 \xe2\x9c\x97\n" "${names[$i]}"; fi
done
total=${#names[@]}
echo; echo "模块合计: $total | 通过: $pass | 失败: $((total-pass))"
[ "$pass" -eq "$total" ]
