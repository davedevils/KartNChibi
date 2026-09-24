$root    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$msbuild = & (Join-Path $PSScriptRoot 'find_msbuild.ps1')
New-Item -ItemType Directory -Force -Path (Join-Path $root 'logs') | Out-Null
$proj    = Join-Path $root 'build\engine\KnCEngine.vcxproj'
$log     = Join-Path $root 'logs\msbuild_full.log'
$console = Join-Path $root 'logs\msbuild_console.log'

& $msbuild $proj /p:Configuration=Release /p:Platform=x64 /nologo "/flp:LogFile=$log;Encoding=UTF-8" 2>&1 | Out-File -Encoding UTF8 $console
Write-Host "MSBuild exit code: $LASTEXITCODE"
