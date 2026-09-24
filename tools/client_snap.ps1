# Captures the stock KnC client window per stage change while someone plays it usage in docs client README

param(
    [string]$Out = "",
    [double]$Interval = 2.0,
    [int]$Keep = 5,
    [string]$Process = "KnC",
    [switch]$Once,
    [switch]$NoMemory
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
if ($Out -eq "") { $Out = Join-Path $repo "reference\stock" }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$pilot = Join-Path $repo "DevClient\knc-pilot.exe"

# the stage numbers of the stock state machine
$stageNames = @{
    1 = "logo"; 2 = "title"; 3 = "login"; 4 = "channel"; 5 = "menu"; 6 = "garage"; 7 = "shop"; 8 = "lobby"; 9 = "room";
    11 = "game"; 13 = "tutorial"; 14 = "licence"; 15 = "ghostrace"; 18 = "carfactory"; 19 = "roomeditor";
    22 = "scenario"; 23 = "ghostmode"; 24 = "missionmenu"; 25 = "missionrace"; 26 = "questmenu"
}
# the stage int of the stock exe at image base 0x400000 the pilot reads the same address
$stageAddress = [IntPtr]0x00B2360C
$dialogAddress = [IntPtr]0x00F727F0

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Snap {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr hdc);
    [DllImport("gdi32.dll")] public static extern bool BitBlt(IntPtr dst, int x, int y, int w, int h, IntPtr src, int sx, int sy, uint rop);
    [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr h, IntPtr address, byte[] buffer, int size, out int read);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
}
"@

function Get-Client {
    # an elevated copy hides its path and its memory so the copy we can read wins when both run
    $all = @(Get-Process -Name $Process -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 })
    $p = $all | Where-Object { try { $null -ne $_.Path } catch { $false } } | Select-Object -First 1
    if ($null -eq $p) { $p = $all | Select-Object -First 1 }
    return $p
}

# the pilot pipe first then the raw int at the stage address minus one when neither answers
function Get-Stage([System.Diagnostics.Process]$p) {
    if (Test-Path $pilot) {
        try {
            $reply = & $pilot dump 2>$null
            if ($LASTEXITCODE -eq 0 -and "$reply" -match "stage=(-?\d+)") { return [int]$Matches[1] }
        } catch { }
    }
    if ($NoMemory) { return -1 }
    $h = [Snap]::OpenProcess(0x0410, $false, $p.Id)
    if ($h -eq [IntPtr]::Zero) { return -1 }
    try {
        $buf = New-Object byte[] 4
        $read = 0
        if ([Snap]::ReadProcessMemory($h, $stageAddress, $buf, 4, [ref]$read) -and $read -eq 4) {
            return [BitConverter]::ToInt32($buf, 0)
        }
    } finally { [void][Snap]::CloseHandle($h) }
    return -1
}

# true when the bitmap is one flat colour a black print means the driver refused the print path
function Test-Flat([System.Drawing.Bitmap]$bmp) {
    $first = $bmp.GetPixel(1, 1)
    foreach ($x in @(($bmp.Width / 3), ($bmp.Width / 2), ($bmp.Width * 2 / 3))) {
        foreach ($y in @(($bmp.Height / 3), ($bmp.Height / 2), ($bmp.Height * 2 / 3))) {
            if ($bmp.GetPixel([int]$x, [int]$y) -ne $first) { return $false }
        }
    }
    return $true
}

# PrintWindow with the full content flag only a flat print is dropped a screen copy could show another window
function Capture-Window([IntPtr]$hwnd) {
    $rc = New-Object Snap+RECT
    if (-not [Snap]::GetClientRect($hwnd, [ref]$rc)) { return $null }
    $w = $rc.Right - $rc.Left
    $h = $rc.Bottom - $rc.Top
    if ($w -le 0 -or $h -le 0) { return $null }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    $ok = [Snap]::PrintWindow($hwnd, $hdc, 3)
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    if (-not $ok -or (Test-Flat $bmp)) {
        $bmp.Dispose()
        return $null
    }
    return @{ Bitmap = $bmp; Method = "printwindow" }
}

function Save-Snap([int]$stage, [hashtable]$shot) {
    $name = if ($stageNames.ContainsKey($stage)) { $stageNames[$stage] } else { "stage" }
    $prefix = "{0}_{1}_" -f $stage, $name
    $existing = @(Get-ChildItem -Path $Out -Filter ($prefix + "*.png") | Sort-Object Name)
    $n = 1
    if ($existing.Count -gt 0) {
        $last = $existing[-1].BaseName
        if ($last -match "_(\d+)$") { $n = [int]$Matches[1] + 1 }
    }
    $path = Join-Path $Out ("{0}{1:D3}.png" -f $prefix, $n)
    $shot.Bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $shot.Bitmap.Dispose()
    $all = @(Get-ChildItem -Path $Out -Filter ($prefix + "*.png") | Sort-Object Name)
    while ($all.Count -gt $Keep) {
        Remove-Item $all[0].FullName -Force
        $all = @(Get-ChildItem -Path $Out -Filter ($prefix + "*.png") | Sort-Object Name)
    }
    Write-Host ("{0}  stage {1} {2}  {3}x{4}  {5}" -f (Get-Date -Format "HH:mm:ss"), $stage, $name, $shot.Bitmap.Width, $shot.Bitmap.Height, $shot.Method)
    return $path
}

Write-Host "snapping $Process into $Out every $Interval s, Ctrl C stops"
$lastStage = -2
$lastShot = [DateTime]::MinValue
while ($true) {
    $p = Get-Client
    if ($null -eq $p) {
        if ($Once) { Write-Host "no $Process window"; break }
        Start-Sleep -Milliseconds 500
        continue
    }
    if ([Snap]::IsIconic($p.MainWindowHandle)) { Start-Sleep -Milliseconds 500; continue }
    $stage = Get-Stage $p
    $now = Get-Date
    $due = ($stage -ne $lastStage) -or (($now - $lastShot).TotalSeconds -ge $Interval)
    if ($due) {
        $saved = $null
        if ((Test-Path $pilot) -and $stage -ge 0) {
            # the pilot inside the client copies its own d3d back buffer the only clean capture of a d3d9 window
            $target = Join-Path $Out ("{0}_{1}_pilot.png" -f $stage, $(if ($stageNames.ContainsKey($stage)) { $stageNames[$stage] } else { "stage" }))
            $reply = & $pilot shot "$target d3d" 2>$null
            if ("$reply" -match "^ok") {
                $bmp = [System.Drawing.Bitmap]::FromFile($target)
                $copy = New-Object System.Drawing.Bitmap $bmp
                $bmp.Dispose()
                Remove-Item $target -ErrorAction SilentlyContinue
                $saved = Save-Snap $stage @{ Bitmap = $copy; Method = "pilot" }
            }
        }
        if ($null -eq $saved) {
            $shot = Capture-Window $p.MainWindowHandle
            if ($null -ne $shot) { $saved = Save-Snap $stage $shot }
        }
        if ($Once -and $saved) { Write-Host $saved; break }
        $lastStage = $stage
        $lastShot = $now
    }
    Start-Sleep -Milliseconds 200
}
