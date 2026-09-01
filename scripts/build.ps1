# build-unit.ps1 — 単元コードのビルド・実行(MSVC環境の自動設定つき)
#
# 用途: 教科書の各単元のコードをCLIでビルド・実行する。コードファースト原則(AGENTS.md)の実行手段。
#
# なぜこのスクリプトが要るのか:
#   Visual Studio 18 (2026) の vcvars64.bat は、この環境で INCLUDE / LIB に Windows SDK を
#   追加しない。そのため cl.exe が kernel32.lib を見つけられず LNK1104 で落ち、rc.exe も見つからない。
#   さらに vswhere.exe が PATH に無い。毎回この回避策を手で足すのは事故のもとなので、
#   検出と設定をここへ封じ込める(generation-plan.md §11-C)。
#
# 使い方:
#   powershell -ExecutionPolicy Bypass -File scripts\build-unit.ps1 -Path code\solutions\unit-00-01 -Run
#   powershell -ExecutionPolicy Bypass -File scripts\build-unit.ps1 -Path code\unit-00-00 -Run
#   powershell -ExecutionPolicy Bypass -File scripts\build-unit.ps1 -Path engine -CMake -Run
#   powershell -ExecutionPolicy Bypass -File scripts\build-unit.ps1 -Source $env:TEMP\break\main.cpp
#
#   -Path    ビルド対象のディレクトリ。CMakeLists.txt があれば CMake、無ければ *.cpp を直接 cl でビルド
#   -Run     ビルド成功後に実行ファイルを起動する
#   -CMake   CMakeLists.txt があってもなくても CMake を強制する
#   -Env     環境変数の設定だけ行い、検出結果を表示して終了する(調査用)
#   -Source  .cpp を1本だけビルドする(-Path より優先。CMakeLists.txt があっても無視する)
#
# -Source は「わざと壊して実測する」ための口である(style-guide §1.5-③ / 検査S15)。
#   * -Path のフォルダ一括ビルドでは、壊した複製を同じフォルダへ置くと**両方**がビルドされて
#     出力が混ざる。壊した1本だけを狙うために -Source を使う
#   * コンパイラが吐く生の出力を `===== 生の出力ここから/ここまで =====` で挟んで表示する。
#     証跡ファイル(logs/build-evidence/)へは、この挟まれた範囲を**そのまま**貼ること
#   * 失敗しても [build-unit] のエラー扱いにしない(失敗させるのが目的のため)。終了コードは報告する
#   * ソースのあるフォルダで `cl <ファイル名>` を実行する。読者がIDEで見るのと同じ表示にするため、
#     フルパスではなくファイル名だけを渡している。**中間ファイル(.obj/.exe)はソースの隣に出る**ので、
#     壊した測定は `%TEMP%` へ複製してから行うこと。そのときファイル名は本文と同じ名前のままにする
#
# 注意: このファイルはBOM付きUTF-8で保存すること(PowerShell 5.1の制約)

param(
    [string]$Path = ".",
    [string]$Source = "",
    [switch]$Run,
    [switch]$CMake,
    [switch]$Env
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Fail($msg) {
    Write-Output "[build-unit] エラー: $msg"
    exit 1
}

# --- 1. Visual Studio のインストール先を探す ---------------------------------

$vsRoot = $null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $found = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($found) { $vsRoot = $found.Trim() }
}
if (-not $vsRoot) {
    # vswhere が無い / 何も返さない場合は既定の場所を新しい順に走査する
    $candidates = @()
    foreach ($base in @("$env:ProgramFiles\Microsoft Visual Studio", "${env:ProgramFiles(x86)}\Microsoft Visual Studio")) {
        if (Test-Path $base) {
            foreach ($ver in (Get-ChildItem $base -Directory | Sort-Object Name -Descending)) {
                foreach ($ed in @("Enterprise", "Professional", "Community", "BuildTools")) {
                    $p = Join-Path $ver.FullName $ed
                    if (Test-Path (Join-Path $p "VC\Tools\MSVC")) { $candidates += $p }
                }
            }
        }
    }
    if ($candidates.Count -gt 0) { $vsRoot = $candidates[0] }
}
if (-not $vsRoot) { Fail "Visual Studio (C++ツール入り) が見つかりません。" }

# --- 2. MSVC ツールセットの版を選ぶ(最新) -----------------------------------

$msvcBase = Join-Path $vsRoot "VC\Tools\MSVC"
$msvcDir = Get-ChildItem $msvcBase -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $msvcDir) { Fail "MSVCツールセットが $msvcBase に見つかりません。" }
$MSVC = $msvcDir.FullName
$clExe = Join-Path $MSVC "bin\Hostx64\x64\cl.exe"
if (-not (Test-Path $clExe)) { Fail "cl.exe が見つかりません: $clExe" }

# --- 3. Windows SDK の版を選ぶ(最新。Include と Lib の両方が揃うもの) -------

$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
if (-not (Test-Path "$sdkRoot\Include")) { Fail "Windows SDK が $sdkRoot に見つかりません。" }
$sdkVer = $null
foreach ($v in (Get-ChildItem "$sdkRoot\Include" -Directory | Sort-Object Name -Descending)) {
    if ((Test-Path "$sdkRoot\Include\$($v.Name)\um") -and (Test-Path "$sdkRoot\Lib\$($v.Name)\um\x64")) {
        $sdkVer = $v.Name
        break
    }
}
if (-not $sdkVer) { Fail "Include と Lib が揃った Windows SDK が見つかりません。" }

# --- 4. 環境変数を組み立てる(vcvars64.batの代わり) --------------------------

$SDKI = "$sdkRoot\Include\$sdkVer"
$SDKL = "$sdkRoot\Lib\$sdkVer"
$SDKB = "$sdkRoot\bin\$sdkVer\x64"

$env:INCLUDE = "$MSVC\include;$SDKI\ucrt;$SDKI\um;$SDKI\shared;$SDKI\winrt;$SDKI\cppwinrt"
$env:LIB     = "$MSVC\lib\x64;$SDKL\ucrt\x64;$SDKL\um\x64"

$pathAdds = @("$MSVC\bin\Hostx64\x64", $SDKB)
$cmakeDir = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if (Test-Path $cmakeDir) { $pathAdds += $cmakeDir }
$ninjaDir = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
if (Test-Path $ninjaDir) { $pathAdds += $ninjaDir }
$env:PATH = ($pathAdds -join ";") + ";" + $env:PATH

Write-Output "[build-unit] Visual Studio : $vsRoot"
Write-Output "[build-unit] MSVC toolset  : $($msvcDir.Name)"
Write-Output "[build-unit] Windows SDK   : $sdkVer"

if ($Env) {
    Write-Output "[build-unit] INCLUDE = $env:INCLUDE"
    Write-Output "[build-unit] LIB     = $env:LIB"
    exit 0
}

# --- 5-0. -Source: .cpp を1本だけビルドする(壊して実測する用) ---------------

if ($Source) {
    if (-not (Test-Path $Source)) { Fail "ソースが見つかりません: $Source" }
    $srcPath = (Resolve-Path $Source).Path
    $srcDir  = Split-Path $srcPath -Parent
    $srcName = Split-Path $srcPath -Leaf
    $srcBase = [System.IO.Path]::GetFileNameWithoutExtension($srcName)

    Write-Output "[build-unit] 単一ソース   : $srcPath"
    Write-Output "[build-unit] コマンド     : cl /nologo /utf-8 /EHsc /std:c++17 $srcName"
    Write-Output "[build-unit] ===== 生の出力ここから ====="
    Push-Location $srcDir
    try {
        & cl /nologo /utf-8 /EHsc /std:c++17 $srcName
        $code = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    Write-Output "[build-unit] ===== 生の出力ここまで (終了コード $code) ====="

    if ($code -ne 0) {
        Write-Output "[build-unit] ビルドは失敗した。壊して測っているなら、これが期待どおりの結果である。"
        exit $code
    }

    $exePath = Join-Path $srcDir "$srcBase.exe"
    if (-not (Test-Path $exePath)) { Fail "ビルドは通ったが $exePath が見つかりません。" }
    Write-Output "[build-unit] ビルド成功   : $exePath"
    if ($Run) {
        Write-Output "[build-unit] ----- 実行結果 -----"
        & $exePath
        $runCode = $LASTEXITCODE
        Write-Output "[build-unit] ----- 終了コード: $runCode -----"
        if ($runCode -ne 0) { exit $runCode }
    }
    exit 0
}

# --- 5. ビルドする -----------------------------------------------------------

$target = Resolve-Path $Path
Write-Output "[build-unit] 対象         : $target"

$hasCMakeLists = Test-Path (Join-Path $target "CMakeLists.txt")

if ($CMake -or $hasCMakeLists) {
    if (-not $hasCMakeLists) { Fail "-CMake が指定されましたが $target に CMakeLists.txt がありません。" }
    $buildDir = Join-Path $target "build"
    Write-Output "[build-unit] CMake構成 -> $buildDir"
    # Visual Studio の「フォルダーを開く」と同じ Ninja 構成を使う。
    # 生成方式を省略すると、この環境では未導入の Visual Studio ジェネレーターが選ばれる。
    & cmake -S $target -B $buildDir -G Ninja
    if ($LASTEXITCODE -ne 0) { Fail "CMakeの構成に失敗しました (exit $LASTEXITCODE)" }
    & cmake --build $buildDir
    if ($LASTEXITCODE -ne 0) { Fail "ビルドに失敗しました (exit $LASTEXITCODE)" }
    $exe = Get-ChildItem $buildDir -Recurse -Filter *.exe -ErrorAction SilentlyContinue |
           Sort-Object LastWriteTime -Descending | Select-Object -First 1
} else {
    $sources = Get-ChildItem $target -Filter *.cpp -File
    if ($sources.Count -eq 0) { Fail "$target に CMakeLists.txt も .cpp もありません。" }
    $outDir = Join-Path $target "build"
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    $exeName = (Split-Path $target -Leaf) + ".exe"
    $exePath = Join-Path $outDir $exeName
    Write-Output "[build-unit] cl /utf-8 /EHsc $($sources.Name -join ' ') -> $exePath"
    Push-Location $outDir
    try {
        & cl /nologo /utf-8 /EHsc /std:c++17 ($sources.FullName) /Fe:$exePath
        $code = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($code -ne 0) { Fail "コンパイルに失敗しました (exit $code)" }
    $exe = Get-Item $exePath
}

if (-not $exe) { Fail "ビルドは通りましたが実行ファイルが見つかりません。" }
Write-Output "[build-unit] ビルド成功   : $($exe.FullName)"

# --- 6. 実行する -------------------------------------------------------------

if ($Run) {
    Write-Output "[build-unit] ----- 実行結果 -----"
    & $exe.FullName
    $runCode = $LASTEXITCODE
    Write-Output "[build-unit] ----- 終了コード: $runCode -----"
    if ($runCode -ne 0) { exit $runCode }
}

exit 0
