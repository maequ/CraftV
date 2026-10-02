# Copies RedCraft.asi (and RedCraft.ini, if there isn't one yet) into the RDR2 folder.
# The folder comes from REDCRAFT_RDR2_DIR or -GameDir; nothing is hard-coded.
#   $env:REDCRAFT_RDR2_DIR = 'D:\Games\Red Dead Redemption 2'
#   .\scripts\deploy-rdr2.ps1
#   .\scripts\deploy-rdr2.ps1 -GameDir 'D:\Games\Red Dead Redemption 2' -OverwriteIni
#   .\scripts\deploy-rdr2.ps1 -Remove        # take RedCraft out again (keeps RedCraft.log)
param(
    [string]$GameDir = $env:REDCRAFT_RDR2_DIR,
    [switch]$OverwriteIni,
    [switch]$Remove
)
. "$PSScriptRoot\env.ps1"

if (-not $GameDir) { throw "Set REDCRAFT_RDR2_DIR (or pass -GameDir) to the folder that contains RDR2.exe." }
if (-not (Test-Path (Join-Path $GameDir 'RDR2.exe'))) { throw "RDR2.exe not found in '$GameDir'." }

$asiTarget = Join-Path $GameDir 'RedCraft.asi'
$iniTarget = Join-Path $GameDir 'RedCraft.ini'

if ($Remove) {
    Remove-Item $asiTarget -ErrorAction SilentlyContinue
    Write-Host "Removed RedCraft.asi from $GameDir (RedCraft.ini and RedCraft.log left in place)."
    return
}

$asi = Join-Path $script:RedCraftRoot 'build\rdr2\Release\RedCraft.asi'
if (-not (Test-Path $asi)) {
    Write-Host "RedCraft.asi not built yet; building..."
    & "$PSScriptRoot\build-native.ps1" -NoTests
}
if (-not (Test-Path $asi)) { throw "RedCraft.asi still missing. Is the ScriptHookRDR2 SDK in sdk\ScriptHookRDR2_SDK?" }

Copy-Item $asi $asiTarget -Force
Write-Host "Copied RedCraft.asi -> $asiTarget"
if ($OverwriteIni -or -not (Test-Path $iniTarget)) {
    Copy-Item (Join-Path $script:RedCraftRoot 'rdr2\RedCraft.ini') $iniTarget -Force
    Write-Host "Copied RedCraft.ini -> $iniTarget"
} else {
    Write-Host "Kept your existing RedCraft.ini (use -OverwriteIni to replace it)."
}

# What else the game needs to load an .asi.
$hook = Join-Path $GameDir 'ScriptHookRDR2.dll'
$loader = Join-Path $GameDir 'dinput8.dll'
if (-not (Test-Path $hook)) { Write-Warning "ScriptHookRDR2.dll is not in the game folder. RedCraft won't load without it." }
if (-not (Test-Path $loader)) { Write-Warning "dinput8.dll (the ASI loader) is not in the game folder. RedCraft won't load without it." }
$exe = Get-Item (Join-Path $GameDir 'RDR2.exe')
Write-Host "RDR2.exe file version: $($exe.VersionInfo.FileVersion)"
Write-Host "Story mode only. Never take mods into Red Dead Online."
