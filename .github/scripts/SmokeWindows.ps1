param(
    [Parameter(Mandatory = $true)][string]$AppDirectory,
    [ValidateRange(1, 30)][int]$Seconds = 5
)

$ErrorActionPreference = 'Stop'
$appDir = (Resolve-Path -LiteralPath $AppDirectory).Path
$exe = Join-Path $appDir 'AegisubQT.exe'
if (!(Test-Path -LiteralPath $exe) -or !(Test-Path -LiteralPath (Join-Path $appDir 'qml\Main.qml'))) {
    throw 'The staged payload must contain AegisubQT.exe and application qml/Main.qml.'
}
foreach ($dll in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $appDir $dll))) { throw "The staged x64 MSVC payload is missing $dll." }
}

# Use a fresh working directory with a hostile QML candidate. The application
# must load its own payload rather than this working-directory override.
$cwd = Join-Path ([IO.Path]::GetTempPath()) ('Aegisub smoke (isolated) ' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $cwd 'qml') | Out-Null
@'
import QtQml
QtObject {
    Component.onCompleted: {
        console.warn("AEGISUB_UNTRUSTED_CWD_QML_EXECUTED")
        Qt.quit()
    }
}
'@ | Set-Content -LiteralPath (Join-Path $cwd 'qml\Main.qml') -Encoding utf8

# A file association can choose an arbitrary working directory. It must not
# add that directory to either autoload or module/include discovery.
New-Item -ItemType Directory -Path (Join-Path $cwd 'automation\autoload') | Out-Null
New-Item -ItemType Directory -Path (Join-Path $cwd 'automation\include\moonscript') | Out-Null
New-Item -ItemType Directory -Path (Join-Path $cwd 'moonscript') | Out-Null
'print("AEGISUB_UNTRUSTED_CWD_AUTOMATION_EXECUTED")' | Set-Content -LiteralPath (Join-Path $cwd 'automation\autoload\hostile.lua') -Encoding ascii
'print("AEGISUB_UNTRUSTED_CWD_MODULE_EXECUTED"); error("untrusted module")' | Set-Content -LiteralPath (Join-Path $cwd 'automation\include\moonscript\init.lua') -Encoding ascii
'print("AEGISUB_UNTRUSTED_CWD_MODULE_EXECUTED"); error("untrusted module")' | Set-Content -LiteralPath (Join-Path $cwd 'moonscript\init.lua') -Encoding ascii

$savedEnv = @{}
foreach ($name in @('PATH', 'QT_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH', 'QT_QPA_PLATFORM', 'QT_QUICK_BACKEND')) {
    $savedEnv[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
$proc = $null
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = ''
    $env:QML_IMPORT_PATH = ''
    $env:QML2_IMPORT_PATH = ''
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_QUICK_BACKEND = 'software'
    $stderr = Join-Path $cwd 'stderr.log'
    $stdout = Join-Path $cwd 'stdout.log'
    $verify = Start-Process -FilePath $exe -ArgumentList '--verify-subtitle-renderer' -WorkingDirectory $cwd -WindowStyle Hidden -PassThru `
        -RedirectStandardError $stderr -RedirectStandardOutput $stdout
    if (!$verify.WaitForExit(30000)) { Stop-Process -Id $verify.Id; throw 'Subtitle renderer verification timed out.' }
    if ($verify.ExitCode -ne 0) {
        Get-Content -LiteralPath $stderr
        throw 'Packaged SRT import/libass rendering verification failed.'
    }
    $proc = Start-Process -FilePath $exe -WorkingDirectory $cwd -WindowStyle Hidden -PassThru `
        -RedirectStandardError $stderr -RedirectStandardOutput $stdout
    Start-Sleep -Seconds $Seconds
    $proc.Refresh()
    $text = (Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue) +
            (Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue)
    if ($proc.HasExited -or $text -notmatch 'QML loaded\. Root objects count: 1' -or
            $text -match 'AEGISUB_UNTRUSTED_CWD_(QML|AUTOMATION|MODULE)_EXECUTED') {
        Write-Output $text
        throw 'Staged application failed the isolated startup/QML/automation-origin check.'
    }
    Write-Output "PASS isolated deployed startup and working-directory QML/automation rejection: $appDir"
    Write-Output "Startup logs: $cwd"
} finally {
    if ($proc -and !$proc.HasExited) { Stop-Process -Id $proc.Id }
    foreach ($name in $savedEnv.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedEnv[$name], 'Process')
    }
}
