$root    = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$msbuild = & (Join-Path $PSScriptRoot 'find_msbuild.ps1')
New-Item -ItemType Directory -Force -Path (Join-Path $root 'logs') | Out-Null
$sln     = Join-Path $root 'build\KnC.sln'
$log     = Join-Path $root 'logs\msbuild_engine_tools.log'

# build just engine and tools targets server tests and client are broken
& $msbuild $sln /t:KnCEngine,dat_manager,map_viewer,model_viewer,ui_editor /p:Configuration=Release /p:Platform=x64 /nologo /m 2>&1 | Out-File -Encoding UTF8 $log
Write-Host "Exit code: $LASTEXITCODE"
