# 一键：配置 + 编译 + 跑测试（Windows / MinGW）
# 用法： .\scripts\build.ps1            # 全部
#        .\scripts\build.ps1 A1         # 只跑名字含 A1 的测试
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# 测试输出含中文（UTF-8）；PowerShell 默认 GBK，先切到 UTF-8 避免乱码。
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
if ($env:TERM -ne 'dumb') { chcp 65001 | Out-Null }

if (-not (Test-Path "build")) {
    cmake -S . -B build -G "MinGW Makefiles"
}
cmake --build build -j

if ($args.Count -ge 1) {
    ctest --test-dir build -R $args[0] --output-on-failure
} else {
    ctest --test-dir build --output-on-failure
}
