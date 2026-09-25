# AegisubQT Windows installer build script.
# Mirrors the upstream Aegisub release pipeline (tools/win-installer-setup.ps1),
# reduced to what AegisubQT ships: deployed Qt runtime, QML, assets, automation,
# compiled locale .qm files and the MSVC runtime (app-local, via windeployqt).
#
# Output: <build>\AegisubQT-<version>.exe (named like upstream's Aegisub-3.4.2.exe)

param (
    [Parameter(Position = 0)]
    [string]$BuildRoot = "",
    [Parameter(Position = 1)]
    [string]$Version = "4.0.0"
)

$ErrorActionPreference = 'Stop'

# $PSScriptRoot is unavailable inside param default values on Windows PowerShell 5.1
if (!$BuildRoot) { $BuildRoot = Join-Path $PSScriptRoot "..\..\build" }

$BuildRoot = Resolve-Path $BuildRoot
$SourceRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$StagingDir = Join-Path $BuildRoot "installer-staging"
$QmlSourceDir = Join-Path $SourceRoot "qml"
$ExePath = Join-Path $BuildRoot "AegisubQT.exe"

if (!(Test-Path $ExePath)) {
    throw "Build output not found: $ExePath (run the CMake build first)"
}

Write-Host "BUILD_ROOT=$BuildRoot"
Write-Host "STAGING_DIR=$StagingDir"

Write-Host "[1/5] Preparing staging directory"
Remove-Item -LiteralPath $StagingDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $StagingDir | Out-Null

# Inno Setup's Chinese wizard translations are unofficial and not shipped with
# the compiler; fetch them once (same source as upstream Aegisub's installer).
$LangsDir = Join-Path $PSScriptRoot "langs"
foreach ($lang in @("ChineseSimplified", "ChineseTraditional")) {
    $islPath = Join-Path $LangsDir "$lang.isl"
    if (!(Test-Path $islPath)) {
        New-Item -ItemType Directory -Path $LangsDir -Force | Out-Null
        $islUrl = "https://raw.github.com/jrsoftware/issrc/is-6_7_3/Files/Languages/Unofficial/$lang.isl"
        Invoke-WebRequest $islUrl -OutFile $islPath -UseBasicParsing
    }
}

Write-Host "[2/5] Copying application payload"
Copy-Item -LiteralPath $ExePath -Destination $StagingDir
foreach ($dir in @("qml", "assets", "automation")) {
    Copy-Item -Path (Join-Path $BuildRoot $dir) -Destination $StagingDir -Recurse
}
New-Item -ItemType Directory -Path (Join-Path $StagingDir "locale") | Out-Null
Copy-Item -Path (Join-Path $BuildRoot "*.qm") -Destination (Join-Path $StagingDir "locale")

Write-Host "[3/5] Deploying Qt runtime (windeployqt)"
$windeployqt = (Get-Command windeployqt.exe -ErrorAction SilentlyContinue).Source
if (!$windeployqt) {
    $qtBin = Split-Path (Split-Path (Get-ChildItem "$BuildRoot\CMakeCache.txt" -ErrorAction SilentlyContinue) -Parent) -Parent
    throw "windeployqt.exe not found on PATH"
}
& $windeployqt --release --compiler-runtime --no-opengl-sw --qmldir "$QmlSourceDir" --dir "$StagingDir" (Join-Path $StagingDir "AegisubQT.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed (exit $LASTEXITCODE)" }

# Prune standalone VC redistributable installers deployed by windeployqt; app-local CRT DLLs are deployed directly below.
Remove-Item -Path (Join-Path $StagingDir "vc_redist*.exe") -Force -ErrorAction SilentlyContinue

# --compiler-runtime normally copies the MSVC CRT next to the app; verify and
# fall back to a manual redist copy for toolchains that cannot locate it.
$needed = @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")
$missing = @($needed | Where-Object { !(Test-Path (Join-Path $StagingDir $_)) })
if ($missing.Count -gt 0) {
    Write-Host "[3/5] windeployqt did not provide VC runtime, searching redist manually"
    $redistRoots = @()
    if ($env:VCINSTALLDIR) { $redistRoots += (Join-Path $env:VCINSTALLDIR "Redist\MSVC") }
    $redistRoots += "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Redist\MSVC"
    $redistRoots += "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Redist\MSVC"
    $crtDir = $null
    foreach ($root in $redistRoots) {
        $hit = Get-Item -Path (Join-Path $root "*\x64\Microsoft.VC*.CRT") -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1
        if ($hit) { $crtDir = $hit.FullName; break }
    }
    if (!$crtDir) { throw "MSVC redist not found; install Visual Studio Build Tools or copy VC runtime DLLs manually" }
    Write-Host "[3/5] Using CRT from $crtDir"
    foreach ($dll in $missing) { Copy-Item (Join-Path $crtDir $dll) -Destination $StagingDir }
}

Write-Host "[4/5] Compiling installer (Inno Setup)"
$iscc = (Get-Command iscc.exe -ErrorAction SilentlyContinue).Source
if (!$iscc) {
    $isccCandidates = @(
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\iscc.exe",
        "C:\Program Files (x86)\Inno Setup 6\iscc.exe",
        "C:\Program Files\Inno Setup 6\iscc.exe"
    )
    $iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (!$iscc) { throw "Inno Setup not found; install with: winget install JRSoftware.InnoSetup" }
}
& $iscc "/DStagingDir=$StagingDir" "/DVersion=$Version" "/DSourceRoot=$SourceRoot" (Join-Path $PSScriptRoot "AegisubQT.iss")
if ($LASTEXITCODE -ne 0) { throw "iscc failed (exit $LASTEXITCODE)" }

$InstallerExe = Join-Path $BuildRoot "AegisubQT-$Version.exe"
Write-Host "[5/5] Done: $InstallerExe"
