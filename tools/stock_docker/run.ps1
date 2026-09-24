# builds the stock client wine image and runs one scenario usage in docs tools README

param(
    [ValidateSet("boot", "lobby", "walk", "race", "refs", "factory", "paint", "mission", "antenna", "videocheck", "slotswap", "shell")]
    [string]$Scenario = "boot",
    [string]$User = "dock1",
    [string]$Pass = "dock1pwd",
    [string]$FallbackUser = "dock2",
    [string]$FallbackPass = "dock2pwd",
    [string]$Client = "",
    [string]$Out = "",
    [string]$Image = "knc-stock-wine",
    [string]$ServerHost = "host.docker.internal",
    [string]$Desktop = "1280x1024",
    [string]$CacheVolume = "knc-stock-cache",
    [int]$Retries = 3,
    [int]$RetryWait = 45,
    [string]$WineOverrides = "",
    [string]$DbContainer = "knc-mariadb",
    [string]$Network = "",
    [switch]$NoBuild
)

# docker writes its progress on stderr so a native call must never stop the script
$ErrorActionPreference = "Continue"
$tools = Split-Path -Parent $PSScriptRoot
$repo = Split-Path -Parent $tools
if ($Client -eq "") { $Client = Join-Path $repo "DevClient" }
if ($Out -eq "") { $Out = Join-Path $repo "reference\docker" }
$Client = (Resolve-Path $Client -ErrorAction Stop).Path

# the package client is never used here only the dev copy with the pilot dll
if ($env:KNC_PROTECTED_CLIENT -and $Client -like "$env:KNC_PROTECTED_CLIENT*") { throw "refusing $Client, point -Client at a copy like DevClient" }
if (-not (Test-Path (Join-Path $Client "KnC.exe"))) { throw "no KnC.exe in $Client" }
if (-not (Test-Path (Join-Path $Client "dinput8.dll"))) { throw "no pilot dinput8.dll in $Client" }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Out = (Resolve-Path $Out -ErrorAction Stop).Path

if (-not $NoBuild) {
    $t = Get-Date
    & docker build -t $Image -f (Join-Path $PSScriptRoot "Dockerfile") $tools
    if ($LASTEXITCODE -ne 0) { throw "docker build failed" }
    Write-Host ("image {0} ready in {1:N0} s" -f $Image, ((Get-Date) - $t).TotalSeconds)
}

# the Data stamp is file count bytes and newest write so a changed tree is copied again
$stampArgs = @()
if ($CacheVolume -ne "") {
    $n = 0; $len = 0L; $newest = 0L
    $data = New-Object IO.DirectoryInfo (Join-Path $Client "Data")
    if ($data.Exists) {
        foreach ($f in $data.EnumerateFiles("*", [IO.SearchOption]::AllDirectories)) {
            $n++; $len += $f.Length
            if ($f.LastWriteTimeUtc.Ticks -gt $newest) { $newest = $f.LastWriteTimeUtc.Ticks }
        }
        $stampArgs = @("-e", ("KNC_DATA_STAMP={0}-{1}-{2}" -f $n, $len, $newest))
    }
}

# a fresh driver is held on the licence tab so the tutorial flag and the licence class are set together
function Clear-TutorialGate {
    param([string[]]$Users)
    $envLines = & docker exec $DbContainer env 2>$null
    if (-not $envLines) { Write-Host "cannot reach $DbContainer to clear the licence tutorial flag"; return }
    $dbUser = ($envLines | Where-Object { $_ -like "MYSQL_USER=*" }) -replace "MYSQL_USER=", ""
    $dbPass = ($envLines | Where-Object { $_ -like "MYSQL_PASSWORD=*" }) -replace "MYSQL_PASSWORD=", ""
    $dbName = ($envLines | Where-Object { $_ -like "MYSQL_DATABASE=*" }) -replace "MYSQL_DATABASE=", ""
    if (-not $dbUser -or -not $dbPass -or -not $dbName) { return }
    $list = ($Users | Where-Object { $_ -ne "" } | ForEach-Object { "'$_'" }) -join ","
    if ($list -eq "") { return }
    $sql = "UPDATE characters c JOIN accounts a ON a.id = c.account_id " +
           "SET c.tutorial_completed = 1, c.license_class = GREATEST(c.license_class, 1) " +
           "WHERE a.username IN ($list) AND (c.tutorial_completed = 0 OR c.license_class = 0)"
    & docker exec $DbContainer mariadb "-u$dbUser" "-p$dbPass" -e $sql $dbName 2>$null
}

Clear-TutorialGate -Users @($User, $FallbackUser)

# exit 5 means the account was already in game exit 6 means the licence flag needs a database clear
$curUser = $User
$curPass = $Pass
for ($attempt = 1; $attempt -le ($Retries + 1); $attempt++) {
    $runId = Get-Date -Format "yyyyMMdd_HHmmss"
    $dockerArgs = @(
        "run", "--rm", "--name", ("knc-stock-{0}-{1}" -f $Scenario, $runId),
        "--add-host", "host.docker.internal:host-gateway",
        "--shm-size", "1g",
        "-e", "KNC_USER=$curUser", "-e", "KNC_PASS=$curPass", "-e", "KNC_RUN_ID=$runId",
        "-e", "KNC_HOST=$ServerHost", "-e", "KNC_DESKTOP=$Desktop",
        "--mount", "type=bind,source=$Client,target=/client-ro,readonly",
        "--mount", "type=bind,source=$Out,target=/out"
    )
    # the pak and the Data tree live in a named volume so later boots start in seconds
    if ($CacheVolume -ne "") { $dockerArgs += $stampArgs + @("--mount", "type=volume,source=$CacheVolume,target=/cache") }
    if ($WineOverrides -ne "") { $dockerArgs += @("-e", "KNC_DLLOVERRIDES=$WineOverrides") }
    # a throwaway server on its own docker network is reached by container name
    if ($Network -ne "") { $dockerArgs += @("--network", $Network) }
    if ($Scenario -eq "shell") { $dockerArgs += "-it" }
    $dockerArgs += @($Image, $Scenario)

    $t = Get-Date
    & docker @dockerArgs
    $rc = $LASTEXITCODE
    $dir = Join-Path $Out ("{0}_{1}" -f $runId, $Scenario)
    Write-Host ("scenario {0} attempt {1} user {2} exit {3} after {4:N0} s" -f $Scenario, $attempt, $curUser, $rc, ((Get-Date) - $t).TotalSeconds)
    if ($rc -ne 5 -and $rc -ne 6) { break }
    if ($attempt -le $Retries) {
        if ($rc -eq 6) {
            Write-Host "account $curUser created its driver but the licence gate held it, clearing the flag and retrying"
            Clear-TutorialGate -Users @($curUser)
        }
        # a fallback account skips the wait since it is a different login not the same busy one
        elseif ($curUser -eq $User -and $FallbackUser -ne "" -and $FallbackUser -ne $User) {
            Write-Host "account $curUser is in game elsewhere, trying $FallbackUser now"
            $curUser = $FallbackUser
            $curPass = $FallbackPass
        } else {
            Write-Host "account $curUser is in game elsewhere, trying $User again in $RetryWait s"
            $curUser = $User
            $curPass = $Pass
            Start-Sleep -Seconds $RetryWait
        }
    }
}

$summary = Join-Path $dir "summary.json"
if (Test-Path $summary) {
    $s = Get-Content $summary -Raw | ConvertFrom-Json
    Write-Host ("result {0}" -f $s.result)
    foreach ($m in $s.marks) { Write-Host ("  {0,-22} {1,7} s" -f $m[0], $m[1]) }
    foreach ($f in $s.fps) { Write-Host ("  fps {0,-10} {1}" -f $f.at, $f.fps) }
    foreach ($p in $s.shots) { if ($p.file) { Write-Host ("  {0}  stage {1}  {2}" -f $p.file, $p.stage, $p.method) } }
    if ($s.finished) { Write-Host ("  finish {0}" -f $s.finished) }
}
Write-Host "shots and logs in $dir"
exit $rc
