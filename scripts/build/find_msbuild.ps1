# returns the MSBuild for the v143 toolset VS 2022 first any edition then newer via vswhere
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere not found, install Visual Studio 2022 or later" }
$path = & $vswhere -products * -version '[17.0,18.0)' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $path) {
    $path = & $vswhere -latest -products * -version '[17.0,)' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
}
if (-not $path) { throw "no Visual Studio 2022 or later with MSBuild found" }
$path
