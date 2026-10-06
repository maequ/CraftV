# Assembles dist\CraftV: what Sary copies into the GTA V folder (gta\INSTALL.txt says how). Only CraftV's own
# files and ReShade's CC0 shader headers go in; ReShade itself and Script Hook V come from their official sites.
#   .\scripts\make-dist.ps1
. "$PSScriptRoot\env.ps1"

$root = $script:CraftVRoot
$dist = Join-Path $root 'dist\CraftV'
$asi = Join-Path $root 'build\gta\Release\CraftV.asi'
if (-not (Test-Path $asi)) { throw "build CraftV.asi first: .\scripts\build-native.ps1" }
$fxh = Join-Path $root 'sdk\reshade-shaders-src\Shaders'
if (-not (Test-Path (Join-Path $fxh 'ReShade.fxh'))) { throw "ReShade.fxh missing: clone crosire/reshade-shaders (slim) into sdk\reshade-shaders-src" }

New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item $asi, (Join-Path $root 'gta\CraftV.ini'), (Join-Path $root 'gta\INSTALL.txt') $dist -Force
$view = Join-Path $dist 'Minecraft view (ReShade)'
$shaders = Join-Path $view 'reshade-shaders\Shaders'
New-Item -ItemType Directory -Force $shaders | Out-Null
Copy-Item (Join-Path $root 'gta\reshade\ReShade.ini'), (Join-Path $root 'gta\reshade\ReShadePreset.ini') $view -Force
Copy-Item (Join-Path $root 'gta\shaders\CraftV.fx'), (Join-Path $fxh 'ReShade.fxh'), (Join-Path $fxh 'ReShadeUI.fxh') $shaders -Force
Get-ChildItem $dist -Recurse -File | ForEach-Object { "{0,-60} {1,10:N0} bytes  {2:HH:mm}" -f $_.FullName.Substring($dist.Length + 1), $_.Length, $_.LastWriteTime }
