$root    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$msbuild = & (Join-Path $PSScriptRoot 'find_msbuild.ps1')
New-Item -ItemType Directory -Force -Path (Join-Path $root 'logs') | Out-Null
$base = Join-Path $root 'build\tools'
$log  = Join-Path $root 'logs\msbuild_tools_console.log'
$tools = @('dat_manager','map_viewer','model_viewer','ui_editor')

$all = @()
foreach ($t in $tools) {
    $proj = "$base\$t.vcxproj"
    $out = & $msbuild $proj /p:Configuration=Release /p:Platform=x64 /nologo /m 2>&1
    $all += "=== $t ==="
    $all += ($out | Select-String 'error C[0-9]|0 Erreur|1 Erreur|0 Error|1 Error|succeeded' | ForEach-Object { $_.ToString() })
}
$all | Out-File -Encoding UTF8 $log
Write-Host "Exit code: $LASTEXITCODE"
