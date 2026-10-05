# Copies CraftV.asi (and CraftV.ini, if there isn't one yet) into the GTA V folder. Nothing else is touched:
# no game files are moved, replaced or removed. The folder comes from CRAFTV_GTA_DIR or -GameDir; nothing is
# hard-coded.
#   $env:CRAFTV_GTA_DIR = 'D:\Games\Grand Theft Auto V'
#   .\scripts\deploy-gta.ps1
#   .\scripts\deploy-gta.ps1 -GameDir 'D:\Games\Grand Theft Auto V' -OverwriteIni
#   .\scripts\deploy-gta.ps1 -Remove        # take CraftV out again (keeps CraftV.ini and CraftV.log)
param(
    [string]$GameDir = $env:CRAFTV_GTA_DIR,
    [switch]$OverwriteIni,
    [switch]$Remove
)
. "$PSScriptRoot\env.ps1"

if (-not $GameDir) { throw "Set CRAFTV_GTA_DIR (or pass -GameDir) to the folder that contains GTA5.exe." }
if (-not (Test-Path (Join-Path $GameDir 'GTA5.exe'))) { throw "GTA5.exe not found in '$GameDir' (CraftV targets GTA V Legacy)." }

$asiTarget = Join-Path $GameDir 'CraftV.asi'
$iniTarget = Join-Path $GameDir 'CraftV.ini'

if ($Remove) {
    Remove-Item $asiTarget -ErrorAction SilentlyContinue
    Write-Host "Removed CraftV.asi from $GameDir (CraftV.ini and CraftV.log left in place)."
    return
}

$asi = Join-Path $script:CraftVRoot 'build\gta\Release\CraftV.asi'
if (-not (Test-Path $asi)) {
    Write-Host "CraftV.asi not built yet; building..."
    & "$PSScriptRoot\build-native.ps1" -NoTests
}
if (-not (Test-Path $asi)) { throw "CraftV.asi still missing. Is the Script Hook V SDK in sdk\ScriptHookV_SDK?" }

Copy-Item $asi $asiTarget -Force
Write-Host "Copied CraftV.asi -> $asiTarget"
if ($OverwriteIni -or -not (Test-Path $iniTarget)) {
    Copy-Item (Join-Path $script:CraftVRoot 'gta\CraftV.ini') $iniTarget -Force
    Write-Host "Copied CraftV.ini -> $iniTarget"
} else {
    Write-Host "Kept your existing CraftV.ini (use -OverwriteIni to replace it)."
}

# What else the game needs to load an .asi (CraftV never installs these itself).
$hook = Join-Path $GameDir 'ScriptHookV.dll'
$loader = Join-Path $GameDir 'dinput8.dll'
if (-not (Test-Path $hook)) { Write-Warning "ScriptHookV.dll is not in the game folder. Get it from http://www.dev-c.com/gtav/scripthookv/" }
if (-not (Test-Path $loader)) { Write-Warning "dinput8.dll (the ASI loader that comes with Script Hook V) is not in the game folder." }
$exe = Get-Item (Join-Path $GameDir 'GTA5.exe')
Write-Host "GTA5.exe file version: $($exe.VersionInfo.FileVersion) (Script Hook V must support this exact build)"
Write-Host "Story mode only. Never take mods into GTA Online."
