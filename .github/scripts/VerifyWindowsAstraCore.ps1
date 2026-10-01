param([Parameter(Mandatory = $true)][string]$AppDirectory)
$ErrorActionPreference = 'Stop'
$appRoot = (Resolve-Path -LiteralPath $AppDirectory).Path
$runtime = Join-Path $appRoot 'assets/bin'
$tools = Join-Path $PSScriptRoot '../../third_party/astracore_runtime_tools'
& python (Join-Path $tools 'verify-runtime.py') --runtime-dir $runtime
if ($LASTEXITCODE -ne 0) { throw 'Complete deployed AstraCore verification failed' }
$work = Join-Path ([IO.Path]::GetTempPath()) ('AegisubQt media 核验 ' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$savedPath = $env:PATH
try {
    & python (Join-Path $tools 'make-test-inputs.py') $work
    if ($LASTEXITCODE -ne 0) { throw 'Media fixture generation failed' }
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $media = Join-Path $work 'media.mkv'
    & (Join-Path $runtime 'ffmpeg.exe') -nostdin -v error -y -framerate 10 -i (Join-Path $work 'frame%02d.png') -i (Join-Path $work 'tone.wav') -c:v libx264 -bf 3 -crf 18 -pix_fmt yuv420p -c:a pcm_s16le -shortest $media
    if ($LASTEXITCODE -ne 0) { throw 'Bundled FFmpeg fixture encoding failed' }
    & (Join-Path $appRoot 'astra_deployed_bridge_smoke.exe') $media
    if ($LASTEXITCODE -ne 0) { throw "Application media bridge validation failed: $LASTEXITCODE" }
} finally {
    $env:PATH = $savedPath
    # The generated absolute directory must stay inside the temporary root.
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $actual = [IO.Path]::GetFullPath($work)
    if (!$actual.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing cleanup outside the temporary directory'
    }
    Remove-Item -LiteralPath $actual -Recurse -Force
}
