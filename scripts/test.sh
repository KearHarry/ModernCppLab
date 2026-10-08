#!/usr/bin/env bash
# =============================================================================
#  各模块测试一键跑 —— 直接用 C/C++ 编译器构建运行，免 CMake，自动发现模块
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
CC="${CC:-gcc}"
bin="$root/.testbin"; mkdir -p "$bin"
solDir="$root/.tmp_sol"
test_timeout_seconds="${TEST_TIMEOUT_SECONDS:-30}"

if ! [[ "$test_timeout_seconds" =~ ^[1-9][0-9]*$ ]]; then
    echo "TEST_TIMEOUT_SECONDS 必须是正整数。" >&2
    exit 2
fi

# GNU/Linux 通常有 timeout；macOS 安装 coreutils 后命令名是 gtimeout。
# 两者都没有时优先用系统 Perl 的 POSIX alarm，最后退回 Bash watchdog。
run_with_timeout() {
    local seconds="$1"
    shift

    if command -v timeout >/dev/null 2>&1; then
        timeout -k 2s "${seconds}s" "$@"
        return $?
    fi
    if command -v gtimeout >/dev/null 2>&1; then
        gtimeout -k 2s "${seconds}s" "$@"
        return $?
    fi
    if command -v perl >/dev/null 2>&1; then
        perl -e 'my $seconds = shift @ARGV; alarm $seconds; exec { $ARGV[0] } @ARGV or die "exec: $!\n";' \
            "$seconds" "$@"
        local status=$?
        [ "$status" -eq 142 ] && return 124 # 128 + SIGALRM(14)
        return "$status"
    fi

    local flag="$bin/.timeout.$$.$RANDOM"
    "$@" &
    local test_pid=$!
    (
        sleep "$seconds"
        if kill -0 "$test_pid" 2>/dev/null; then
            : > "$flag"
            kill -TERM "$test_pid" 2>/dev/null || true
            sleep 1
            kill -KILL "$test_pid" 2>/dev/null || true
        fi
    ) &
    local watchdog_pid=$!
    local status
    wait "$test_pid"; status=$?
    kill "$watchdog_pid" 2>/dev/null || true
    wait "$watchdog_pid" 2>/dev/null || true
    if [ -e "$flag" ]; then
        rm -f -- "$flag"
        return 124
    fi
    return "$status"
}

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
    cpp_srcs=(modules/"$m"/tests/*.cpp)
    c_srcs=()
    # 实现源：有些模块（如 D5 pimpl）把实现放在 src/*.cpp；--sol 时用 .tmp_sol/ 下的同名答案替换。
    if [ -d "modules/$m/src" ]; then
        for f in modules/"$m"/src/*.cpp; do
            [ -e "$f" ] || continue
            base="$(basename "$f")"
            if [ "$use_sol" -eq 1 ] && [ -e "$solDir/$base" ]; then cpp_srcs+=("$solDir/$base"); else cpp_srcs+=("$f"); fi
        done
        # .c 必须保持 C 语言身份：先由 CC/C11 生成对象，再交给 C++ 链接步骤。
        for f in modules/"$m"/src/*.c; do
            [ -e "$f" ] || continue
            base="$(basename "$f")"
            if [ "$use_sol" -eq 1 ] && [ -e "$solDir/$base" ]; then c_srcs+=("$solDir/$base"); else c_srcs+=("$f"); fi
        done
    fi
    incs=()
    [ "$use_sol" -eq 1 ] && incs+=(-I "$solDir")
    incs+=(-I common -I "modules/$m" -I "modules/$m/include")
    c_objects=()
    c_compile_ok=1
    for c_src in "${c_srcs[@]}"; do
        c_base="$(basename "${c_src%.c}")"
        c_object="$bin/${m}__${c_base}.c.o"
        if "$CC" -std=c11 -O0 -g "${incs[@]}" -Wall -Wextra -c "$c_src" -o "$c_object"; then
            c_objects+=("$c_object")
        else
            echo "[$m] C 源编译失败"
            c_compile_ok=0
            break
        fi
    done
    if [ "$c_compile_ok" -eq 1 ] && \
       "$CXX" -std=c++20 -O0 -g "${incs[@]}" -Wall -Wextra -pthread \
           "${cpp_srcs[@]}" "${c_objects[@]}" -o "$bin/$m.exe"; then
        if run_with_timeout "$test_timeout_seconds" "$bin/$m.exe"; then
            names+=("$m"); oks+=(1)
        else
            run_status=$?
            if [ "$run_status" -eq 124 ]; then
                echo "[$m] 测试超时（${test_timeout_seconds} 秒），已终止"
            fi
            names+=("$m"); oks+=(0)
        fi
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
