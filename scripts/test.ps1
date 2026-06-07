# =============================================================================
#  各模块测试一键跑 —— 直接用 g++ 编译+运行，免 CMake 配置，自动发现模块
# -----------------------------------------------------------------------------
#  用法（在仓库任意位置执行）：
#    .\scripts\test.ps1                 # 跑所有「有测试」的模块（测你写的 include 代码）
#    .\scripts\test.ps1 A5              # 只跑名字含 A5 的模块
#    .\scripts\test.ps1 A               # 跑所有 A 系列
#    .\scripts\test.ps1 A5 C6 E1        # 跑多个
#    .\scripts\test.ps1 -Sol            # 改测参考答案（.tmp_sol/），用来确认答案全绿
#    .\scripts\test.ps1 -Sol A5         # 只确认 A5 的参考答案
#
#  退出码：全部通过 = 0，有失败 = 1（方便接 CI / && 链式判断）。
# =============================================================================
param(
    [switch]$Sol,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Filters
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# 测试输出含中文（UTF-8）；PowerShell 默认 GBK，先切到 UTF-8 避免乱码。
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
if ($env:TERM -ne 'dumb') { chcp 65001 | Out-Null }

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Host "找不到 g++，请确认 MinGW-w64 已在 PATH 中。" -ForegroundColor Red
    exit 1
}

$bin = Join-Path $root ".testbin"
New-Item -ItemType Directory -Force -Path $bin | Out-Null
$solDir = Join-Path $root ".tmp_sol"      # 参考答案目录（-Sol 时优先于骨架）

# ---- 发现所有「tests/ 下有 .cpp」的模块 ----
$mods = Get-ChildItem (Join-Path $root "modules") -Directory | Where-Object {
    Test-Path (Join-Path $_.FullName "tests\*.cpp")
} | Sort-Object Name

# ---- 按过滤词筛选（子串匹配，忽略大小写）----
if ($Filters) {
    $mods = $mods | Where-Object {
        $name = $_.Name
        @($Filters | Where-Object { $name -like "*$_*" }).Count -gt 0
    }
}

if (-not $mods) { Write-Host "没有匹配的模块。" -ForegroundColor Yellow; exit 0 }

$results = New-Object System.Collections.ArrayList
foreach ($m in $mods) {
    $name = $m.Name
    # 测试源：tests/*.cpp 必编。
    $srcs = @(Get-ChildItem (Join-Path $m.FullName "tests\*.cpp") | ForEach-Object { $_.FullName })
    # 实现源：有些模块（如 D5 pimpl）把实现放在 src/*.cpp 里，需要一并编译。
    # -Sol 时，若 .tmp_sol/ 下有同名 .cpp（参考答案），用它替换骨架实现。
    $srcDir = Join-Path $m.FullName "src"
    if (Test-Path $srcDir) {
        $srcs += @(Get-ChildItem (Join-Path $srcDir "*.cpp") | ForEach-Object {
            $cand = Join-Path $solDir $_.Name
            if ($Sol -and (Test-Path $cand)) { $cand } else { $_.FullName }
        })
    }
    $exe  = Join-Path $bin ("{0}.exe" -f $name)

    $tag = if ($Sol) { "  (参考答案)" } else { "" }
    Write-Host ""
    Write-Host ("==== {0}{1} ====" -f $name, $tag) -ForegroundColor Cyan

    # 头文件搜索路径：-Sol 时把 .tmp_sol 放最前，让答案覆盖同名骨架头。
    $incs = @()
    if ($Sol) { $incs += @("-I", $solDir) }
    $incs += @("-I", (Join-Path $root "common"),
               "-I", $m.FullName,
               "-I", (Join-Path $m.FullName "include"))

    # g++ / 测试程序会往 stderr 写诊断（如告警）。当输出被捕获或重定向（CI、管道）时，
    # $ErrorActionPreference=Stop 会把原生 stderr 当成「终止性错误」而中断脚本。这里临时降为
    # Continue，并把 g++ 的 stderr 并入正常输出按纯文本打印，仅凭 $LASTEXITCODE 判定成败。
    $savedEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & g++ -std=c++20 -O0 -g $incs -Wall -Wextra -pthread $srcs -o $exe 2>&1 | ForEach-Object { Write-Host $_ }
    $compiledOk = ($LASTEXITCODE -eq 0)
    if (-not $compiledOk) {
        $ErrorActionPreference = $savedEAP
        Write-Host ("[{0}] 编译失败" -f $name) -ForegroundColor Red
        [void]$results.Add([pscustomobject]@{ Module = $name; Status = "编译失败"; Ok = $false })
        continue
    }
    & $exe
    $ranOk = ($LASTEXITCODE -eq 0)
    $ErrorActionPreference = $savedEAP
    if ($ranOk) {
        [void]$results.Add([pscustomobject]@{ Module = $name; Status = "通过 ✓"; Ok = $true })
    } else {
        [void]$results.Add([pscustomobject]@{ Module = $name; Status = "失败 ✗"; Ok = $false })
    }
}

# ---- 汇总 ----
Write-Host ""
Write-Host "================== 汇总 ==================" -ForegroundColor Cyan
foreach ($r in $results) {
    $c = if ($r.Ok) { "Green" } else { "Red" }
    Write-Host ("{0,-28} {1}" -f $r.Module, $r.Status) -ForegroundColor $c
}
$pass  = @($results | Where-Object { $_.Ok }).Count
$total = $results.Count
Write-Host ""
Write-Host ("模块合计: {0} | 通过: {1} | 失败: {2}" -f $total, $pass, ($total - $pass))
if ($pass -lt $total) { exit 1 } else { exit 0 }
