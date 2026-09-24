$root    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$msbuild = & (Join-Path $PSScriptRoot 'find_msbuild.ps1')
New-Item -ItemType Directory -Force -Path (Join-Path $root 'logs') | Out-Null
$sln     = Join-Path $root 'build\KnC.sln'
$log     = Join-Path $root 'logs\msbuild_sln.log'
$console = Join-Path $root 'logs\msbuild_sln_console.log'

& $msbuild $sln /p:Configuration=Release /p:Platform=x64 /nologo /m "/flp:LogFile=$log;Encoding=UTF-8" 2>&1 | Out-File -Encoding UTF8 $console
Write-Host "Exit code: $LASTEXITCODE"
