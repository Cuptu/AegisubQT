# AegisubQT portable package build script.
# Mirrors the upstream Aegisub packages/win_installer/portable/create-portable.ps1:
# deploys the Qt runtime alongside the app payload, marks the tree as portable
# (portable.txt keeps all settings in a local ini) and zips it up.
#
# Output: <build>\AegisubQT-<version>-portable.zip

param (
    [Parameter(Position = 0)]
    [string]$BuildRoot = "",
    [Parameter(Position = 1)]
    [string]$Version = "4.0.1"
)

$ErrorActionPreference = 'Stop'

# $PSScriptRoot is unavailable inside param default values on Windows PowerShell 5.1
if (!$BuildRoot) { $BuildRoot = Join-Path $PSScriptRoot "..\..\..\build" }

$BuildRoot = Resolve-Path $BuildRoot
$SourceRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$StagingDir = Join-Path $BuildRoot "AegisubQT-portable"
$QmlSourceDir = Join-Path $SourceRoot "qml"
$ExePath = Join-Path $BuildRoot "AegisubQT.exe"

if (!(Test-Path $ExePath)) {
    throw "Build output not found: $ExePath (run the CMake build first)"
}

$PortableZipPath = Join-Path $BuildRoot "AegisubQT-$Version-portable.zip"

Write-Host "BUILD_ROOT=$BuildRoot"
Write-Host "STAGING_DIR=$StagingDir"

Write-Host "[1/5] Preparing staging directory"
Remove-Item -LiteralPath $StagingDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $StagingDir | Out-Null

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
    throw "windeployqt.exe not found on PATH"
}
& $windeployqt --release --compiler-runtime --no-opengl-sw --qmldir "$QmlSourceDir" --dir "$StagingDir" (Join-Path $StagingDir "AegisubQT.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed (exit $LASTEXITCODE)" }

# Prune standalone VC redistributable installers deployed by windeployqt; app-local CRT DLLs are deployed directly below.
Remove-Item -Path (Join-Path $StagingDir "vc_redist*.exe") -Force -ErrorAction SilentlyContinue

$needed = @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")
$missing = @($needed | Where-Object { !(Test-Path (Join-Path $StagingDir $_)) })
if ($missing.Count -gt 0) {
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
    foreach ($dll in $missing) { Copy-Item (Join-Path $crtDir $dll) -Destination $StagingDir }
}

Write-Host "[4/5] Writing portable marker"
# main.cpp keeps all QSettings in a local ini when this marker sits next to the exe.
Set-Content -LiteralPath (Join-Path $StagingDir "portable.txt") -Value "AegisubQT portable mode"

Write-Host "[5/5] Creating portable zip"
Remove-Item -LiteralPath $PortableZipPath -Force -ErrorAction SilentlyContinue

# Build the zip in a way that avoids some PowerShell versions emitting
# backslashes in the entry names (same trick as upstream).
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipRoot = Split-Path $StagingDir -Leaf
$baseLen = $StagingDir.Length + 1
$zip = [System.IO.Compression.ZipFile]::Open($PortableZipPath, 'Create')
try {
    foreach ($file in Get-ChildItem -LiteralPath $StagingDir -Recurse -File) {
        $entryName = "$zipRoot/" + $file.FullName.Substring($baseLen).Replace('\', '/')
        [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $entryName, [System.IO.Compression.CompressionLevel]::Optimal)
    }
}
finally {
    $zip.Dispose()
}

Write-Host "Done: $PortableZipPath"
