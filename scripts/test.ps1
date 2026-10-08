# =============================================================================
#  各模块测试一键跑 —— 直接用 C/C++ 编译器构建运行，免 CMake，自动发现模块
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

$cxx = if ([string]::IsNullOrWhiteSpace($env:CXX)) { "g++" } else { $env:CXX }
$cc  = if ([string]::IsNullOrWhiteSpace($env:CC))  { "gcc" } else { $env:CC }
if (-not (Get-Command $cxx -ErrorAction SilentlyContinue)) {
    Write-Host ("找不到 C++ 编译器 {0}，请检查 CXX/PATH。" -f $cxx) -ForegroundColor Red
    exit 1
}

$bin = Join-Path $root ".testbin"
New-Item -ItemType Directory -Force -Path $bin | Out-Null
$solDir = Join-Path $root ".tmp_sol"      # 参考答案目录（-Sol 时优先于骨架）
$testTimeoutSeconds = 30                   # 每个测试进程独立限时，防止错误并发代码永久挂住。

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
    $cppSrcs = @(Get-ChildItem (Join-Path $m.FullName "tests\*.cpp") | ForEach-Object { $_.FullName })
    $cSrcs = @()
    # 实现源：有些模块（如 D5 pimpl）把实现放在 src/*.cpp 里，需要一并编译。
    # -Sol 时，若 .tmp_sol/ 下有同名 .cpp（参考答案），用它替换骨架实现。
    $srcDir = Join-Path $m.FullName "src"
    if (Test-Path $srcDir) {
        $cppSrcs += @(Get-ChildItem (Join-Path $srcDir "*.cpp") | ForEach-Object {
            $cand = Join-Path $solDir $_.Name
            if ($Sol -and (Test-Path $cand)) { $cand } else { $_.FullName }
        })
        # 真正的 C 翻译单元必须由 CC 以 C11 单独编译，不能交给 g++ 当作 C++。
        $cSrcs += @(Get-ChildItem (Join-Path $srcDir "*.c") | ForEach-Object {
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

    # 编译器 / 测试程序会往 stderr 写诊断（如告警）。当输出被捕获或重定向（CI、管道）时，
    # $ErrorActionPreference=Stop 会把原生 stderr 当成「终止性错误」而中断脚本。这里临时降为
    # Continue，并把 g++ 的 stderr 并入正常输出按纯文本打印，仅凭 $LASTEXITCODE 判定成败。
    $savedEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    $cObjects = @()
    $cCompiledOk = $true
    if ($cSrcs.Count -gt 0 -and -not (Get-Command $cc -ErrorAction SilentlyContinue)) {
        Write-Host ("[{0}] 含 .c 源，但找不到 C 编译器 {1}（请检查 CC/PATH）" -f $name, $cc) `
            -ForegroundColor Red
        $cCompiledOk = $false
    }
    if ($cCompiledOk) {
        foreach ($cSrc in $cSrcs) {
            $base = [System.IO.Path]::GetFileNameWithoutExtension($cSrc)
            $cObject = Join-Path $bin ("{0}__{1}.c.o" -f $name, $base)
            & $cc -std=c11 -O0 -g $incs -Wall -Wextra -c $cSrc -o $cObject 2>&1 | ForEach-Object { Write-Host $_ }
            if ($LASTEXITCODE -ne 0) {
                $cCompiledOk = $false
                break
            }
            $cObjects += $cObject
        }
    }
    if (-not $cCompiledOk) {
        $ErrorActionPreference = $savedEAP
        Write-Host ("[{0}] C 源编译失败" -f $name) -ForegroundColor Red
        [void]$results.Add([pscustomobject]@{ Module = $name; Status = "编译失败"; Ok = $false })
        continue
    }

    & $cxx -std=c++20 -O0 -g $incs -Wall -Wextra -pthread $cppSrcs $cObjects -o $exe 2>&1 | ForEach-Object { Write-Host $_ }
    $compiledOk = ($LASTEXITCODE -eq 0)
    if (-not $compiledOk) {
        $ErrorActionPreference = $savedEAP
        Write-Host ("[{0}] 编译失败" -f $name) -ForegroundColor Red
        [void]$results.Add([pscustomobject]@{ Module = $name; Status = "编译失败"; Ok = $false })
        continue
    }
    $ranOk = $false
    $timedOut = $false
    try {
        # 直接继承当前控制台，保留测试的实时输出；WaitForExit 提供进程级超时。
        $process = Start-Process -FilePath $exe -WorkingDirectory $root `
            -NoNewWindow -PassThru -ErrorAction Stop
        if (-not $process.WaitForExit($testTimeoutSeconds * 1000)) {
            $timedOut = $true
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            [void]$process.WaitForExit(2000) # 即使终止失败，脚本本身也不再无界等待。
            Write-Host ("[{0}] 测试超时（{1} 秒），已终止" -f $name, $testTimeoutSeconds) `
                -ForegroundColor Red
        } else {
            $ranOk = ($process.ExitCode -eq 0)
        }
    } catch {
        Write-Host ("[{0}] 无法运行测试：{1}" -f $name, $_.Exception.Message) -ForegroundColor Red
    } finally {
        $ErrorActionPreference = $savedEAP
    }
    if ($ranOk) {
        [void]$results.Add([pscustomobject]@{ Module = $name; Status = "通过 ✓"; Ok = $true })
    } elseif ($timedOut) {
        [void]$results.Add([pscustomobject]@{ Module = $name; Status = "超时 ✗"; Ok = $false })
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
