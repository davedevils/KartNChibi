$root    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$msbuild = & (Join-Path $PSScriptRoot 'find_msbuild.ps1')
New-Item -ItemType Directory -Force -Path (Join-Path $root 'logs') | Out-Null
$sln     = Join-Path $root 'build\tools\KnC_Tools.sln'
$log     = Join-Path $root 'logs\msbuild_knc_tools.log'

& $msbuild $sln /p:Configuration=Release /p:Platform=x64 /nologo /m 2>&1 | Out-File -Encoding UTF8 $log
Write-Host "Exit code: $LASTEXITCODE"
